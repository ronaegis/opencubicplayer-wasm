#!/usr/bin/env node
/**
 * Loads the shipped page and stores real file bytes with ocpStoreVisitorFile,
 * the function index.html calls. The listing is Module.FS.readdir('/music'),
 * the same Emscripten filesystem filesel/filesystem-unix.c opens with opendir.
 */

const http = require('http');
const fs = require('fs');
const path = require('path');
const puppeteer = require('puppeteer');

const buildRoot = path.resolve(__dirname, '../build');
const mime = {
    '.html': 'text/html; charset=utf-8',
    '.js': 'text/javascript; charset=utf-8',
    '.wasm': 'application/wasm',
    '.data': 'application/octet-stream',
    '.json': 'application/json',
    '.txt': 'text/plain; charset=utf-8',
    '.md': 'text/plain; charset=utf-8'
};

function startServer() {
    const server = http.createServer((req, res) => {
        const urlPath = decodeURIComponent(req.url.split('?')[0]);
        const rel = urlPath === '/' ? 'index.html' : urlPath.replace(/^\/+/, '');
        const file = path.normalize(path.join(buildRoot, rel));
        if (!file.startsWith(buildRoot)) {
            res.writeHead(403);
            res.end('forbidden');
            return;
        }
        fs.readFile(file, (err, data) => {
            if (err) {
                res.writeHead(404);
                res.end('missing ' + rel);
                return;
            }
            const type = mime[path.extname(file)] || 'application/octet-stream';
            res.writeHead(200, { 'Content-Type': type, 'Cross-Origin-Resource-Policy': 'cross-origin' });
            res.end(data);
        });
    });
    return new Promise((resolve) => {
        server.listen(0, '127.0.0.1', () => resolve(server));
    });
}

function silentModBytes() {
    const data = Buffer.alloc(2108);
    const title = Buffer.from('OCP visitor probe');
    title.copy(data, 0);
    data[950] = 1;
    data[951] = 127;
    data.write('M.K.', 1080, 'ascii');
    return data;
}

async function main() {
    if (!fs.existsSync(path.join(buildRoot, 'index.html')) || !fs.existsSync(path.join(buildRoot, 'ocp.js'))) {
        console.error('visitor-file: build outputs missing in wasm/build (run ./wasm/build.sh first)');
        process.exit(1);
    }
    if (!fs.existsSync(path.join(buildRoot, 'visitor-file.js'))) {
        console.error('visitor-file: wasm/build/visitor-file.js missing');
        process.exit(1);
    }

    const bytes = silentModBytes();
    const server = await startServer();
    const port = server.address().port;
    const browser = await puppeteer.launch({
        headless: true,
        args: [
            '--no-sandbox',
            '--disable-setuid-sandbox',
            '--disable-dev-shm-usage',
            '--disable-gpu',
            '--no-first-run'
        ]
    });

    try {
        const page = await browser.newPage();
        page.on('pageerror', (err) => {
            console.log('visitor-file pageerror: ' + err.message);
        });
        await page.goto('http://127.0.0.1:' + port + '/index.html', {
            waitUntil: 'domcontentloaded',
            timeout: 120000
        });
        await page.waitForFunction(() => {
            return typeof ocpStoreVisitorFile === 'function' &&
                window.Module && window.Module.calledRun && window.Module.FS &&
                typeof window.Module.FS.writeFile === 'function' &&
                typeof window.Module.FS.readdir === 'function';
        }, { timeout: 180000 });

        const report = await page.evaluate((payload) => {
            const fs = window.Module.FS;
            const data = new Uint8Array(payload);
            const stored = ocpStoreVisitorFile(fs, 'visitor-probe.mod', data);
            const listed = fs.readdir('/music');
            const readBack = fs.readFile(stored);
            let same = readBack.length === data.length;
            if (same) {
                for (let i = 0; i < data.length; i++) {
                    if (readBack[i] !== data[i]) {
                        same = false;
                        break;
                    }
                }
            }
            if (typeof window.Module._wasm_filesel_rescan === 'function') {
                window.Module._wasm_filesel_rescan();
            }
            return {
                stored: stored,
                listed: listed,
                same: same,
                size: readBack.length
            };
        }, Array.from(bytes));

        const name = 'visitor-probe.mod';
        if (!report.listed || report.listed.indexOf(name) < 0) {
            throw new Error('selector filesystem did not list ' + name + ': ' + JSON.stringify(report.listed));
        }
        if (!report.same || report.size !== bytes.length) {
            throw new Error('bytes read back from ' + report.stored + ' did not match (' + report.size + ')');
        }
        console.log('visitor-file: PASS listed ' + name + ' on /music (' + report.size + ' bytes) at ' + report.stored);
    } finally {
        await browser.close();
        await new Promise((resolve) => server.close(resolve));
    }
}

main().catch((err) => {
    console.error('visitor-file: FAIL ' + err.message);
    process.exit(1);
});
