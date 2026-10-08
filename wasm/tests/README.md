# OCP WASM Test Suite

Comprehensive CLI test suite for OpenCubicPlayer WASM canvas rendering and sound output validation, designed for CI/CD integration.

## Overview

This test suite validates that the WASM version of OpenCubicPlayer correctly handles:

- **Canvas Rendering**: Text display, color rendering, screen updates, and OCP interface drawing
- **Sound Output**: Web Audio API integration, SDL2 audio, OCP audio systems, and MOD playback

## Quick Start

```bash
# Setup and run all tests
cd wasm/tests
npm install
npm test
```

## Test Structure

```
wasm/tests/
├── package.json              # Dependencies and npm scripts
├── test-runner.js            # Main CLI test orchestrator
├── test-canvas.js            # Canvas rendering tests
├── test-audio.js             # Audio output tests
├── test-*.html               # Browser-based test pages
├── test-visitor-file.js      # Shipped page: a visitor file lands in /music
├── test-config-roundtrip.js  # Shipped page: a saved ocp.ini is read after reload
├── test-playback.js          # Shipped page: open a demo module, check sound and keys
├── *.test.js                 # Jest: wrapped symbols, page contract, dynCall list
└── README.md                 # This file
```

## CLI Commands

### Basic Usage

```bash
# Run complete test suite
npm test
# or
node test-runner.js

# Run specific test suites
npm run test:canvas    # Canvas tests only
npm run test:audio     # Audio tests only

# CI mode with JSON output
npm run test:ci
```

### Advanced Options

```bash
# Verbose output
node test-runner.js --verbose

# Run with visible browser (debugging)
node test-runner.js --no-headless --verbose

# Skip specific test suites
node test-runner.js --skip-canvas    # Audio only
node test-runner.js --skip-audio     # Canvas only

# Enable actual audio playback (not recommended for CI)
node test-runner.js --enable-audio

# Custom timeout
node test-runner.js --timeout 60000

# Output results to file
node test-runner.js --output-file results.json
```

## Test Categories

### Canvas Rendering Tests

**Infrastructure Tests:**
- Canvas element existence and initialization
- 2D context and WebGL support
- Correct canvas dimensions (1280x1024)

**WASM Integration Tests:**
- WASM module loading and initialization
- SDL2 system availability
- Required function exports

**Rendering Capability Tests:**
- Text rendering functionality
- Color rendering and graphics
- Screen update mechanisms
- OCP interface drawing integration

### Audio Output Tests

**Web Audio API Tests:**
- Web Audio API support detection
- AudioContext creation and state management
- Sample rate validation (44.1kHz+)
- Basic audio node creation

**SDL2 Audio Tests:**
- SDL2 audio system integration
- Audio device initialization
- Audio callback functionality

**OCP Audio System Tests:**
- WASM module audio integration
- MCP (Music Control Protocol) system
- PLR (Player) system functionality
- Audio mixer system

**Audio Output Tests:**
- Test tone generation (440Hz)
- Stereo output capability
- Volume control functionality
- MOD file playback capability
- Audio visualization support

## Workflow file

GitHub Actions runs `.github/workflows/ocp-wasm-tests.yml` on every push to the release branch and on pull requests. It builds with Emscripten 5.0.2, installs the test dependencies from `package-lock.json` with `npm ci`, runs `npm test`, and uploads the site files as a build artifact. It does not post pull-request comments.

## Output Formats

### Human-Readable Output

```
============================================================
    OpenCubicPlayer WASM Test Suite Results
============================================================
Test Duration: 12.5s
Total Tests: 24
Passed: 22
Failed: 2
Success Rate: 91.7%

Canvas Rendering Tests:
  Tests: 12
  Passed: 11
  Failed: 1
  Success Rate: 91.7%

Audio Output Tests:
  Tests: 12
  Passed: 11
  Failed: 1
  Success Rate: 91.7%
```

### CI/JSON Output

```json
{
  "timestamp": "2025-01-20T10:30:00.000Z",
  "duration": "12.5",
  "overall": {
    "total": 24,
    "passed": 22,
    "failed": 2,
    "successRate": 91.7
  },
  "canvas": {
    "total": 12,
    "passed": 11,
    "failed": 1,
    "successRate": 91.7,
    "results": [...]
  },
  "audio": {
    "total": 12,
    "passed": 11,
    "failed": 1,
    "successRate": 91.7,
    "results": [...]
  },
  "exitCode": 1
}
```

## Environment Requirements

### Dependencies

- **Node.js 18+**
- **npm**
- **Puppeteer** (for headless browser testing)
- **Chalk** (for colored output)
- **Commander** (for CLI parsing)

### Browser Requirements

The tests use Puppeteer with Chromium, which supports:
- Canvas 2D API
- WebGL
- Web Audio API
- WASM execution
- SDL2 emscripten port

### WASM Files

Tests require the build artifacts produced by `wasm/build.sh`. The script creates
`wasm/tests/build` as a symlink to the latest build output, so the following
paths must exist before running the suite:
- `build/ocp.js` - Emscripten-generated JavaScript
- `build/ocp.wasm` - Compiled WASM binary
- `build/ocp.data` - Asset data (optional)

Build these first with:
```bash
cd wasm
./build.sh
```

## Troubleshooting

### Expected Test Results

**With HTTP Server (All Modes):**
- Canvas Tests: ~80-90% pass rate (WASM loads completely)
- Audio Tests: ~70-80% pass rate (full OCP audio system tested)
- Infrastructure tests pass, WASM module loads completely
- SDL2 and OCP audio systems are fully tested

**Expected Limitations in Headless Mode:**
- WebGL not supported (hardware limitation)
- Some canvas operations may have reduced functionality
- Audio playback verification limited (no actual sound output)

### Common Issues

**"Network Error: ocp.data" or "WASM module not loaded"**
- ✅ **SOLVED**: Tests now use HTTP server to bypass browser file:// security restrictions
- The HTTP server automatically starts and serves WASM files properly
- WASM module now loads completely in headless mode
- Full OCP functionality is tested including SDL2 and audio systems

**"Canvas element not found"**
- Ensure test HTML files are in the same directory
- Check file permissions

**"AudioContext creation failed"**
- This is expected in headless mode for some audio tests
- Use `--enable-audio` flag for full audio testing (not recommended for CI)

**"Puppeteer browser launch failed"**
- Install missing system dependencies:
  ```bash
  # Ubuntu/Debian
  sudo apt-get install -y chromium-browser

  # Or use bundled Chromium
  npm install puppeteer
  ```

**"WebGL not supported"**
- Expected in headless mode - WebGL requires GPU acceleration
- This doesn't affect basic canvas 2D functionality

### Debug Mode

Run tests with visible browser for debugging:
```bash
node test-runner.js --no-headless --verbose
```

### Verbose Logging

Enable detailed logging:
```bash
node test-runner.js --verbose
```

## Development

### Adding New Tests

1. **Canvas Tests**: Add to `test-canvas.js` in the appropriate test method
2. **Audio Tests**: Add to `test-audio.js` following the existing pattern
3. **Integration Tests**: Modify `test-runner.js` for orchestration

### Test Structure

Each test should return an object with:
```javascript
{
    name: "Test Name",
    passed: true/false,
    details: "Descriptive message about the test result"
}
```

### Browser Test Pages

The HTML test pages provide browser-based testing and can be opened directly for manual testing or debugging.

## Test Utilities

### dyncall-fallback.js

**Purpose**: Provides compatibility shims for legacy `dynCall_*` functions when running tests in headless Puppeteer environments.

**Why Needed**:
- Modern Emscripten omits `dynCall` shims unless explicitly requested in exported runtime methods
- Test pages load the WASM bundle directly without the production `index.html` initialization wrapper
- Headless browser environment lacks some Module initialization that the production build provides
- **Not needed in production**: The main `index.html` includes proper initialization sequences

**How It Works**:
The fallback script provides JavaScript implementations of the dynCall functions by looking up function pointers in the WASM table:

```javascript
// Example: dynCall_iiii calls a function with signature (int, int, int) -> int
globalThis.dynCall_iiii = function(index, arg1, arg2, arg3) {
    const table = Module.wasmTable || globalThis.wasmTable;
    const fn = table.get(index);  // Look up function in WASM table
    return fn(arg1, arg2, arg3);  // Call it with arguments
};
```

**Supported Signatures**: 30+ function signatures including:
- Integer returns: `i`, `ii`, `iii`, `iiii`, `iiiii`, etc.
- Void returns: `v`, `vi`, `vii`, `viii`, `viiii`, etc.
- 64-bit returns: `j`, `ji`, `jii`, `jiii`, etc.
- Float/double returns: `f`, `d`, `fiii`, `diii`, etc.

**Usage in Test Files**:
```html
<!-- Load BEFORE the WASM bundle -->
<script src="dyncall-fallback.js"></script>
<script src="build/ocp.js"></script>
```

**When to Update**:
If Emscripten changes its function calling conventions or if new function signatures are needed, update the `signatures` array in `dyncall-fallback.js`.

## License

This test suite is part of OpenCubicPlayer and follows the same GPL-2.0 license.
