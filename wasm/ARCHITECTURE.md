# WASM Architecture Overview

**Last updated:** October 7, 2026
**Status:** Experimental browser port. See wasm/README.md for what the build actually does.

---

## Design Principles

The WASM port follows these core principles:

1. **Minimal Upstream Changes**: Keep modifications to original OCP files to an absolute minimum
2. **Wrapper Pattern**: Use macro-include wrappers to add WASM-specific behavior without modifying original sources
3. **Non-Blocking Architecture**: Replace blocking operations with stepper-based implementations for browser event loop compatibility
4. **Platform Abstraction**: Runtime-configurable behavior through platform callbacks instead of hardcoded stubs
5. **Standard Build**: Avoid post-processing or patching of generated WASM bundles

---

## Platform Abstraction Layer

### Overview

The platform abstraction layer provides runtime-configurable behavior for timing, paths, and other platform-specific functionality. This eliminates hardcoded stubs and enables browser-based configuration through JavaScript APIs.

### Components

#### 1. Framelock Platform API

**Files:**
- `framelock-platform.h` - Platform timing API definition
- `framelock-platform.c` - WASM implementation
- `framelock-wasm.c` - Full OCP framelock using platform callbacks

**Purpose:** Browser-driven timing without `usleep()` or `gettimeofday()` calls

**Key Functions:**
```c
void framelock_platform_init(void);
void framelock_platform_sleep(unsigned long usec);
unsigned long framelock_platform_get_time_usec(void);
int framelock_platform_needs_sleep(void);          // Returns 0 for WASM
int framelock_platform_should_render(int target_fps);
```

**Integration:** Controlled by `emscripten_set_main_loop()` for smooth browser integration

#### 2. Path Configuration API

**Files:**
- `psetting-platform.h` - Path configuration API
- `psetting-platform.c` - WASM path management

**Purpose:** Runtime-configurable virtual filesystem paths

**Supported Path Types:**
- `program` - Program directory
- `home` - User home directory
- `config` - Configuration directory
- `temp` - Temporary files
- `assets` - Asset files (help, fonts)
- `data` - Data files
- `doc` - Documentation

**Key Functions:**
```c
int psetting_platform_init_paths(struct configAPI_t *cfgAPI);
int psetting_platform_set_path(const char *key, const char *value);
const char *psetting_platform_get_path(const char *key);
const char *psetting_platform_get_paths_json(void);
```

#### 3. JavaScript Configuration API

**File:** `wasm-platform-config-api.c`

**Exported Functions (7):**

```javascript
// Path Configuration
Module.ccall('wasm_set_virtual_path', 'number', ['string', 'string'], ['program', '/custom/']);
Module.ccall('wasm_get_virtual_path', 'string', ['string'], ['program']);
Module.ccall('wasm_get_paths_json', 'string', [], []);

// Timing Control
Module.ccall('wasm_set_no_sleep_mode', 'void', ['number'], [1]);
Module.ccall('wasm_get_no_sleep_mode', 'number', [], []);

// Debugging
Module.ccall('wasm_get_frame_count', 'number', [], []);
Module.ccall('wasm_force_frame_render', 'void', [], []);
```

**Benefits:**
- No recompilation needed to change paths
- Runtime configuration from JavaScript
- Easy to adapt for different deployment scenarios

---

## Wrapper Pattern

All WASM-specific behavior uses the "macro-include wrapper" pattern.

### How It Works

1. **Define** macros to rename original symbols before inclusion
2. **Include** the original `.c` file to get all static functions and data
3. **Undefine** macros and reimplement with WASM-specific behavior
4. Link the wrapper instead of the original in `CMakeLists.txt`

### Example: `pfilesel-stepper.c`

```c
// Step 1: Rename original functions
#define fsFileSelect fsFileSelect_blocking
#define fsInit fsInit_blocking

// Step 2: Include original file (gets all internals)
#include "../filesel/pfilesel.c"

// Step 3: Undefine and provide new implementations
#undef fsFileSelect
#undef fsInit

// Provide stepper-based implementation
signed int fsFileSelect(void) {
    struct fsSetup_stepper_engine *setup_engine = fsSetup_stepper_get_engine();

    /* If setup stepper is active, drive it instead of file selector */
    if (fsSetup_stepper_is_active(setup_engine)) {
        if (fsSetup_stepper_run(setup_engine)) {
            /* Setup finished - cleanup and return to file selector */
            fsSetup_stepper_finish(setup_engine);
            plSetTextMode(plScrType);
            fsScanDir(0);
        }
        return 0;
    }

    /* Normal file selector logic */
    result = fsFileSelectStepperRun(&fs_stepper_engine, 1);
    // ...
}
```

### Why This Pattern?

- **Static Functions**: Original functions use `static` (internal linkage), can't be called externally
- **Weak Symbols Don't Work**: Emscripten has bugs with weak symbols in MAIN_MODULE mode
- **--wrap Limitations**: Linker `--wrap` doesn't work within the same compilation unit
- **No Upstream Changes**: Project policy requires minimal modifications to original OCP files
- **Preprocessor Works**: Macro renaming is the only reliable approach given these constraints

### Existing Wrappers

| Wrapper File | Purpose | Original File | Lines |
|--------------|---------|---------------|-------|
| `pfilesel-stepper.c` | Non-blocking file selector + setup stepper | `filesel/pfilesel.c` | ~158 |
| `cpiface-wrapper.c` | Stepper-based interface rendering | `cpiface/cpiface.c` | ~200 |
| `wasm-filesystem-unix-wrapper.c` | Reference counting fixes | `filesel/filesystem-unix.c` | ~100 |
| `poutput-sdl2-wrapper.c` | SDL2 output with browser adaptations | `stuff/poutput-sdl2.c` | ~150 |

### Adding a New Wrapper

1. Create `wasm/yourfile-wrapper.c`
2. Add comprehensive header comment explaining:
   - **PURPOSE**: What WASM-specific behavior you're adding
   - **WHY**: Why the wrapper pattern is necessary
   - **MAINTENANCE**: How to validate changes
3. Implement the wrapper pattern
4. Update `wasm/CMakeLists.txt` to link your wrapper instead of the original
5. Add tests to verify behavior

---

## Stepper Architecture

### Problem: Blocking Loops in Browser

Traditional OCP uses blocking loops:
```c
// Desktop version - blocks until user quits
void run_file_selector(void) {
    while (1) {
        handle_input();
        draw_screen();
        if (user_quit) break;
    }
}
```

This blocks the browser event loop, freezing the UI.

### Solution: Stepper Pattern

Break infinite loops into single iterations:

```c
// WASM version - returns after each iteration
int run_file_selector_step(void) {
    framelock();  // Proper timing

    handle_input();
    draw_screen();

    if (user_quit) return DONE;
    return IN_PROGRESS;
}

// Called from browser main loop (60fps)
void browser_main_loop(void) {
    int result = run_file_selector_step();
    if (result == IN_PROGRESS) {
        // Yield to browser, will be called again
    }
}
```

### Stepper Implementations

#### 1. File Selector Stepper

**File**: `filesel/pfilesel.c:3234-4057`

```c
struct fs_file_select_engine {
    int is_active;   /* Engine is currently running */
    int first_run;   /* First iteration flag */
};

/* Run one iteration of file selector */
signed int fsFileSelectStepperRun(
    struct fs_file_select_engine *engine,
    int iteration_limit  /* 1 = single step, -1 = unlimited */
) {
    /* Transform while(1) into for loop with limit */
    for (int fs_iter = 0;
         (iteration_limit < 0) || (fs_iter < iteration_limit);
         fs_iter++) {
        // One iteration of file selector logic
        // ...

        if (file_selected) return 1;
        if (user_cancelled) return 0;
        if (error) return -1;
    }
    return FS_FILESELECT_RESULT_IN_PROGRESS;  // Still running
}
```

#### 2. Setup Dialog Stepper

**Files:**
- `fsSetup-stepper.h` - Setup stepper API
- `fsSetup-stepper.c` - Non-blocking implementation

**Purpose:** Fixes ALT-C hang by running setup dialog incrementally

**Key Structure:**
```c
struct fsSetup_stepper_engine {
    int is_active;
    int first_run;
    int stored;             /* Config saved flag */
    int in_keyboard_help;   /* Currently showing keyboard help */
    int last_fps_current;   /* For update detection */
};
```

**Integration:**
```c
void fsSetup(void) {
    struct fsSetup_stepper_engine *engine = fsSetup_stepper_get_engine();
    /* Just initialize the stepper and return immediately */
    fsSetup_stepper_init(engine);
}

int fsSetup_stepper_run(struct fsSetup_stepper_engine *engine) {
    /* Draw the dialog */
    fsSetup_stepper_draw(engine);

    framelock();  /* Proper timing */

    /* Process ALL pending keyboard events (don't wait!) */
    while (Console.KeyboardHit()) {
        c = Console.KeyboardGetChar();
        // Handle keyboard input
    }

    /* Continue on next frame */
    return 0;
}
```

**Critical Fix:** `fsFileSelectIsActive()` must return TRUE when EITHER file selector OR setup stepper is active, otherwise runtime mode switches away and stepper never runs.

#### 3. Interface Stepper

**File**: `wasm/cpiface-wrapper.c`

Similar pattern for the cpiface interface, allowing pattern view and controls to update incrementally without blocking.

---

## State Machine Architecture

### Runtime Modes

The WASM runtime uses an explicit state machine to manage transitions between file selector and playback:

```c
enum wasm_runtime_mode {
    WASM_MODE_INIT,                  /* Initial state, not yet ready */
    WASM_MODE_FILESEL,               /* File selector active */
    WASM_MODE_FILESEL_SELECTED,      /* File selected, ready to load */
    WASM_MODE_PLAYBACK,              /* Playing a file */
    WASM_MODE_PLAYBACK_WITH_FILESEL, /* Playback with file selector overlay */
    WASM_MODE_VIRTUAL_INTERFACE,     /* Virtual device interfaces (modland setup) */
    WASM_MODE_ERROR                  /* Error state */
};
```

### State Transitions

```
INIT --> FILESEL --> FILESEL_SELECTED --> PLAYBACK
            |              |                   |
            |              |                   +---> PLAYBACK_WITH_FILESEL
            |              |                              |
            +---> VIRTUAL_INTERFACE                       |
                           |                              |
                           +------------------------------+
                                    (Press 'F' key or ESC)
```

**State Management** (`wasm-original-main.c:85-94`):
```c
struct wasm_runtime_state {
    enum wasm_runtime_mode mode;
    struct moduleinfostruct pending_module_info;
    struct ocpfilehandle_t *pending_filehandle;
};

static struct wasm_runtime_state g_runtime = {
    .mode = WASM_MODE_INIT,
    .pending_filehandle = NULL
};
```

### Main Loop Flow

**File**: `wasm-original-main.c:992-1021`

```c
void wasm_main_loop(void) {
    /* If wasm_ocp_main hasn't been called yet, just return */
    if (!wasm_ocp_main_called) {
        return;
    }

    /* Periodic IDBFS maintenance */
    extern void idbfs_tick(void);
    idbfs_tick();

    /* Handle file selector modes */
    if (g_runtime.mode == WASM_MODE_FILESEL ||
        g_runtime.mode == WASM_MODE_FILESEL_SELECTED) {
        wasm_drive_file_selector();  // Drive file selector or setup stepper
        return;
    }

    /* Handle file selector during playback */
    if (g_runtime.mode == WASM_MODE_PLAYBACK_WITH_FILESEL) {
        wasm_drive_file_selector_with_playback();
        return;
    }

    /* Handle playback or virtual interface mode */
    if (g_runtime.mode == WASM_MODE_PLAYBACK ||
        g_runtime.mode == WASM_MODE_VIRTUAL_INTERFACE) {
        wasm_interface_main_loop();  // Drive cpiface stepper
        return;
    }
}
```

---

## Minimal Upstream Modifications

While we strive to avoid modifying original OCP files, a few minimal changes were necessary:

### Modified: `filesel/pfilesel.c`

**Changes:** 7 lines added (conditional compilation)

**Purpose:** Allow WASM to provide non-blocking fsSetup() stepper

**Modifications:**

1. **Wrapped fsSetup() definition** (lines 2196-2344):
```c
#ifndef OCP_WASM_FILESEL_STEPPER
void fsSetup(void) {
    // ... original blocking implementation ...
}
#endif /* OCP_WASM_FILESEL_STEPPER */
```

2. **Modified ALT-C handler** (lines 3581-3591):
```c
case KEY_ALT_C:
    fsSetup();
#ifndef OCP_WASM_FILESEL_STEPPER
    plSetTextMode(plScrType);
    fsScanDir(0);
    goto superbreak;
#else
    /* In stepper mode, return immediately to let wrapper drive setup stepper */
    return FS_FILESELECT_RESULT_IN_PROGRESS;
#endif
```

**Rationale:**
- Prevents simultaneous execution of file selector and setup dialog
- Allows wrapper to drive setup stepper exclusively
- Zero impact on non-WASM builds (conditional compilation)
- Follows existing patterns in codebase (similar to modland stepper)

**Documentation:** See `wasm/PFILESEL-MODIFICATIONS.md` for complete details

---

## Memory Management

### Reference Counting

WASM environment requires explicit reference counting for file handles and directory references:

**Pattern**:
```c
/* Acquire reference when storing */
if (dmFile && dmFile->basedir) {
    dirdbRef(dmFile->basedir->dirdb_ref, dirdb_use_dir);
}

/* Release reference when done */
static void wasm_runtime_clear_pending(void) {
    if (g_runtime.pending_filehandle) {
        g_runtime.pending_filehandle->unref(g_runtime.pending_filehandle);
        g_runtime.pending_filehandle = NULL;
    }
}
```

**Files**:
- `wasm/wasm-filesystem-unix-wrapper.c` - Adds missing dirdb references
- `wasm/wasm-original-main.c:651-658` - Cleanup helper for pending files

---

## Build System

### Removed: Asyncify

**Before** (Complex):
- Asyncify enabled (`-s ASYNCIFY=1`)
- Python post-processing to patch function calls
- Manual injection of dynCall wrappers
- ~140 lines of build.sh patching code

**After** (Simple):
- No Asyncify
- No post-processing
- Clean CMake configuration
- Smaller, faster WASM bundle

**Benefit**: Simpler builds, better performance, easier maintenance

### CMake Configuration

**File**: `wasm/CMakeLists.txt:38-46`

```cmake
# Enable stepper-based file selector and platform abstraction
list(APPEND WASM_EXTRA_DEFINES OCP_WASM_FILESEL_STEPPER)

# Add compilation definitions
if(WASM_EXTRA_DEFINES)
    add_compile_definitions(${WASM_EXTRA_DEFINES})
endif()
```

**File**: `wasm/CMakeLists.txt:134-156` (STUFF_SOURCES)

```cmake
set(STUFF_SOURCES
    # Font system and display
    ../stuff/pfonts.c
    ../stuff/poutput-fontengine.c
    ../stuff/poutput-swtext.c
    ../stuff/cp437.c
    ../stuff/ttf.c
    ../stuff/poutput.c

    # SDL2 display wrapper
    poutput-sdl2-wrapper.c
    ../stuff/poutput-keyboard.c

    # Platform-aware framelock (NOT framelock_stub.c!)
    framelock-wasm.c
    framelock-platform.c

    # Other utilities
    ../stuff/poll.c
    ../stuff/err.c
    ../stuff/compat.c
    ../stuff/latin1.c
    ../stuff/utf-8.c
)
```

**File**: `wasm/CMakeLists.txt:256-284` (FILESEL_SOURCES)

```cmake
set(FILESEL_SOURCES
    # Full file selector system for WASM
    ../filesel/dirdb.c
    ../filesel/mdb.c
    pfilesel-stepper.c        # Wrapper with stepper integration
    fsSetup-stepper.c         # Setup dialog stepper
    ../filesel/pfsmain.c
    ../filesel/modlist.c
    # ... other files
)
```

**File**: `wasm/CMakeLists.txt:292-302` (WASM_SOURCES)

```cmake
set(WASM_SOURCES
    wasm-original-main.c
    wasm-stubs.c
    wasm-fileio.c
    sync-download.c
    idbfs-persistence.c
    psetting-wasm.c
    psetting-platform.c       # Platform path management
    wasm-platform-config-api.c  # JavaScript API exports
)
```

---

## Testing

### Test Architecture

**File**: `wasm/tests/test-runner.js`

- Runs in headless Chromium via Puppeteer
- Tests canvas rendering, audio output, MOD playback
- The Puppeteer suites do not load index.html. test-visitor-file.js does.

### Running Tests

```bash
cd wasm/tests
npm test
```

Run `npm test` and read the `Total Tests:` line it prints.

### Test Coverage

**Canvas Rendering Tests (15):**
- Canvas element existence
- 2D context availability
- WebGL support
- WASM module loading
- SDL2 initialization
- Text rendering
- Color rendering
- Screen updates
- MOD file handling

**Audio Output Tests (27):**
- Web Audio API support
- AudioContext creation and state
- Sample rate detection
- Audio node creation
- Stereo output support
- SDL2 audio integration
- OCP audio functions
- MCP/PLR/Mixer systems
- MOD playback capability
- Audio visualization support

**Validation Tests (7):**
- Build artifact verification
- Symbol export validation
- Wrapper symbol renaming
- Runtime validation

---

## File Organization

```
wasm/
├── ARCHITECTURE.md                    # This file
├── PLATFORM-ABSTRACTION.md            # Platform API guide
├── INTERACTIVE-DIALOGS-VERIFICATION.md # Dialog verification
├── PFILESEL-MODIFICATIONS.md          # pfilesel.c changes
├── ALT-C-REAL-FIX.md                  # ALT-C hang fix details
├── UNUSED-FILES.md                    # Cleanup documentation
├── WASM-PLATFORM-COMPLETE.md          # Implementation summary
│
├── CMakeLists.txt                     # Build configuration
├── build.sh                           # Build script
├── config.h                           # WASM-specific config
│
├── Platform Abstraction Layer
│   ├── framelock-platform.h           # Platform timing API
│   ├── framelock-platform.c           # WASM timing implementation
│   ├── framelock-wasm.c               # OCP framelock with platform
│   ├── psetting-platform.h            # Platform path API
│   ├── psetting-platform.c            # WASM path management
│   └── wasm-platform-config-api.c     # JavaScript exports
│
├── Stepper Implementations
│   ├── pfilesel-stepper.c             # File selector wrapper
│   ├── fsSetup-stepper.h              # Setup dialog API
│   ├── fsSetup-stepper.c              # Setup dialog implementation
│   └── cpiface-wrapper.c              # Interface stepper wrapper
│
├── WASM-specific Implementations
│   ├── wasm-original-main.c           # Main entry point & state machine
│   ├── wasm-interface-main.c          # Interface system integration
│   ├── wasm-stubs.c                   # Platform stubs
│   ├── wasm-fileio.c                  # File I/O wrappers
│   ├── sync-download.c                # Synchronous downloads
│   ├── idbfs-persistence.c            # Browser storage
│   ├── psetting-wasm.c                # WASM settings
│   └── download-web.c                 # Download infrastructure
│
├── Other Wrappers
│   ├── wasm-filesystem-unix-wrapper.c # Reference counting fixes
│   └── poutput-sdl2-wrapper.c         # SDL2 with browser adaptations
│
└── tests/
    ├── test-runner.js                 # Test automation
    ├── cpiface-wrapper.test.js        # Wrapper validation
    ├── dyncall-fallback.js            # Test compatibility shims
    └── test-*.html                    # Individual test pages
```

---

## Key Files and Their Purpose

| File | Purpose | Lines | Status |
|------|---------|-------|--------|
| **Platform Abstraction** | | | |
| `framelock-platform.h` | Platform timing API definition | 35 | ✅ Active |
| `framelock-platform.c` | WASM timing implementation | 92 | ✅ Active |
| `framelock-wasm.c` | Full OCP framelock with platform | 191 | ✅ Active |
| `psetting-platform.h` | Platform path API | 33 | ✅ Active |
| `psetting-platform.c` | WASM path management | 243 | ✅ Active |
| `wasm-platform-config-api.c` | JavaScript exports (7 functions) | 122 | ✅ Active |
| **Steppers** | | | |
| `pfilesel-stepper.c` | File selector + setup wrapper | 158 | ✅ Active |
| `fsSetup-stepper.h` | Setup stepper API | 35 | ✅ Active |
| `fsSetup-stepper.c` | Non-blocking setup dialog | 231 | ✅ Active |
| `cpiface-wrapper.c` | Interface rendering wrapper | 200 | ✅ Active |
| **Core** | | | |
| `wasm-original-main.c` | Main entry point, state machine | ~1020 | ✅ Active |
| `wasm-interface-main.c` | Interface system integration | ~520 | ✅ Active |
| `wasm-stubs.c` | Platform stubs | ~1560 | ✅ Active |
| **Deleted** | | | |
| `framelock_stub.c` | Minimal framelock stub | 509 | ❌ Deleted (Oct 23, 2025) |

---

## Best Practices

### When Adding Features

1. **Prefer platform abstraction** over hardcoded behavior
2. **Use stepper pattern** for any blocking loops
3. **Add tests** to `wasm/tests/` for new functionality
4. **Document state transitions** if adding new modes
5. **Run tests** after every change: `npm test --prefix wasm/tests`
6. **Check for blocking loops**: Search for `while (!KeyboardHit())`

### Memory Safety

1. Always `ref()` when storing file handles or directory references
2. Always `unref()` when releasing or in error paths
3. Use cleanup helpers to ensure consistent cleanup
4. Check for NULL before dereferencing

### State Management

1. Use `g_runtime.mode` for high-level state
2. Use `g_wasm_state` for playback state
3. Always transition through well-defined states
4. Clean up resources during state transitions
5. Ensure `fsFileSelectIsActive()` accounts for all active steppers

### Adding New Interactive Dialogs

If you need to create a new interactive dialog that might block:

1. **Check for blocking:** Search for `while (!KeyboardHit())`
2. **Create stepper:** Follow `fsSetup-stepper.c` pattern
3. **Wrap with ifdef:** Conditional compilation like pfilesel.c
4. **Update IsActive():** Ensure mode doesn't switch away
5. **Test thoroughly:** Manual browser testing required

---

## Common Issues and Solutions

### Issue: Dialog Hangs Browser

**Symptom:** Dialog appears but is frozen/unresponsive

**Cause:** Blocking loop in dialog code

**Solution:**
1. Create stepper implementation (see fsSetup-stepper.c)
2. Wrap original dialog with `#ifndef OCP_WASM_FILESEL_STEPPER`
3. Ensure wrapper drives stepper exclusively
4. Update `fsFileSelectIsActive()` to include new stepper

### Issue: Mode Switches Away During Dialog

**Symptom:** Dialog appears for one frame then disappears

**Cause:** `fsFileSelectIsActive()` returns FALSE

**Solution:** Update `fsFileSelectIsActive()` to check if stepper is active

### Issue: Build Fails After Adding File

**Symptom:** Linker errors about missing symbols

**Cause:** File not added to CMakeLists.txt

**Solution:** Add file to appropriate source list in CMakeLists.txt

---

## Future Improvements

### Potential Enhancements

1. **Extract stepper backend**: Move stepper implementation from `pfilesel.c` to separate file to reduce upstream changes
2. **Progressive loading**: Implement lazy loading for large module files
3. **Service worker**: Add offline support via service worker caching
4. **WebGL visualizations**: Port hardware-accelerated visualizers to WebGL
5. **Windows/DOS platform support**: Add platform implementations for other OSes

### Long-term Goals

- Zero changes to original OCP files (except configuration)
- Full feature parity with desktop builds
- Streaming support for large archives

---

_Last updated: 2026-10-05. Experimental. The test count is whatever `npm test` prints._
