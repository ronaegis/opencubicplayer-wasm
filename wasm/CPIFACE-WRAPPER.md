# cpiface-wrapper.c Maintenance Guide

## Purpose

Wraps cpiface.c to add WASM-specific behavior (stepper mode, latency compensation)
without modifying the original source file.

## Why This Approach?

The macro-include trick is used because:

- ✗ **Weak symbols**: Static functions can't be weak; Emscripten has bugs in MAIN_MODULE
- ✗ **--wrap flag**: Only works for undefined symbols, not static functions within same unit
- ✗ **Separate compilation**: Can't access static functions from another .o file
- ✗ **Modify cpiface.c**: Violates project policy of no original source modifications

✓ **Macro rename + include**: Only approach that works with all constraints

## Validation

All validation runs automatically with:

```bash
cd wasm/tests
npm test
```

The test suite validates:

### Compile-Time (during build)
- Static assertions verify structures are defined
- Symbol address checks ensure renamed functions exist
- Header inclusion guards prevent build configuration errors

### Build-Time (in npm test)
- Required symbols are exported from WASM module
- Original function names are properly renamed with `cpiface_original_` prefix
- All renamed symbols exist in object files
- New static functions in cpiface.c are detected and reported

### Runtime (during initialization)
- Wrapper installation circular reference check
- NULL pointer validation
- Function pointer sanity checks

## Adding a New Wrapped Function

If cpiface.c adds a new static function that needs WASM-specific behavior:

### 1. Add to cpiface-wrapper.c rename list (after line 32):

```c
#define newFunction    cpiface_original_newFunction
```

### 2. Add to undef list (after line 51):

```c
#undef newFunction
```

### 3. Add to compile-time validation (in `__validate_renamed_symbols` function, around line 82):

```c
static void __attribute__((unused)) __validate_renamed_symbols(void) {
    volatile void *check[] = {
        // ... existing symbols ...
        (void*)&cpiface_original_newFunction,
    };
    (void)check;
}
```

### 4. Implement wrapper function:

```c
static returnType wasm_newFunction(args...) {
    // WASM-specific logic here
    return cpiface_original_newFunction(args...);
}
```

### 5. Update test (wasm/tests/cpiface-wrapper.test.js):

Add to `knownWrapped` array:

```javascript
const knownWrapped = [
  // ... existing functions ...
  'newFunction',
];
```

Add to `renamedSymbols` array:

```javascript
const renamedSymbols = [
  // ... existing symbols ...
  'cpiface_original_newFunction',
];
```

### 6. Run tests:

```bash
cd wasm/tests
npm test
```

## Current Wrapped Functions

| Original | Wrapper | Purpose |
|----------|---------|---------|
| `plmpCallBack` | `wasm_plmpCallBack` | Stepper-based rendering loop |
| `plmpCloseFile` | `wasm_plmpCloseFile` | State cleanup + latency reset |
| `plmpLateInit` | `wasm_plmpLateInit` | Install wrappers + validation |
| `plmpOpenScreen` | Uses original | Wrapped by stepper |
| `plmpDrawScreen` | Uses original | Wrapped by stepper |
| `plmpCloseScreen` | Uses original | Wrapped by stepper |

## Known Unwrapped Static Functions

These are internal helpers that don't need WASM-specific wrapping:

- `mcpDrawGStrings*` family - rendering helpers
- `drawchannel`, `drawvolbar`, `drawlongvolbar` - UI drawing
- `normalize`, `mcpSetFadePars` - utility functions
- `setFXParamsFromMixer`, `readFXParams` - FX parameter handling
- `renderVuMeter` - VU meter rendering
- `vol16_to_str`, `vol64_to_str` - volume formatting

If `npm test` reports new static functions, determine if they need wrapping
(modify global state, event loops, timing) or are internal helpers.

## Troubleshooting

### "Missing required symbols" error
- Check that #define and #undef match exactly
- Verify symbol is in `__validate_renamed_symbols()` function
- Run `nm` on the object file to see what symbols are actually present:
  ```bash
  nm wasm/build/CMakeFiles/ocp-wasm.dir/wasm/cpiface-wrapper.c.o | grep cpiface_original
  ```

### "Found unrenamed symbols" error
- A symbol wasn't properly renamed with cpiface_original_ prefix
- Check #define list in cpiface-wrapper.c
- Ensure #define appears before `#include "../cpiface/cpiface.c"`

### "NEW static functions found" warning
- cpiface.c added new static functions
- Review each function to determine if it needs wrapping
- If it's an internal helper, add to `knownUnwrapped` in test

### Compile errors about missing symbols
- Run `npm test` to see which symbols are missing
- Add missing symbols to compile-time validation function
- Check that the function exists in cpiface.c

### Runtime "Circular reference detected" error
- The wrapper is calling itself instead of the original
- Verify #define renames are applied before including cpiface.c
- Check that #undef is after the include

## Architecture Details

### How the Macro-Include Works

1. **Before include**: Define macros to rename symbols
   ```c
   #define plmpCallBack cpiface_original_plmpCallBack
   ```

2. **Include source**: Pull in entire cpiface.c
   ```c
   #include "../cpiface/cpiface.c"
   ```
   All static functions are now renamed to `cpiface_original_*`

3. **After include**: Undefine macros to use original names
   ```c
   #undef plmpCallBack
   ```

4. **Implement wrappers**: Create new functions with original names
   ```c
   static interfaceReturnEnum wasm_plmpCallBack(void) {
       // WASM-specific logic
       return cpiface_stepper_run_blocking(&state);
   }
   ```

5. **Export**: Install wrappers in plugin structure
   ```c
   cpiface_original_plOpenCP.Run = wasm_plmpCallBack;
   ```

### Why This Preserves Static Data

When `#include "../cpiface/cpiface.c"` is executed, **the entire translation unit**
is included, including:
- All static variables (e.g., `static int plmpInited = 0`)
- All static arrays
- All static structures

This means any new static data added to cpiface.c automatically becomes part of
cpiface-wrapper.c without any manual tracking.

### WASM-Specific Modifications

The wrapper adds:

1. **Stepper mode**: Converts blocking event loop to frame-by-frame rendering
   - `cpiface_stepper_tick()` advances one frame
   - Allows Emscripten's main loop to remain responsive

2. **Audio latency compensation**: Offsets pattern view timer
   - `wasm_mcpGet_latency_wrapper()` subtracts browser audio latency
   - Keeps pattern display synchronized with audio output

3. **State management**: Proper cleanup on file close
   - `wasm_plmpCloseFile()` resets stepper state
   - Prevents state leakage between files

## Testing Strategy

The test suite uses multiple validation layers:

### Layer 1: Compile-Time (Immediate Feedback)
- Catches missing functions during build
- Validates structure definitions
- Ensures headers are included

### Layer 2: Build-Time (Fast Feedback)
- Runs in ~1 second during `npm test`
- Checks symbol exports in object files
- Validates rename patterns

### Layer 3: Source Analysis (Change Detection)
- Parses cpiface.c for new static functions
- Warns about potential maintenance needs
- Helps prevent silent breakage

### Layer 4: Runtime (Deployment Safety)
- Validates during plugin initialization
- Catches configuration errors
- Prevents circular references

This multi-layered approach ensures issues are caught as early as possible.

## References

- Main wrapper implementation: `wasm/cpiface-wrapper.c`
- Test suite: `wasm/tests/cpiface-wrapper.test.js`
- Stepper interface: `wasm/cpiface_stepper.h`
- Original source: `cpiface/cpiface.c` (DO NOT MODIFY)

## Version History

- **2025-10-15**: Added comprehensive validation (compile-time, build-time, runtime)
- **Earlier**: Initial implementation with macro-include approach
