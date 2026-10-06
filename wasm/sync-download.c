/* OpenCP Module Player - WASM synchronous download helper
 * Provides a blocking helper for legacy code paths that expect
 * synchronous downloads. Uses a synchronous XMLHttpRequest to
 * fetch the resource into MEMFS.
 */

#include <emscripten.h>
#include <emscripten/emscripten.h>
#include <string.h>

EM_JS(int, wasm_sync_download_js, (const char *url, const char *dest_path), {
  var u = UTF8ToString(url);
  var p = UTF8ToString(dest_path);
  try {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', u, false);
    try {
      xhr.overrideMimeType('text/plain; charset=x-user-defined');
    } catch (e) {
      // overrideMimeType may not exist; ignore
    }
    xhr.send(null);
    if ((xhr.status >= 200 && xhr.status < 300) || xhr.status === 0) {
      var data = null;
      if (xhr.response && xhr.response.byteLength !== undefined) {
        data = new Uint8Array(xhr.response);
      } else if (typeof xhr.responseText === 'string') {
        var text = xhr.responseText;
        data = new Uint8Array(text.length);
        for (var i = 0; i < text.length; i++) {
          data[i] = text.charCodeAt(i) & 0xFF;
        }
      } else {
        console.error('wasm_sync_download_js: unsupported response type for', u);
        return -1;
      }
      try {
        var dir = PATH.dirname(p);
        FS.mkdirTree(dir);
      } catch (e) {
        // Directory may already exist; ignore errors from mkdirTree
      }
      FS.writeFile(p, data, { canOwn: true });
      return 0;
    }
    console.error('wasm_sync_download_js: HTTP status', xhr.status, 'for', u);
    return xhr.status || -1;
  } catch (e) {
    console.error('wasm_sync_download_js exception', e);
    return -1;
  }
});

EMSCRIPTEN_KEEPALIVE
int wasm_sync_download_to_file(const char *url, const char *dest_path)
{
  extern void idbfs_mark_dirty_data(void);
  if (!url || !dest_path || !*url || !*dest_path) {
    return -1;
  }
  int rc = wasm_sync_download_js(url, dest_path);
  if (rc == 0) {
    idbfs_mark_dirty_data();
  }
  return rc;
}
