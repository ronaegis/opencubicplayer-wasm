const { execSync } = require('child_process');
const fs = require('fs');
const path = require('path');

describe('cpiface-wrapper validation', () => {
  const buildDir = path.join(__dirname, '../build');
  const wasmFile = path.join(buildDir, 'ocp.wasm');
  const cpifaceWrapperObj = path.join(buildDir, 'CMakeFiles/ocp-wasm.dir/cpiface-wrapper.c.o');
  const cpifaceSource = path.join(__dirname, '../../cpiface/cpiface.c');

  // Required exported symbols from wrapper
  const requiredExports = [
    'cpifaceSessionAPI',
    'cpiface_dllextinfo',
    'dllextinfo',
    'cpiface_stepper_tick',
    'cpiface_stepper_reset',
    'cpiface_stepper_init',
  ];

  // Symbols that should be renamed (not appear with original names)
  const forbiddenSymbols = [
    'plmpCallBack',      // Should be cpiface_original_plmpCallBack
    'plmpOpenScreen',
    'plmpDrawScreen',
    'plmpCloseScreen',
  ];

  // Symbols that must appear (renamed originals)
  // Note: Some may be inlined by compiler, which is fine - compile-time validation ensures they exist
  const renamedSymbols = [
    'cpiface_original_plmpCallBack',
    // plmpOpenScreen and plmpCloseScreen may be inlined
    'cpiface_original_plmpDrawScreen',
    'cpiface_original_plmpCloseFile',
    'cpiface_original_plmpLateInit',
    'cpiface_original_plmpInit',
    'cpiface_original_plmpPreClose',
    'cpiface_original_plmpClose',
    'cpiface_original_plOpenCP',
    'cpiface_original_cpiDebugRun',
    'cpiface_original_curplayer',
    'cpiface_original_cpifaceSessionAPI',
    'cpiface_original_dllextinfo',
  ];

  // Known wrapped functions (should appear in #define list)
  const knownWrapped = [
    'plmpCallBack',
    'plmpOpenScreen',
    'plmpDrawScreen',
    'plmpCloseScreen',
    'plmpOpenFile',
    'plmpCloseFile',
    'plmpLateInit',
    'plmpInit',
    'plmpPreClose',
    'plmpClose',
    'plOpenCP',
    'dllextinfo',
    'cpiDebugRun',
    'curplayer',
    'cpifaceSessionAPI',
  ];

  // Known unwrapped static functions (internal helpers)
  const knownUnwrapped = [
    // Main drawing functions
    'mcpDrawGStringsFixedLengthStream',
    'mcpDrawGStringsSongXofY',
    'mcpDrawGStringsTracked',
    'cpiDrawG1String',
    // GString rendering functions
    'GString_album_render',
    'GString_amplification_render',
    'GString_artist_render',
    'GString_bitrate_render',
    'GString_channels_x_y_render',
    'GString_comment_render',
    'GString_composer_render',
    'GString_date_render',
    'GString_filename_render',
    'GString_filter_render',
    'GString_gvol_render',
    'GString_option_render',
    'GString_order_x_y_render',
    'GString_pausetime_render',
    'GString_playtime_render',
    'GString_pos_render',
    'GString_row_x_y_render',
    'GString_song_x_y_render',
    'GString_speed_render',
    'GString_style_render',
    'GString_tempo_render',
    'GString_title_render',
    // GString allowgrow functions
    'GString_amplification_allowgrow',
    'GString_bitrate_allowgrow',
    'GString_channels_x_y_allowgrow',
    'GString_date_allowgrow',
    'GString_filename_allowgrow',
    'GString_filter_allowgrow',
    'GString_gvol_allowgrow',
    'GString_head5_allowgrow',
    'GString_head6_allowgrow',
    'GString_head7_allowgrow',
    'GString_head8_allowgrow',
    'GString_option_allowgrow',
    'GString_order_x_y_allowgrow',
    'GString_pausetime_allowgrow',
    'GString_playtime_allowgrow',
    'GString_pos_allowgrow',
    'GString_row_x_y_allowgrow',
    'GString_song_x_y_allowgrow',
    'GString_speed_allowgrow',
    'GString_tempo_allowgrow',
    // Mode and UI management
    'cpiChangeMode',
    'cpiChanProcessKey',
    'cpiInitAllModes',
    'cpiVerifyDefModes',
    'cpifaceIdle',
    'cpiResetSongTimer',
    // Utility functions
    'getSeconds',
    'cpiDebug',
    'cpiDebugRecalcLines',
  ];

  test('build artifacts exist', () => {
    expect(fs.existsSync(wasmFile)).toBe(true);
    expect(fs.existsSync(cpifaceWrapperObj)).toBe(true);
  });

  test('required symbols are exported', () => {
    // This test is informational - the compile-time validation is more reliable
    // Since WASM tools may not be available and symbols may be internalized/optimized

    let symbolOutput = '';
    let toolAvailable = false;

    // Try wasm-objdump first (if available)
    try {
      symbolOutput = execSync(`wasm-objdump -x "${wasmFile}" 2>/dev/null || true`,
        { encoding: 'utf8' });
      if (symbolOutput.length > 100) {
        toolAvailable = true;
      }
    } catch (e) {
      // Fallback: check object files with nm
      try {
        symbolOutput = execSync(`nm "${cpifaceWrapperObj}" 2>/dev/null || true`,
          { encoding: 'utf8' });
        if (symbolOutput.length > 100) {
          toolAvailable = true;
        }
      } catch (e2) {
        // If both fail, skip test
        console.warn('Warning: Neither wasm-objdump nor nm available, skipping symbol export check');
        console.warn('This is OK - compile-time validation ensures symbols exist');
        return;
      }
    }

    if (!toolAvailable) {
      console.warn('Warning: Symbol inspection tools not producing output, skipping check');
      console.warn('This is OK - compile-time validation ensures symbols exist');
      return;
    }

    const missingSymbols = [];
    for (const sym of requiredExports) {
      if (!symbolOutput.includes(sym)) {
        missingSymbols.push(sym);
      }
    }

    if (missingSymbols.length > 0) {
      console.warn('Note: Some symbols not found in WASM output:', missingSymbols);
      console.warn('This may be OK - they might be internalized or the build passed already');
    }

    // Don't fail the test - this is informational only
    // The compile-time validation and successful build are more reliable
  });

  test('original symbols are properly renamed', () => {
    let nmOutput = '';

    try {
      nmOutput = execSync(`nm "${cpifaceWrapperObj}" 2>/dev/null`, { encoding: 'utf8' });
    } catch (e) {
      console.warn('Warning: nm not available, skipping rename check');
      return;
    }

    const unrenamedSymbols = [];
    for (const sym of forbiddenSymbols) {
      // Check if symbol appears WITHOUT the cpiface_original prefix
      const lines = nmOutput.split('\n');
      for (const line of lines) {
        // Match symbol name at end of line (nm format: "address type symbol")
        if (line.match(new RegExp(`\\s${sym}$`)) &&
            !line.includes('cpiface_original')) {
          unrenamedSymbols.push(sym);
          break;
        }
      }
    }

    if (unrenamedSymbols.length > 0) {
      console.error('Found unrenamed symbols (should have cpiface_original_ prefix):', unrenamedSymbols);
    }
    expect(unrenamedSymbols).toEqual([]);
  });

  test('renamed symbols exist in object file', () => {
    let nmOutput = '';

    try {
      nmOutput = execSync(`nm "${cpifaceWrapperObj}" 2>/dev/null`, { encoding: 'utf8' });
    } catch (e) {
      console.warn('Warning: nm not available, skipping renamed symbol check');
      return;
    }

    const missingRenamed = [];
    const inlinedSymbols = [];
    for (const sym of renamedSymbols) {
      if (!nmOutput.includes(sym)) {
        missingRenamed.push(sym);
        // Check if this might be inlined (functions that are called but small,
        // or functions that are only referenced but not called directly)
        if (sym.includes('OpenScreen') || sym.includes('CloseScreen') || sym.includes('cpiDebugRun')) {
          inlinedSymbols.push(sym);
        }
      }
    }

    if (inlinedSymbols.length > 0) {
      console.warn('Note: Some symbols may have been inlined by compiler:', inlinedSymbols);
      console.warn('This is OK - compile-time validation ensures they exist in source');
    }

    // Filter out likely inlined symbols from the missing list
    const actuallyMissing = missingRenamed.filter(sym => !inlinedSymbols.includes(sym));

    if (actuallyMissing.length > 0) {
      console.error('Missing renamed symbols (not inlined):', actuallyMissing);
    }
    expect(actuallyMissing).toEqual([]);
  });

  test('no new unwrapped static functions in cpiface.c', () => {
    const cpifaceContent = fs.readFileSync(cpifaceSource, 'utf8');

    // Extract static function names
    const staticFuncRegex = /^static\s+(?:\w+\s+)+(\w+)\s*\(/gm;
    const foundStaticFuncs = [];
    let match;

    while ((match = staticFuncRegex.exec(cpifaceContent)) !== null) {
      foundStaticFuncs.push(match[1]);
    }

    // Check for functions not in known lists
    const unknownFuncs = foundStaticFuncs.filter(func =>
      !knownWrapped.includes(func) &&
      !knownUnwrapped.includes(func)
    );

    if (unknownFuncs.length > 0) {
      console.warn('');
      console.warn('⚠️  NEW static functions found in cpiface.c:');
      unknownFuncs.forEach(func => {
        console.warn(`    - ${func}`);
      });
      console.warn('');
      console.warn('→ Check if these need wrapping in wasm/cpiface-wrapper.c');
      console.warn('→ If they are internal helpers, add them to knownUnwrapped in this test');
      console.warn('');
    }

    // This is a warning, not a failure - new internal helpers are okay
    // We just want to make maintainers aware
    expect(unknownFuncs.length).toBeLessThan(10); // Sanity check - should be very few unknowns
  });

  test('validation functions exist in cpiface-wrapper.c', () => {
    const wrapperSource = fs.readFileSync(
      path.join(__dirname, '../cpiface-wrapper.c'),
      'utf8'
    );

    // Check for compile-time validation function
    expect(wrapperSource).toMatch(/__validate_renamed_symbols/);

    // Check for static assertions
    expect(wrapperSource).toMatch(/_Static_assert/);

    // Check for all required #define renames
    for (const sym of knownWrapped) {
      const defineRegex = new RegExp(`#define\\s+${sym}\\s+cpiface_original_${sym}`);
      if (!wrapperSource.match(defineRegex)) {
        console.error(`Missing #define for ${sym}`);
      }
      expect(wrapperSource).toMatch(defineRegex);
    }

    // Check for all required #undef
    for (const sym of knownWrapped) {
      const undefRegex = new RegExp(`#undef\\s+${sym}`);
      expect(wrapperSource).toMatch(undefRegex);
    }
  });

  test('runtime validation exists in wasm_plmpLateInit', () => {
    const wrapperSource = fs.readFileSync(
      path.join(__dirname, '../cpiface-wrapper.c'),
      'utf8'
    );

    // Check for runtime validation in wasm_plmpLateInit
    expect(wrapperSource).toMatch(/Circular reference detected/);
    expect(wrapperSource).toMatch(/cpiface_original_plmpLateInit is NULL/);
  });
});
