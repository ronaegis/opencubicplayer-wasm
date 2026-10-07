#!/usr/bin/env bash

# OpenCubicPlayer WASM Build Script
# Builds the WASM bundle using the Emscripten toolchain.

set -euo pipefail

usage() {
    cat <<'EOT'
Usage: ./build.sh [options]

Options:
  -c, --clean        Remove the build directory and cached artifacts, then exit.
  -h, --help         Show this help message and exit.

Build Configurations:
  --production       Production build optimized for deployment (default)
                     - Debug symbols disabled
                     - Assertions disabled
                     - Optimization: -O3
                     - Stages sample-files/ (nine Mod Archive modules; see sample-files/LICENSE.md)
                     - Builds CPMDLAND.DAT from Modland's allmods.zip and packs it into ocp.data
                     - Includes help and metadata
                     - Estimated size: ~10-15 MB (gzipped)

  --debug            Development build with debug symbols and all assets
                     - Debug symbols enabled (-g)
                     - Assertions enabled (level 2)
                     - Optimization: -O1
                     - Includes sample files, help, and metadata

  --minimal          Minimal production build (smallest bundle)
                     - Same as --production
                     - Excludes help database
                     - Excludes AdPlug metadata
                     - Estimated size: ~10-14 MB (gzipped)

Notes:
  - If no build configuration is specified, --production is used by default.
  - Build configurations can be combined with --clean.
EOT
}

if [[ ${1:-} == "-h" || ${1:-} == "--help" ]]; then
    usage
    exit 0
fi

# Parse command line arguments
CLEAN_ONLY=0
BUILD_CONFIG="production"  # Default configuration
INCLUDE_SAMPLES=0

for arg in "$@"; do
    case "$arg" in
        -c|--clean)
            CLEAN_ONLY=1
            ;;
        --debug)
            BUILD_CONFIG="debug"
            ;;
        --production)
            BUILD_CONFIG="production"
            ;;
        --minimal)
            BUILD_CONFIG="minimal"
            ;;
        *)
            echo "Error: Unknown option: $arg" >&2
            echo "Run './build.sh --help' for usage information." >&2
            exit 1
            ;;
    esac
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}"

declare -r EM_CACHE="${SCRIPT_DIR}/.emcache"
declare -r BUILD_DIR="${SCRIPT_DIR}/build"

if [[ ${CLEAN_ONLY} -eq 1 ]]; then
    echo "Cleaning WASM build artifacts..."
    rm -rf "${BUILD_DIR}" "${EM_CACHE}"
    if [[ -d "${SCRIPT_DIR}/tests" ]]; then
        rm -f "${SCRIPT_DIR}/tests/build"
    fi
    exit 0
fi

# Set CMake flags based on build configuration
CMAKE_FLAGS=()
case "$BUILD_CONFIG" in
    debug)
        echo "Building OpenCubicPlayer WASM (debug configuration)..."
        echo "  - Debug symbols: enabled"
        echo "  - Optimization: -O1"
        echo "  - Sample files: staged for lazy loading"
        echo "  - Help database: included"
        echo "  - AdPlug database: included"
        # Default CMakeLists.txt settings are already debug-friendly
        CMAKE_FLAGS+=(
            "-DWASM_PRODUCTION_BUILD=OFF"
            "-DWASM_INCLUDE_SAMPLES=ON"
            "-DWASM_INCLUDE_HELP=ON"
            "-DWASM_INCLUDE_ADPLUG_DB=ON"
        )
        INCLUDE_SAMPLES=1
        ;;
    production)
        echo "Building OpenCubicPlayer WASM (production configuration)..."
        echo "  - Debug symbols: disabled"
        echo "  - Optimization: -O3"
        echo "  - Sample files: staged for lazy loading"
        echo "  - Help database: included"
        echo "  - AdPlug database: included"
        CMAKE_FLAGS+=(
            "-DWASM_PRODUCTION_BUILD=ON"
            "-DWASM_INCLUDE_SAMPLES=ON"
            "-DWASM_INCLUDE_HELP=ON"
            "-DWASM_INCLUDE_ADPLUG_DB=ON"
        )
        INCLUDE_SAMPLES=1
        ;;
    minimal)
        echo "Building OpenCubicPlayer WASM (minimal configuration)..."
        echo "  - Debug symbols: disabled"
        echo "  - Optimization: -O3"
        echo "  - Sample files: excluded"
        echo "  - Help database: excluded"
        echo "  - AdPlug database: excluded"
        CMAKE_FLAGS+=(
            "-DWASM_PRODUCTION_BUILD=ON"
            "-DWASM_INCLUDE_SAMPLES=OFF"
            "-DWASM_INCLUDE_HELP=OFF"
            "-DWASM_INCLUDE_ADPLUG_DB=OFF"
        )
        ;;
esac

export EM_CACHE
mkdir -p "${EM_CACHE}"

if ! command -v emcmake >/dev/null 2>&1; then
    echo "Error: emcmake not found. Activate the Emscripten SDK (source emsdk_env.sh)." >&2
    exit 1
fi

if ! command -v emmake >/dev/null 2>&1; then
    echo "Error: emmake not found. Activate the Emscripten SDK (source emsdk_env.sh)." >&2
    exit 1
fi

echo ""
echo "Building the Modland catalog..."
MODLAND_ZIP="${BUILD_DIR}/.allmods.zip"
MODLAND_TXT="${BUILD_DIR}/.allmods.txt"
MODLAND_DAT_DIR="${BUILD_DIR}/.modland-dat"
if curl -fsSL -A "OpenCubicPlayer" -o "${MODLAND_ZIP}.partial" "https://modland.com/allmods.zip"; then
    mv -f "${MODLAND_ZIP}.partial" "${MODLAND_ZIP}"
else
    rm -f "${MODLAND_ZIP}.partial"
    echo "Warning: could not download allmods.zip" >&2
fi
if [[ -f "${MODLAND_ZIP}" ]]; then
    MODLAND_DATE="$(unzip -l "${MODLAND_ZIP}" allmods.txt | awk '/allmods.txt$/ {print $2; exit}')"
    if [[ "${MODLAND_DATE}" =~ ^([0-9]{2})-([0-9]{2})-([0-9]{4})$ ]]; then
        MODLAND_YEAR="${BASH_REMATCH[3]}"
        MODLAND_MONTH="${BASH_REMATCH[1]}"
        MODLAND_DAY="${BASH_REMATCH[2]}"
    elif [[ "${MODLAND_DATE}" =~ ^([0-9]{4})-([0-9]{2})-([0-9]{2})$ ]]; then
        MODLAND_YEAR="${BASH_REMATCH[1]}"
        MODLAND_MONTH="${BASH_REMATCH[2]}"
        MODLAND_DAY="${BASH_REMATCH[3]}"
    else
        MODLAND_YEAR="$(date +%Y)"
        MODLAND_MONTH="$(date +%m)"
        MODLAND_DAY="$(date +%d)"
    fi
    gcc -O2 -I"${SCRIPT_DIR}/host-makedb" -I"${SCRIPT_DIR}/.." -o "${BUILD_DIR}/modland-makedb" \
        "${SCRIPT_DIR}/modland-makedb.c" "${SCRIPT_DIR}/../stuff/file.c"
    unzip -p "${MODLAND_ZIP}" allmods.txt > "${MODLAND_TXT}"
    rm -rf "${MODLAND_DAT_DIR}"
    mkdir -p "${MODLAND_DAT_DIR}"
    "${BUILD_DIR}/modland-makedb" "${MODLAND_TXT}" "${MODLAND_DAT_DIR}/" "${MODLAND_YEAR}" "${MODLAND_MONTH}" "${MODLAND_DAY}"
    mv -f "${MODLAND_DAT_DIR}/CPMDLAND.DAT" "${BUILD_DIR}/CPMDLAND.DAT"
    rm -rf "${MODLAND_DAT_DIR}" "${MODLAND_TXT}" "${MODLAND_ZIP}" "${BUILD_DIR}/modland-makedb"
    echo "Built ${BUILD_DIR}/CPMDLAND.DAT"
elif [[ -f "${BUILD_DIR}/CPMDLAND.DAT" ]]; then
    echo "Warning: keeping the CPMDLAND.DAT already in ${BUILD_DIR}" >&2
else
    echo "Warning: no prebuilt Modland catalog. The modland.com drive stays empty until Refresh database." >&2
fi
rm -f "${BUILD_DIR}/allmods.zip"

echo "Configuring build system..."
emcmake cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" "${CMAKE_FLAGS[@]}"

echo "Building (emmake make -C ${BUILD_DIR})"
emmake make -C "${BUILD_DIR}"

echo "Copying web interface..."
cp -f "${SCRIPT_DIR}/index.html" "${BUILD_DIR}/"
cp -f "${SCRIPT_DIR}/visitor-file.js" "${BUILD_DIR}/"
cp -f "${SCRIPT_DIR}/../COPYING" "${BUILD_DIR}/COPYING"
cp -f "${SCRIPT_DIR}/UNIFONT-LICENSE.txt" "${BUILD_DIR}/UNIFONT-LICENSE.txt"

if [[ ${INCLUDE_SAMPLES} -eq 1 ]]; then
    SAMPLE_SRC="${SCRIPT_DIR}/sample-files"
    SAMPLE_DEST="${BUILD_DIR}/sample-files"
    if [[ -d "${SAMPLE_SRC}" ]]; then
        echo "Staging sample music files in ${SAMPLE_DEST}..."
        SAMPLE_SRC="${SAMPLE_SRC}" SAMPLE_DEST="${SAMPLE_DEST}" python3 - <<'PY'
import json
import os
import pathlib
import shutil

src = pathlib.Path(os.environ["SAMPLE_SRC"]).resolve()
dest = pathlib.Path(os.environ["SAMPLE_DEST"]).resolve()

if dest.exists():
    shutil.rmtree(dest)
dest.mkdir(parents=True, exist_ok=True)

# Load optional metadata.json file if it exists
# This file can contain additional metadata for sample files:
# {
#   "relative/path/to/file.mod": {
#     "title": "Song Title",
#     "composer": "Composer Name",
#     "artist": "Artist Name",
#     "modtype": "MOD ",   // 4-char type string
#     "channels": 4,
#     "playtime": 180,     // duration in seconds
#     "style": "Chiptune",
#     "comment": "A comment",
#     "album": "Album Name",
#     "date": 19950101     // YYYYMMDD format
#   }
# }
metadata_file = src / "metadata.json"
metadata = {}
if metadata_file.exists():
    try:
        metadata = json.loads(metadata_file.read_text(encoding="utf-8"))
        print(f"Loaded metadata for {len(metadata)} files from metadata.json")
    except Exception as e:
        print(f"Warning: Failed to load metadata.json: {e}")

manifest = []

for path in sorted(src.rglob('*')):
    if not path.is_file():
        continue
    # Skip license notes and the metadata sidecar. They are not modules.
    if path.name in ("metadata.json", "LICENSE.md") or path.suffix.lower() == ".md":
        continue
    rel = path.relative_to(src).as_posix()
    out_path = dest / rel
    out_path.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(path, out_path)

    entry = {
        "path": rel,
        "size": path.stat().st_size
    }

    # Merge metadata if available for this file
    if rel in metadata:
        for key in ["title", "composer", "artist", "modtype", "channels", "playtime", "style", "comment", "album", "date"]:
            if key in metadata[rel]:
                entry[key] = metadata[rel][key]

    manifest.append(entry)

manifest_path = dest / "manifest.json"
manifest_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
PY
    else
        echo "Warning: sample files requested but directory not found: ${SAMPLE_SRC}"
    fi
else
    SAMPLE_DEST="${BUILD_DIR}/sample-files"
    if [[ -e "${SAMPLE_DEST}" ]]; then
        echo "Removing staged sample files (not requested)..."
        rm -rf "${SAMPLE_DEST}"
    fi
fi

echo "Updating test symlinks..."
python3 - <<'PY'
import os

root = os.path.dirname(os.path.abspath(__file__))
build = os.path.join(root, 'build')
tests = os.path.join(root, 'tests')

# Remove legacy per-file symlinks from the wasm root and tests folder
legacy_artifacts = ('ocp.js', 'ocp.wasm', 'ocp.data')
for artifact in legacy_artifacts:
    link_path = os.path.join(root, artifact)
    if os.path.islink(link_path):
        os.unlink(link_path)
    tests_link = os.path.join(tests, artifact)
    if os.path.islink(tests_link):
        os.unlink(tests_link)

# Ensure wasm/tests/build -> ../build symlink exists
if os.path.isdir(tests):
    target = os.path.relpath(build, tests)
    link_path = os.path.join(tests, 'build')
    if os.path.islink(link_path) or os.path.exists(link_path):
        if os.path.islink(link_path):
            os.unlink(link_path)
        else:
            raise SystemExit(f"Refusing to overwrite existing file: {link_path}")
    os.symlink(target, link_path)
PY

cat <<EOT

============================================================
Build complete! (${BUILD_CONFIG} configuration)
============================================================

Artifacts are available in: ${BUILD_DIR}

To test locally, run:
  cd "${BUILD_DIR}"
  python3 -m http.server 8080

Then open http://localhost:8080 in your browser.

Tip: Use './build.sh --help' to see all build configurations.
EOT
