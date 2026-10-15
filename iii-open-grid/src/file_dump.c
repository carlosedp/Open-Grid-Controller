// og_dump_file(name) prints a file as base64 lines, then "size crc32" (CRC-32,
// 8 hex digits) on a line of its own, and returns size, crc32.
//
// cat() needs the whole file in RAM (a 29 KB script can fail to load while a
// big script runs) and passes it through print(), where the host can't tell
// '\r', a trailing '\0' or a lost chunk from the real content. Here the file
// is read in small pieces into static buffers, base64 keeps every byte, and
// the size and checksum let the host check it got everything (serial output
// is dropped when the host stops reading).

#include "file_dump.h"

#include <stdint.h>

#include "fs.h"
#include "lauxlib.h"
#include "serial.h"

// 96 bytes -> 128 base64 characters per line
#define DUMP_CHUNK 96

static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, uint32_t len) {
  crc = ~crc;
  while (len--) {
    crc ^= *data++;
    for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
  }
  return ~crc;
}

static uint32_t base64_encode(const uint8_t *in, uint32_t len, char *out) {
  char *o = out;
  uint32_t i = 0;
  for (; i + 2 < len; i += 3) {
    const uint32_t v = ((uint32_t)in[i] << 16) | ((uint32_t)in[i + 1] << 8) | in[i + 2];
    *o++ = b64[(v >> 18) & 63];
    *o++ = b64[(v >> 12) & 63];
    *o++ = b64[(v >> 6) & 63];
    *o++ = b64[v & 63];
  }
  if (i < len) {
    const uint32_t v = ((uint32_t)in[i] << 16) | (i + 1 < len ? (uint32_t)in[i + 1] << 8 : 0);
    *o++ = b64[(v >> 18) & 63];
    *o++ = b64[(v >> 12) & 63];
    *o++ = (i + 1 < len) ? b64[(v >> 6) & 63] : '=';
    *o++ = '=';
  }
  return (uint32_t)(o - out);
}

int og_l_dump_file(lua_State *l) {
  const char *path = luaL_checkstring(l, 1);
  // static: keeps ~400 bytes off core 0's small C stack while Lua runs
  static lfs_file_t file;
  static uint8_t chunk[DUMP_CHUNK];
  static char line[DUMP_CHUNK / 3 * 4 + 2];

  if (fs_file_open(&file, path, LFS_O_RDONLY) != 0) {
    lua_pushnil(l);
    lua_pushfstring(l, "%s: not found", path);
    return 2;
  }

  uint32_t size = 0;
  uint32_t crc = 0;
  int32_t n;
  while ((n = fs_file_read(&file, chunk, DUMP_CHUNK)) > 0) {
    size += (uint32_t)n;
    crc = crc32_update(crc, chunk, (uint32_t)n);
    uint32_t len = base64_encode(chunk, (uint32_t)n, line);
    line[len++] = '\n';
    serial_tx((const uint8_t *)line, len);
  }
  fs_file_close(&file);

  serial("%lu %08lx\r\n", (unsigned long)size, (unsigned long)crc);
  lua_pushinteger(l, (lua_Integer)size);
  lua_pushinteger(l, (lua_Integer)crc);
  return 2;
}
