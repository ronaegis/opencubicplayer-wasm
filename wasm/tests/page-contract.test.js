const fs = require('fs');
const path = require('path');

describe('shipped page contract', () => {
  const page = fs.readFileSync(path.join(__dirname, '../index.html'), 'utf8');

  test('names one asset version', () => {
    expect(page).toMatch(/var OCP_ASSET_VERSION = '\d+'/);
    expect(page).not.toMatch(/\?v=\d+/);
    expect(page).toMatch(/OCP_ASSET_VERSION/);
  });

  test('shows a persistent startup error and a non-fatal notice', () => {
    expect(page).toMatch(/function\s*\(message\)\s*\{[\s\S]*ocpFatalShown = true/);
    expect(page).toMatch(/window\.ocpShowFatalError\s*=/);
    expect(page).toMatch(/window\.ocpShowNotice\s*=/);
    expect(page).toMatch(/45000/);
  });

  test('says the canvas is desktop-sized', () => {
    expect(page).toMatch(/id="desktop-notice"/);
    expect(page).toMatch(/640 pixels wide/);
    expect(page).toMatch(/no touch keyboard/);
  });

  test('discloses the Modland host', () => {
    expect(page).toMatch(/id="modland-notice"/);
    expect(page).toMatch(/third-party host modland\.com/);
  });

  test('waits for ocp.ini before starting playback', () => {
    expect(page).toMatch(/_wasm_startup_is_complete/);
    expect(page).toMatch(/did not finish reading ocp\.ini/);
  });
});
