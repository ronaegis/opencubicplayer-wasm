#!/usr/bin/env node
/**
 * The shipped page must read ocp.ini after IndexedDB populate and before
 * playback starts. The old stub config init never read the file back, so a
 * saved setting was lost on every reload.
 *
 * A file that merely survives a reload proves nothing about the player. This
 * test saves a different player device order and checks that the player
 * picks the device the saved file names first.
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
    '.txt': 'text/plain; charset=utf-8'
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

async function waitReady(page) {
    await page.waitForFunction(() => {
        const status = document.getElementById('status');
        return status && status.textContent.indexOf('OpenCubicPlayer WASM ready') === 0;
    }, { timeout: 180000 });
    const fatal = await page.evaluate(() => {
        const error = document.getElementById('error');
        return {
            shown: !!window.ocpFatalShown,
            text: error ? error.textContent : ''
        };
    });
    if (fatal.shown) {
        throw new Error('fatal overlay: ' + fatal.text);
    }
}

async function main() {
    if (!fs.existsSync(path.join(buildRoot, 'index.html')) || !fs.existsSync(path.join(buildRoot, 'ocp.js'))) {
        console.error('config-roundtrip: build outputs missing in wasm/build');
        process.exit(1);
    }

    const server = await startServer();
    const port = server.address().port;
    const browser = await puppeteer.launch({
        headless: true,
        args: ['--no-sandbox', '--disable-setuid-sandbox', '--disable-dev-shm-usage', '--disable-gpu', '--no-first-run']
    });

    try {
        const page = await browser.newPage();
        let stderrLines = [];
        page.on('pageerror', (err) => {
            console.log('config-roundtrip pageerror: ' + err.message);
        });
        page.on('console', (msg) => {
            if (msg.type() === 'error') {
                stderrLines.push(msg.text());
            }
        });
        const detected = (name) => stderrLines.some((line) => line.indexOf(name) !== -1 && /\(detected\)/.test(line));
        await page.goto('http://127.0.0.1:' + port + '/index.html', {
            waitUntil: 'domcontentloaded',
            timeout: 120000
        });
        await waitReady(page);
        if (!detected('devpSDL2')) {
            throw new Error('first load did not pick devpSDL2 from the default ocp.ini');
        }

        await page.evaluate(() => new Promise((resolve, reject) => {
            const fsApi = window.Module.FS;
            const iniPath = '/home/web_user/.ocp/ocp.ini';
            let text;
            try {
                text = new TextDecoder().decode(fsApi.readFile(iniPath));
            } catch (err) {
                reject(new Error('ocp.ini missing after startup: ' + (err && err.message ? err.message : err)));
                return;
            }
            if (!/playerdevices=devpSDL2/.test(text)) {
                reject(new Error('ocp.ini is missing the browser player device'));
                return;
            }
            // Both names stay in the list, so the browser defaults leave it alone.
            text = text.replace(/playerdevices=.*/, 'playerdevices=devpNone devpSDL2');
            if (!/ocp_roundtrip_probe=1/.test(text)) {
                text += (text.endsWith('\n') ? '' : '\n') + 'ocp_roundtrip_probe=1\n';
            }
            fsApi.writeFile(iniPath, text);
            fsApi.syncfs(false, (err) => {
                if (err) {
                    reject(new Error('IndexedDB sync failed: ' + err));
                    return;
                }
                resolve();
            });
        }));

        stderrLines = [];
        await page.reload({ waitUntil: 'domcontentloaded', timeout: 120000 });
        await waitReady(page);
        if (!detected('devpNone') || detected('devpSDL2')) {
            throw new Error('the player ignored the saved playerdevices order after reload');
        }

        const kept = await page.evaluate(() => {
            const text = new TextDecoder().decode(window.Module.FS.readFile('/home/web_user/.ocp/ocp.ini'));
            return {
                probe: /ocp_roundtrip_probe=1/.test(text),
                order: /playerdevices=devpNone devpSDL2/.test(text)
            };
        });
        if (!kept.order || !kept.probe) {
            throw new Error('ocp.ini did not survive reload: ' + JSON.stringify(kept));
        }
        console.log('config-roundtrip: PASS the player used the saved device order after reload');
    } finally {
        await browser.close();
        await new Promise((resolve) => server.close(resolve));
    }
}

main().catch((err) => {
    console.error('config-roundtrip: FAIL ' + err.message);
    process.exit(1);
});
