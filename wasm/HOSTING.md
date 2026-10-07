# Hosting the WASM player

This page is a set of static files. The host serves them. It does not run a player process, accept uploads, or keep an `ocp.ini`. A visitor’s module stays in their browser.

The live demo is https://ronaegis.github.io/opencubicplayer-wasm/.

Build with Emscripten 5.0.2, then publish the site files from `wasm/build/`. `wasm/README.md` covers the build command. This file covers what to upload, how a web server has to serve it, and how to choose plugins.

## Build

From the repository root:

```bash
git submodule update --init --recursive
cd wasm
./build.sh                 # production, the default
```

`emcmake`, `emcc`, and `emmake` must be on `PATH`. The script also needs CMake and Python 3. The playsid side module needs Perl and the xa 6502 assembler (`xa65` on Debian and Ubuntu).

Submodules the browser build compiles: `playsid/libsidplayfp-git`, `playsid/libresidfp-git`, `playopl/adplug-git`, `playopl/libbinio-git`, `playopl/adplugdb-git`, and `playtimidity/timidity-git`.

`ocp.hlp` at the repository root and `wasm/unifont-16.0.04.ttf` are packed into `ocp.data` on a production build.

| `./build.sh` flag | Compiler | Help (`ocp.hlp`) | AdPlug database | Demo modules |
| --- | --- | --- | --- | --- |
| `--production` (default) | `-O3`, assertions off | packed into `ocp.data` | packed into `ocp.data` | copied to `sample-files/` |
| `--debug` | `-O1`, `-g`, assertions on | packed into `ocp.data` | packed into `ocp.data` | copied to `sample-files/` |
| `--minimal` | `-O3`, assertions off | left out | left out | left out |

Those switches are `WASM_PRODUCTION_BUILD`, `WASM_INCLUDE_HELP`, `WASM_INCLUDE_ADPLUG_DB`, and `WASM_INCLUDE_SAMPLES`. Sample files are copied by `build.sh`. The CMake option only records that the copy was requested. `OCP_WASM_DEBUG_LOGGING` is off unless you pass `-DOCP_WASM_DEBUG_LOGGING=ON` to CMake yourself.

`ocp.data` is the font, and on a production build the help database and `adplug.db`. It does not contain the demo modules.

## Files to upload

Upload the site files, which sit next to each other. Leave `CMakeFiles/`, object files, and the generated Makefile on the build machine. `wasm/build/` holds both.

A production build of this tree is about 22 MB, of which `ocp.data` is about 12 MB and `ocp.wasm` is about 3.4 MB.

| Upload | Role |
| --- | --- |
| `index.html` | Page, canvas, and plugin list |
| `visitor-file.js` | Writes a file the visitor picked into the in-browser `/music` directory |
| `ocp.js` | Emscripten loader |
| `ocp.wasm` | Player core. Must be from the same build as every side module |
| `ocp.data` | Unifont, help, AdPlug database |
| `*.wasm` beside `ocp.js` | One side module per plugin |
| `COPYING` | GPL-2 |
| `UNIFONT-LICENSE.txt` | Font terms |
| `sample-files/manifest.json` | Titles and sizes for the demo modules |
| `sample-files/*.{669,ahx,dmf,it,mod,s3m,stm,xm}` | The nine demo modules. See `sample-files/LICENSE.md` |

`build.sh` writes `manifest.json` and does not copy `LICENSE.md` or `metadata.json` into the staged directory. Credits for the nine modules belong on the site because those files are redistributed unmodified under the Mod Archive Distribution license. The root `README.md` and `sample-files/LICENSE.md` are that credit.

Check the directory on the build machine before you upload it:

```bash
cd wasm/build
python3 -m http.server 8080
```

Open `http://127.0.0.1:8080/`. Opening `index.html` as a `file://` URL does not start the module.

## Web server

The public URL is the directory that contains `index.html`. A subdirectory works (`https://example.com/player/`) when every URL in the page is still relative to that directory. The page loads `ocp.js`, `ocp.wasm`, each plugin, and `sample-files/` with relative URLs.

Set the index file to `index.html`. Leave a missing file as a real 404. Rewriting every missing path to `index.html` turns a missing plugin into an HTML body, and the loader reports a failed plugin fetch.

Serve `.wasm` as `application/wasm`. Emscripten compiles `ocp.wasm` with `instantiateStreaming`, which requires that type. A wrong type is logged and the loader falls back to an array buffer, so the page can still start. Plugin files are fetched as bytes and written into the in-browser filesystem; their type is not checked by the streaming compiler.

Let the host compress responses on the fly and set `Content-Encoding`. The content type stays `application/wasm`.

Keep the page, `ocp.js`, `ocp.wasm`, `ocp.data`, the plugin files, `visitor-file.js`, and `sample-files/` on one origin. The fetches are same-origin.

These headers are not used: `Cross-Origin-Opener-Policy`, `Cross-Origin-Embedder-Policy`, and anything for `SharedArrayBuffer`. Audio goes through Emscripten SDL2 and `ScriptProcessorNode`. There is no service worker.

`index.html` should be revalidated. The binaries are requested with a query string, so they can be cached for a long time:

```text
index.html                         Cache-Control: no-cache
ocp.js, ocp.wasm, ocp.data, *.wasm Cache-Control: public, max-age=31536000
```

A CDN that ignores query strings will keep an old binary until the filename changes.

### nginx

```nginx
server {
    listen 443 ssl;
    server_name player.example;
    root /var/www/ocp;
    index index.html;

    location ~* \.wasm$ {
        default_type application/wasm;
        add_header Cache-Control "public, max-age=31536000";
    }

    location = /index.html {
        add_header Cache-Control "no-cache";
    }
}
```

### Apache

```apache
AddType application/wasm .wasm

<Files "index.html">
    Header set Cache-Control "no-cache"
</Files>
```

`mod_headers` has to be enabled for `Header set`.

### Caddy

```caddy
player.example {
    root * /var/www/ocp
    file_server
    header *.wasm Content-Type application/wasm
    header *.wasm Cache-Control "public, max-age=31536000"
    header /index.html Cache-Control "no-cache"
}
```

### GitHub Pages

Publish the site files as the root of the Pages branch (`gh-pages` for a project site). The live demo is that layout. Pages serves HTTPS and compresses responses. It caches for several minutes, so a new upload shows up after that cache expires and after the `?v=` token below changes.

### Object storage

Set the content type of every `.wasm` object to `application/wasm` at upload time. Point the site at the directory that contains `index.html`.

## What the browser loads

1. `index.html` loads `ocp.js?v=10`.
2. `locateFile` in that page loads `ocp.wasm` and `ocp.data`, and appends `?v=10`.
3. The page fetches every `url` in `wasmPluginManifest`, appends `?v=10`, and writes the bytes to `/program/autoload/<name>.wasm` in the in-browser filesystem.
4. `wasm_start_ocp` loads every side module in that directory.
5. The page fetches `sample-files/manifest.json` with `cache: 'no-cache'`. A missing manifest is a console warning. The player still starts, and the demo songs are absent. "Open a module from this computer" still works.

One plugin response that is not OK stops startup. The status line is `Failed to load plugins`. A side module from a different build than `ocp.wasm` can fail inside `dlopen`, and `wasm_start_ocp` then returns `-1` with the same status line. Upload `ocp.wasm` and the side modules from one `./build.sh` run.

`sample-files/manifest.json` is the list the file selector shows before a demo file is downloaded. Opening an entry fetches `sample-files/<filename>`.

## Choosing plugins

Two lists select plugins. They have to name the same set.

- `wasm/CMakeLists.txt` compiles a side module for each `include(...-plugin.cmake)` line. There is no per-plugin `-D` switch.
- `wasmPluginManifest` in `wasm/index.html` is what the browser fetches. `build.sh` copies `index.html` into `wasm/build/`, so a later build replaces an edit made only in the build directory.

The `formats` array on each manifest entry is a label. The loader reads `name` and `url`. The file selector learns extensions from the plugin after it loads (`fsRegisterExt` in that format’s type source).

To drop a format from a hosted site:

1. Remove its object from `wasmPluginManifest`.
2. Leave its `.wasm` out of the upload. A file that is not listed is never fetched.
3. Comment out its `include()` in `wasm/CMakeLists.txt` and run `./build.sh` again when you also want it out of the build.

A manifest entry whose file is missing makes the page refuse to start. An empty manifest still starts the player, with no audio device and no format loaders.

### Devices

`wasm/wasm-original-main.c` writes this on every start:

```text
[sound]
playerdevices=devpSDL2 devpNone
wavetabledevices=devwMix devwMixF devwNone
sdl_buffer_ms=50
```

The names are case-sensitive. The first loaded driver that detects hardware is the one that plays. A name with no loaded plugin is logged as `driver not found`, and the next name is tried.

| File | Driver name | Keep it |
| --- | --- | --- |
| `devpsdl2.wasm` | `devpSDL2` | Yes, if the site should make sound. This is the WebAudio output. |
| `devpnone.wasm` | `devpNone` | Yes. It is the silent fallback in the list above. |
| `devwmix.wasm` | `devwMix` | Yes for module playback. It is tried before the float mixer. |
| `devwmixf.wasm` | `devwMixF` | Optional. Used when `devwMix` is not loaded. Higher-quality mixer. |
| `devwnone.wasm` | `devwNone` | Yes. Silent wavetable fallback. |

`freverb.wasm` and `ireverb.wasm` are the reverb effects. The player starts without them.

Saved settings from the in-player setup go to `/home/web_user/.ocp` in that browser (IndexedDB). The two device lines above are written again on the next load. There is no server-side `ocp.ini`. Closing the tab can drop the last write.

### Format plugins

| File | Extensions the plugin registers |
| --- | --- |
| `playxm.wasm` | `NST`, `MOD`, `MXM`, `WOW`, `XM` |
| `playgmd.wasm` | `669`, `AMS`, `DMF`, `MDL`, `MTM`, `OKT`, `OKTA`, `PTM`, `S3M`, `STM`, `ULT` |
| `playit.wasm` | `IT` |
| `playhvl.wasm` | `HVL`, `AHX` |
| `playay.wasm` | `AY`, `EMUL` |
| `playym.wasm` | `YM`, `MIX` |
| `playsid.wasm` | `MUS`, `SID`, `RSID`, `PSID` |
| `playgme.wasm` | `AY`, `GBS`, `GYM`, `HES`, `KSS`, `NSF`, `NSFE`, `SAP`, `SPC`, `VGM`, `VGZ` |
| `playopl.wasm` | The extensions reported by the compiled AdPlug players, plus numeric names `0` through `99` |
| `playtimidity.wasm` | `MID`, `MIDI`, `RMI`, `KAR` |
| `playflac.wasm` | `FLA`, `FLAC`, `FLC`, `OGA` |
| `playogg.wasm` | `OGA`, `OGG` |
| `playwav.wasm` | `WAV`, `WAVE` |
| `playmp2.wasm` | `MP1`, `MP2`, `MP3` |
| `modland.wasm` | No extension. Virtual Modland directory. The browser contacts Modland when that directory is opened. |

`S3M` and the other `playgmd.wasm` extensions stay available while that file is in the manifest. The label on the `playxm` entry also mentions some of those names; the loader does not use that label.

`playay.wasm` and `playgme.wasm` both register `AY`. `playflac.wasm` and `playogg.wasm` both register `OGA`. Identification uses the file contents.

`playopl.wasm` plays without `adplug.db`. The database is song metadata. `--minimal` leaves it out of `ocp.data`.

`playtimidity.wasm` does not include a `timidity.cfg` or instrument patches. A `timidity.cfg` placed next to `index.html` is not copied into the player. MIDI needs that config inside the in-browser home directory (`/home/web_user/timidity.cfg`, `_timidity.cfg`, or `.timidity.cfg`), which this page does not preload.

`modland.wasm` is optional. While it is listed, the file has to be on the host. The player still starts if Modland itself is unreachable.

Audio CD, MusicBrainz lookups, gzip, tar, bzip2, and pak archives, the cube visualiser, and TGA pictures are not part of this browser build. Zip is compiled. Bzip2 members inside a zip are not.

## Replacing a published build

1. Run `./wasm/build.sh` with the same Emscripten version.
2. In `wasm/index.html`, raise the same token in all three places: `fetch(plugin.url + '?v=10')`, the `?v=10` appended by `locateFile` for `.wasm` and `.data`, and `<script src="ocp.js?v=10">`.
3. Upload `index.html` together with `ocp.js`, `ocp.wasm`, `ocp.data`, and every side module from that build.
4. In the browser network panel, `ocp.wasm` and each listed plugin return 200. The status line reaches ready. Open one file from the selector, and open one demo entry when `sample-files/` is part of the upload.

## License

The player is GPL-2 only. Ship `COPYING` and `UNIFONT-LICENSE.txt` with the page. The demo modules have their own terms in `sample-files/LICENSE.md`. Browser-port bugs belong at https://github.com/ronaegis/opencubicplayer-wasm/issues.
