# OpenCubicPlayer WASM

Experimental browser port of [OpenCubicPlayer](https://github.com/mywave82/opencubicplayer), version 3.5.0+wasm.0.1.0. The desktop cpiface UI is drawn into a 1280×1024 canvas. This page is not the upstream release and not the Homebrew `ocp` package.

Bug reports for the browser port belong on https://github.com/ronaegis/opencubicplayer-wasm/issues. Desktop bugs that reproduce on Stian's tree belong at the upstream repository.

Live demo: https://ronaegis.github.io/opencubicplayer-wasm/

## Build

Emscripten 5.0.2 (`emcc` on `PATH`) and CMake. The playsid side module also needs the `xa` 6502 assembler (`xa65` on Debian and Ubuntu) and Perl.

```bash
cd wasm
./build.sh              # production, the default
./build.sh --debug
./build.sh --minimal
./build.sh --clean      # delete build/ and .emcache, then exit
```

Production flags from `CMakeLists.txt`: `-O3`, assertions off, `INITIAL_MEMORY` 16 MB, `ALLOW_MEMORY_GROWTH` on, no memory maximum. Help is included. `sample-files/` is copied beside the page and contains the nine demo modules credited in the root README and in `sample-files/LICENSE.md`.

The script writes `build/ocp.js`, `build/ocp.wasm`, `build/ocp.data`, one `.wasm` side module per plugin, `index.html`, `visitor-file.js`, `COPYING`, and `UNIFONT-LICENSE.txt`.

`ocp.data` is the GNU Unifont file, the default `ocp.ini`, the help database, `adplug.db`, and the prebuilt Modland catalog. It does not contain sample music.

Serve the build directory over HTTP. Opening the HTML from `file://` will not start the module.

```bash
cd wasm/build
python3 -m http.server 8080
```

`python3 -m http.server` does not set a WASM MIME type. Some hosts need `application/wasm` for `.wasm`. SharedArrayBuffer and COOP/COEP headers are not required.

What to upload, how to serve it, and how to choose plugins: [HOSTING.md](HOSTING.md).

## What the page does

- Every side module listed in `index.html` (`wasmPluginManifest`) is fetched at startup, before the first frame.
- Audio is Emscripten SDL2, which uses `ScriptProcessorNode`. There is no AudioWorklet processor in this tree.
- "Open a module from this computer", or a drop on that box, calls `ocpStoreVisitorFile` in `visitor-file.js`. That writes the bytes under `/music` on the Emscripten filesystem. `filesel/filesystem-unix.c` lists that directory with `opendir`. Open the file from the on-canvas selector.
- F during playback returns to the selector. In this browser build, Esc returns to the selector. Other keys are the desktop player's keys inside the canvas. Press F1 there for the in-player list.
- IDBFS is mounted at `/home/web_user/.ocp`. The player reads `ocp.ini` from there on every load, so settings changed in the in-player setup persist. The page starts a save when it is hidden or closed. The save is asynchronous, so a browser that closes the tab first can still drop it.
- A click on the canvas only gives it keyboard focus. The desktop player's click-to-open and right-click fullscreen are off in this build.
- There is no service worker.

Not in this browser build: Audio CD, MusicBrainz lookups, gzip, tar, bzip2, pak, the cube visualiser, and TGA pictures. Zip is compiled. Bzip2 members inside zip are not.

## Tests

`npm test` runs five things against `wasm/build`:

- `test-runner.js` drives `test-canvas-rendering.html` and `test-sound-output.html` with Puppeteer. These are test pages, not `index.html`.
- `test-visitor-file.js` loads the shipped `index.html` and checks that a visitor file lands in `/music`.
- `test-config-roundtrip.js` saves a different player device order into `ocp.ini`, reloads, and checks that the player used it.
- `test-playback.js` opens a demo module on the shipped page with a click and the keyboard, and checks for sound, no page errors, and working F1 and Escape.
- Jest checks the wrapped cpiface symbols in the compiled object, the page contract, and the dynCall list.

```bash
cd wasm/tests
npm install
npm test
```

Puppeteer Total Tests on this commit: 42.

The count above is filled from the `Total Tests:` line printed by `npm test` for this commit.

## License

GNU GPL version 2 only. See the root `COPYING`. The font is `unifont-16.0.04.ttf`; its terms are `UNIFONT-LICENSE.txt`. Upstream patches are listed in `PATCHES.md`.
