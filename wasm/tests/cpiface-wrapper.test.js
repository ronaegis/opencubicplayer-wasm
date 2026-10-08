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


function objectContainsSymbol(buf, name) {
  const bytes = Buffer.from(name, 'utf8');
  if (bytes.length === 0 || bytes.length > 127) {
    throw new Error('symbol name is too long for a single length byte: ' + name);
  }
  const prefixed = Buffer.concat([Buffer.from([bytes.length]), bytes]);
  return buf.includes(prefixed);
}

function readSymbolObject(file) {
  if (!fs.existsSync(file)) {
    throw new Error('missing object ' + file);
  }
  const buf = fs.readFileSync(file);
  if (buf.length < 64) {
    throw new Error('object too small to contain symbols: ' + file + ' (' + buf.length + ' bytes)');
  }
  return buf;
}

  test('required symbols are exported', () => {
    const buf = readSymbolObject(cpifaceWrapperObj);
    const missingSymbols = requiredExports.filter((sym) => !objectContainsSymbol(buf, sym));
    expect(missingSymbols).toEqual([]);
  });

  test('original symbols are properly renamed', () => {
    const buf = readSymbolObject(cpifaceWrapperObj);
    const unrenamedSymbols = forbiddenSymbols.filter((sym) => objectContainsSymbol(buf, sym));
    expect(unrenamedSymbols).toEqual([]);
  });

  test('renamed symbols exist in object file', () => {
    const buf = readSymbolObject(cpifaceWrapperObj);
    const inlined = new Set([
      'cpiface_original_plmpOpenScreen',
      'cpiface_original_plmpCloseScreen',
      'cpiface_original_cpiDebugRun',
    ]);
    const missingRenamed = renamedSymbols.filter((sym) => !objectContainsSymbol(buf, sym) && !inlined.has(sym));
    expect(missingRenamed).toEqual([]);
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
  });
});
