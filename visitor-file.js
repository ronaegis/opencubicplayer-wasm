/* Store a visitor-chosen file on the Emscripten filesystem that
 * filesel/filesystem-unix.c reads with opendir/readdir. GPL-2.0-only.
 */
(function (root) {
    'use strict';

    function sanitizeVisitorFilename(name) {
        var base = String(name || 'tune.bin').split(/[/\\]/).pop();
        base = base.replace(/[^A-Za-z0-9._ ()-]/g, '_');
        if (!base || base === '.' || base === '..') {
            base = 'tune.bin';
        }
        if (base.length > 180) {
            base = base.slice(0, 180);
        }
        return base;
    }

    function ensureVisitorDir(fs, path) {
        if (fs.analyzePath(path).exists) {
            return;
        }
        if (typeof fs.mkdirTree === 'function') {
            fs.mkdirTree(path);
            return;
        }
        var segments = path.split('/').filter(Boolean);
        var current = '';
        var i;
        for (i = 0; i < segments.length; i++) {
            current += '/' + segments[i];
            if (!fs.analyzePath(current).exists) {
                fs.mkdir(current);
            }
        }
    }

    function ocpStoreVisitorFile(fs, filename, bytes) {
        if (!fs || typeof fs.writeFile !== 'function' || typeof fs.readdir !== 'function') {
            throw new Error('ocpStoreVisitorFile: filesystem is missing writeFile or readdir');
        }
        var data = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes);
        var safe = sanitizeVisitorFilename(filename);
        ensureVisitorDir(fs, '/music');
        var path = '/music/' + safe;
        if (fs.analyzePath(path).exists) {
            fs.unlink(path);
        }
        fs.writeFile(path, data);
        return path;
    }

    root.ocpStoreVisitorFile = ocpStoreVisitorFile;
    root.sanitizeVisitorFilename = sanitizeVisitorFilename;
})(typeof globalThis !== 'undefined' ? globalThis : this);
