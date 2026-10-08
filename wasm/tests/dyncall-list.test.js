const fs = require('fs');
const path = require('path');

describe('dynCall signature list', () => {
  const cmake = fs.readFileSync(path.join(__dirname, '../CMakeLists.txt'), 'utf8');
  const page = fs.readFileSync(path.join(__dirname, '../index.html'), 'utf8');

  test('every cmake dynCall signature is copied into index.html', () => {
    const cmakeMatch = cmake.match(/EXPORTED_RUNTIME_METHODS='\[(.*?)\]'/);
    expect(cmakeMatch).not.toBeNull();
    const cmakeSigs = [...cmakeMatch[1].matchAll(/dynCall_([a-z]+)/g)].map((hit) => hit[1]);
    expect(cmakeSigs.length).toBeGreaterThan(0);

    const pageMatch = page.match(/const signatures = \[([\s\S]*?)\];/);
    expect(pageMatch).not.toBeNull();
    const pageSigs = new Set([...pageMatch[1].matchAll(/'([a-z]+)'/g)].map((hit) => hit[1]));

    const missing = cmakeSigs.filter((sig) => !pageSigs.has(sig));
    expect(missing).toEqual([]);
  });
});
