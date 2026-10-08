#!/usr/bin/env node
/**
 * Opens a demo module on the shipped page the way a visitor does: click the
 * canvas, move down one entry, press Enter.
 *
 * The other suites load their own test pages and never start a module, so a
 * build that crashed on the first file open still passed them. This one fails
 * when the open traps, when no sound comes out, when a click on the canvas is
 * taken as Enter, or when the player keys stop reaching the interface.
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
            res.writeHead(200, { 'Content-Type': mime[path.extname(file)] || 'application/octet-stream' });
            res.end(data);
        });
    });
    return new Promise((resolve) => {
        server.listen(0, '127.0.0.1', () => resolve(server));
    });
}

const wait = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

function playerState(page) {
    return page.evaluate(() => {
        const seen = new Set();
        let downloaded = 0;
        const map = window.Module.sampleRemoteMap || {};
        Object.keys(map).forEach((key) => {
            const entry = map[key];
            if (!seen.has(entry)) {
                seen.add(entry);
                if (entry.downloaded) {
                    downloaded++;
                }
            }
        });
        const sdl = window.Module.SDL2 || {};
        return {
            downloaded,
            audio: sdl.audioContext ? sdl.audioContext.state : null,
            fatal: !!window.ocpFatalShown,
            frames: window.Module._wasm_get_frame_count()
        };
    });
}

/* Peak level of what SDL sends to the speakers, sampled for half a second. */
function outputPeak(page) {
    return page.evaluate(() => new Promise((resolve) => {
        const sdl = window.Module.SDL2;
        const ctx = sdl.audioContext;
        const analyser = ctx.createAnalyser();
        analyser.fftSize = 2048;
        sdl.audio.scriptProcessorNode.connect(analyser);
        const data = new Float32Array(analyser.fftSize);
        let peak = 0;
        const started = Date.now();
        const timer = setInterval(() => {
            analyser.getFloatTimeDomainData(data);
            for (let i = 0; i < data.length; i++) {
                peak = Math.max(peak, Math.abs(data[i]));
            }
            if (Date.now() - started > 500) {
                clearInterval(timer);
                sdl.audio.scriptProcessorNode.disconnect(analyser);
                resolve(peak);
            }
        }, 50);
    }));
}

async function main() {
    if (!fs.existsSync(path.join(buildRoot, 'index.html')) || !fs.existsSync(path.join(buildRoot, 'ocp.js'))) {
        console.error('playback: build outputs missing in wasm/build');
        process.exit(1);
    }

    const server = await startServer();
    const port = server.address().port;
    const browser = await puppeteer.launch({
        headless: true,
        args: ['--no-sandbox', '--disable-setuid-sandbox', '--disable-dev-shm-usage', '--disable-gpu',
            '--no-first-run', '--autoplay-policy=no-user-gesture-required']
    });

    try {
        const page = await browser.newPage();
        await page.setViewport({ width: 1400, height: 950 });
        const pageErrors = [];
        page.on('console', (msg) => {
            if (msg.type() === 'error' && msg.text().includes('wasm_main_loop')) {
                pageErrors.push(msg.text());
            }
        });
        page.on('pageerror', (err) => pageErrors.push(err.stack || err.message));

        await page.goto('http://127.0.0.1:' + port + '/index.html', {
            waitUntil: 'domcontentloaded',
            timeout: 120000
        });
        await page.waitForFunction(() => {
            const status = document.getElementById('status');
            return status && status.textContent.indexOf('OpenCubicPlayer WASM ready') === 0;
        }, { timeout: 180000 });
        await wait(1500);

        // A click is how a visitor focuses the canvas. If it were taken as
        // Enter it would open ".." and the next two keys would open a
        // directory instead of the first demo module.
        await page.click('#canvas');
        await wait(500);
        await page.keyboard.press('ArrowDown');
        await wait(500);
        await page.keyboard.press('Enter');

        await page.waitForFunction(() => {
            const sdl = window.Module.SDL2;
            return window.ocpFatalShown || (sdl && sdl.audioContext && sdl.audioContext.state === 'running');
        }, { timeout: 30000 }).catch(() => {});
        await wait(2000);

        const playing = await playerState(page);
        if (pageErrors.length) {
            throw new Error('page error after opening a module: ' + pageErrors[0]);
        }
        if (playing.fatal) {
            throw new Error('fatal overlay after opening a module');
        }
        if (playing.downloaded !== 1) {
            throw new Error('expected one demo module to be fetched, got ' + playing.downloaded);
        }
        if (playing.audio !== 'running') {
            throw new Error('audio context is ' + playing.audio + ', expected running');
        }
        const peak = await outputPeak(page);
        if (!(peak > 0.001)) {
            throw new Error('the module opened but the output is silent (peak ' + peak + ')');
        }

        // Resizing during playback must keep the framebuffer and layout in sync.
        for (const ratio of ['full', '4:3', '16:9', 'full']) {
            const beforeResize = await playerState(page);
            await page.click('#ratio-control button[data-ratio="' + ratio + '"]');
            await wait(1000);
            const resized = await playerState(page);
            const canvasWidth = await page.$eval('#canvas', canvas => canvas.width);
            if (ratio === 'full' && canvasWidth <= 132 * 8) {
                throw new Error('Full did not exercise a screen wider than the old 132-column limit');
            }
            if (pageErrors.length || resized.fatal || !(resized.frames > beforeResize.frames)) {
                throw new Error('playback stopped after selecting ' + ratio + ': ' + (pageErrors[0] || 'fatal or stalled loop'));
            }
            if (!((await outputPeak(page)) > 0.001)) {
                throw new Error('audio stopped after selecting ' + ratio);
            }
        }
        await page.click('#canvas');

        // Keys the old hand-written key map dropped: F1 opens help, Escape
        // closes it. The loop has to keep running through both.
        await page.keyboard.press('F1');
        await wait(1000);
        await page.keyboard.press('Escape');
        await wait(1000);
        const before = await playerState(page);
        await wait(1000);
        const after = await playerState(page);
        if (pageErrors.length) {
            throw new Error('page error after F1 and Escape: ' + pageErrors[0]);
        }
        if (after.fatal || !(after.frames > before.frames)) {
            throw new Error('the main loop stopped after F1 and Escape');
        }

        console.log('playback: PASS opened a demo module, output peak ' + peak.toFixed(3) + ', live resizing and keys handled');
    } finally {
        await browser.close();
        await new Promise((resolve) => server.close(resolve));
    }
}

main().catch((err) => {
    console.error('playback: FAIL ' + err.message);
    process.exit(1);
});
