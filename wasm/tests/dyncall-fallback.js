/* dynCall fallback helpers for Emscripten WASM bundles
 *
 * Modern Emscripten builds omit legacy dynCall_* shims unless the runtime
 * requests them explicitly. Our headless Puppeteer tests load the generated
 * bundle directly (without the surrounding wasm/index.html scaffolding), so
 * we provide the same compatibility layer here to keep the test pages working.
 */
(function() {
    const signatures = [
        'i','ii','iii','iiii','iiiii','iiiiii','iiiiiii','iiiiiiii','iiiiiiiiiii',
        'iiiiiiiiiiii','iiiiiiiiiiiii','iiiiij','iiiiijj','iiiiid','iiji',
        'diii','fiii','j','ji','jii','jiii','jiiii','v','vi','vii','viii','viiii',
        'viiiii','viiiiii','viiiiiii','viiiiiiiiii','viiiiiiiiiiiiiii','viij',
        'viijj','viijii','viif','viid','viiifi','viiidi'
    ];

    const globalObj = typeof globalThis !== 'undefined'
        ? globalThis
        : (typeof window !== 'undefined' ? window : self);

    globalObj.Module = globalObj.Module || {};

    function resolveTable() {
        if (globalObj.Module && globalObj.Module.wasmTable) {
            return globalObj.Module.wasmTable;
        }
        if (typeof globalObj.wasmTable !== 'undefined') {
            return globalObj.wasmTable;
        }
        return null;
    }

    function makeDynCall(sig) {
        return function(index, ...args) {
            const table = resolveTable();
            if (!table) {
                throw new Error('dynCall_' + sig + ' invoked before wasmTable is ready');
            }
            const fn = table.get(index);
            return fn.apply(null, args);
        };
    }

    signatures.forEach((sig) => {
        const name = 'dynCall_' + sig;
        if (!globalObj[name]) {
            globalObj[name] = makeDynCall(sig);
        }
        if (!globalObj.Module[name]) {
            globalObj.Module[name] = globalObj[name];
        }
    });

    if (!globalObj.Module.dynCall) {
        globalObj.Module.dynCall = function(signature, index, ...args) {
            const table = resolveTable();
            if (!table) {
                throw new Error('dynCall invoked before wasmTable is ready');
            }
            const fn = table.get(index);
            return fn.apply(null, args);
        };
    }
})();
