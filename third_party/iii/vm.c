#include <string.h>

#include "pico/time.h"
#include "hardware/watchdog.h"

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

#include "device.h"
#include "fs.h"
#include "help.h"
#include "iii.h"
#include "metro.h"
#include "midi.h"
#include "serial.h"
#include "vm.h"

#include "resource/lib_lua.h"

lua_State *L = NULL;

#define PRINT_TYPES 0

static void print_stack_pos(lua_State *l, int pos, const char *prefix,
                            bool newline) {
  if (lua_isinteger(l, pos)) {
    const int64_t i64 = lua_tointeger(l, pos);
#if PRINT_TYPES
    serial("%sinteger: %lld", prefix, i64);
#else
    serial("%lld", i64);
#endif
  } else if (lua_isnumber(l, pos)) {
    double d = lua_tonumber(l, pos);
#if PRINT_TYPES
    serial("%snumber: %f", prefix, d);
#else
    serial("%f", d);
#endif
  }
  // NB: we don't want to run this check first,
  // because it actually returns true for numbers and integers as well
  else if (lua_isstring(l, pos)) {
    const char *str = lua_tostring(l, pos);
#if PRINT_TYPES
    serial("%sstring: %s", prefix, str);
#else
    serial("%s", str);
#endif
  } else if (lua_isboolean(l, pos)) {
    const int b = lua_toboolean(l, pos);
#if PRINT_TYPES
    serial("%s%s", prefix, b ? "true" : "false");
#else
    serial("%s", b ? "true" : "false");
#endif
  } else if (lua_isuserdata(l, pos)) {
    const void *p = lua_topointer(l, pos);
    serial("%suserdata: 0x%p", prefix, p);
  } else if (lua_islightuserdata(l, pos)) {
    const void *p = lua_topointer(l, pos);
    serial("%slightuserdata: 0x%p", prefix, p);
  } else if (lua_isfunction(l, pos)) {
    const void *p = lua_topointer(l, pos);
    serial("%sfunction: 0x%p", prefix, p);
  } else if (lua_isnil(l, pos)) {
    serial("%snil", prefix);
  } else if (lua_istable(l, pos)) {
    const void *p = lua_topointer(l, pos);
    serial("%stable: 0x%p\r\n", prefix, p);
    // FIXME: nested tables fail with hard crash
    /*lua_pushnil(l);
    while (lua_next(l, pos) != 0) {
      print_stack_pos(l, -2, "  (key) ", false);
      serial("\t\t");
      print_stack_pos(l, -1, "  (val) ", true);
      lua_pop(l, 1);
    }*/
  } else if (lua_isthread(l, pos)) {
    const void *p = lua_topointer(l, pos);
    serial("%sthread: 0x%p", prefix, p);
  } else {
    serial("%s(unknown type)", prefix);
  }
  if (newline)
    serial("\r\n");
}

static int l_my_print(lua_State *l) {
  int nargs = lua_gettop(l);
  for (int i = 1; i <= nargs; i++) {
    print_stack_pos(l, i, "", true);
  }
  return 0;
}

int l_report(lua_State *L, int status) {
  if (status != LUA_OK) {
    const char *msg = lua_tostring(L, -1);
    serial(msg);
    lua_pop(L, 1);
  }
  return status;
}

static int msghandler(lua_State *L) {
  const char *msg = lua_tostring(L, 1);
  if (msg == NULL) {                         // is error object not a string?
    if (luaL_callmeta(L, 1, "__tostring") && // does it have a * metamethod
        (lua_type(L, -1) == LUA_TSTRING)) {  // that produces a string?
      return 1;                              // that is the message
    } else {
      msg = lua_pushfstring(L, "(error object is a %s value)",
                            luaL_typename(L, 1));
    }
  }
  luaL_traceback(L, L, msg, 1); // append a standard traceback
  return 1;                     // return the traceback
}

int docall(lua_State *L, int narg, int nres) {
  int status;
  int base = lua_gettop(L) - narg;  // function index
  lua_pushcfunction(L, msghandler); // push message handler
  lua_insert(L, base);              // put it under function and args
  status = lua_pcall(L, narg, nres, base);
  lua_remove(L, base); // remove message handler from the stack
  return status;
}

//////////////////////////////////////////////////////////////////

static int l_help(lua_State *l) {
  (void)l;
  serial(device_help_txt());
  serial(help_txt);
  return 0;
}

static int l_dostring(lua_State *l) {
  const char *str = lua_tostring(l, 1);
  int res = luaL_dostring(L, str);
  if (res != LUA_OK) {
    const char *err = lua_tostring(L, -1);
    serial("%s", (const uint8_t *)err);
  }
  return 0;
}

static int l_midi_tx(lua_State *l) {
  // TODO: check if arg1 is table, unpack if so
  // default vals for all here so we don't need to do it in lua
  uint8_t data1 = (uint8_t)lua_tointeger(l, 1);
  uint8_t data2 = (uint8_t)lua_tointeger(l, 2);
  uint8_t data3 = (uint8_t)lua_tointeger(l, 3);
  midi_tx(data1, data2, data3);
  return 0;
}

//--- midi helpers: these used to be lua functions in lib.lua; the api is the
//--- same, but C is ~10x faster and allocates nothing beyond the result

// `v or def` for an optional integer argument
static lua_Integer opt_int(lua_State *l, int idx, lua_Integer def) {
  return lua_toboolean(l, idx) ? lua_tointeger(l, idx) : def;
}

static void midi_tx_shortcut(lua_State *l, uint8_t status) {
  // midi_tx((ch or 1)-1 + status, note or 66, vel or 100), and same for cc
  midi_tx((uint8_t)(opt_int(l, 3, 1) - 1 + status), (uint8_t)opt_int(l, 1, 66),
          (uint8_t)opt_int(l, 2, 100));
}

static int l_midi_note_on(lua_State *l) {
  midi_tx_shortcut(l, 0x90);
  return 0;
}

static int l_midi_note_off(lua_State *l) {
  midi_tx_shortcut(l, 0x80);
  return 0;
}

static int l_midi_cc(lua_State *l) {
  midi_tx_shortcut(l, 0xb0);
  return 0;
}

// midi_tx_many({b1, b2, b3, b1, b2, b3, ...}): one midi_tx per 3 bytes
static int l_midi_tx_many(lua_State *l) {
  const lua_Integer n = luaL_len(l, 1);
  for (lua_Integer i = 0; n > 0 && i <= (n - 1) / 3; i++) {
    uint8_t b[3];
    for (int k = 0; k < 3; k++) {
      lua_geti(l, 1, 3 * i + k + 1);
      b[k] = (uint8_t)lua_tointeger(l, -1);
      lua_pop(l, 1);
    }
    midi_tx(b[0], b[1], b[2]);
  }
  return 0;
}

// msg[key] = data[i], as is (nil leaves the field out)
static void msg_copy(lua_State *l, int i, const char *key) {
  lua_geti(l, 1, i);
  lua_setfield(l, -2, key);
}

static void msg_int(lua_State *l, const char *key, lua_Integer v) {
  lua_pushinteger(l, v);
  lua_setfield(l, -2, key);
}

static lua_Integer data_int(lua_State *l, int i) {
  lua_geti(l, 1, i);
  const lua_Integer v = lua_tointeger(l, -1);
  lua_pop(l, 1);
  return v;
}

// midi_to_msg({b1, b2, b3}) -> {type = "note_on", note = .., vel = .., ch = ..}
static int l_midi_to_msg(lua_State *l) {
  luaL_checktype(l, 1, LUA_TTABLE);
  const lua_Integer d1 = data_int(l, 1);
  const lua_Integer ch = (d1 & 0x0f) + 1;
  const char *type = NULL;
  lua_createtable(l, 0, 4);

  switch (d1 & 0xf0) {
  case 0x90: {
    msg_copy(l, 2, "note");
    msg_copy(l, 3, "vel");
    msg_int(l, "ch", ch);
    // velocity zero means note off
    type = data_int(l, 3) > 0 ? "note_on" : "note_off";
    break;
  }
  case 0x80:
    type = "note_off";
    msg_copy(l, 2, "note");
    msg_copy(l, 3, "vel");
    msg_int(l, "ch", ch);
    break;
  case 0xb0:
    type = "cc";
    msg_copy(l, 2, "cc");
    msg_copy(l, 3, "val");
    msg_int(l, "ch", ch);
    break;
  case 0xe0:
    type = "pitchbend";
    msg_int(l, "val", data_int(l, 2) + (data_int(l, 3) << 7));
    msg_int(l, "ch", ch);
    break;
  case 0xa0:
    type = "key_pressure";
    msg_copy(l, 2, "note");
    msg_copy(l, 3, "val");
    msg_int(l, "ch", ch);
    break;
  case 0xd0:
    type = "channel_pressure";
    msg_copy(l, 2, "val");
    msg_int(l, "ch", ch);
    break;
  case 0xc0:
    type = "program_change";
    msg_copy(l, 2, "val");
    msg_int(l, "ch", ch);
    break;
  default:
    switch (d1) {
    case 0xfa: type = "start"; break;
    case 0xfc: type = "stop"; break;
    case 0xfb: type = "continue"; break;
    case 0xf8: type = "clock"; break;
    case 0xf2:
      type = "song_position";
      msg_copy(l, 2, "lsb");
      msg_copy(l, 3, "msb");
      break;
    case 0xf3:
      type = "song_select";
      msg_copy(l, 2, "val");
      break;
    case 0xfe: // active sensing: empty message
      break;
    default:
      type = d1 == 0xf0 ? "sysex" : "other";
      lua_pushvalue(l, 1);
      lua_setfield(l, -2, "raw");
      break;
    }
  }

  if (type != NULL) {
    lua_pushstring(l, type);
    lua_setfield(l, -2, "type");
  }
  return 1;
}

static int l_metro_set(lua_State *l) {
  int nargs = lua_gettop(l);
  int index = (int)lua_tointeger(l, 1) - 1;
  double s = (double)lua_tonumber(l, 2);
  if (index < 0 || index > METRO_COUNT - 1)
    return 0;
  if (nargs == 3) {
    int count = (int)lua_tointeger(l, 3);
    metro_set_with_count(index, s, count);
  } else {
    metro_set(index, s);
  }
  return 0;
}

static int l_get_time(lua_State *l) {
  uint64_t us = to_us_since_boot(get_absolute_time());
  lua_pushnumber(l, (lua_Number)((double)us / 1000000.0));
  return 1;
}

static int l_device_id(lua_State *l) {
  lua_pushstring(l, device_id());
  return 1;
}

static int l_device_version(lua_State *l) {
  lua_pushstring(l, device_version());
  return 1;
}

static int l_device_reset(lua_State *l) {
  (void)l;
  iii_reset();
  return 0;
}

static int l_fs_read_file(lua_State *l) {
  const char *path = lua_tostring(l, 1);
  lfs_file_t file;
  const int flags = LFS_O_RDONLY;
  if (fs_file_open(&file, path, flags) == 0) {
    const unsigned int sz = (unsigned int)fs_file_size(&file);
    char *buf = malloc(sz + 1);
    buf[sz] = 0;
    fs_file_read(&file, buf, sz);
    fs_file_close(&file);
    lua_pushlstring(l, buf, sz);
    free(buf);
  } else {
    lua_pushboolean(l, 0);
  }
  return 1;
}

// feeds a file to lua_load() in small chunks, so compiling a script
// doesn't also need a full copy of its source in RAM
#define FILE_READER_CHUNK 256
typedef struct {
  lfs_file_t file;
  bool done;
  char buf[FILE_READER_CHUNK];
} file_reader_t;

// files uploaded through the repl end with a '\0' (see REPL_CMD_END_WRITE);
// stop there, as luaL_dostring() on the whole file did
static const char *file_reader(lua_State *l, void *ud, size_t *size) {
  (void)l;
  file_reader_t *r = (file_reader_t *)ud;
  *size = 0;
  if (r->done)
    return NULL;
  const int32_t n = fs_file_read(&r->file, r->buf, FILE_READER_CHUNK);
  if (n <= 0) {
    r->done = true;
    return NULL;
  }
  const char *nul = memchr(r->buf, '\0', (size_t)n);
  if (nul != NULL) {
    r->done = true;
    *size = (size_t)(nul - r->buf);
  } else {
    *size = (size_t)n;
  }
  return r->buf;
}

#define VM_FILE_NOT_FOUND (-1)

// like luaL_dostring() on the file's contents: compile and run it, leaving
// its results (or an error message) on the stack. returns a lua status, or
// VM_FILE_NOT_FOUND (nothing pushed) if the file can't be opened.
// compile the file at `path` (named `name` in error messages), leaving the
// function or an error message on the stack. returns a lua status, or
// VM_FILE_NOT_FOUND (nothing pushed) if the file can't be opened.
static int vm_loadfile(lua_State *l, const char *path, const char *name) {
  if (path == NULL)
    return VM_FILE_NOT_FOUND;
  // chunk name as a lua string rather than a buffer on the small C stack;
  // "@" makes errors read "file.lua:12:"
  lua_pushfstring(l, "@%s", name);
  file_reader_t *r = malloc(sizeof(file_reader_t));
  if (r == NULL) {
    lua_pop(l, 1);
    lua_pushliteral(l, "not enough memory");
    return LUA_ERRMEM;
  }
  if (fs_file_open(&r->file, path, LFS_O_RDONLY) != 0) {
    free(r);
    lua_pop(l, 1);
    return VM_FILE_NOT_FOUND;
  }
  r->done = false;
  int res = lua_load(l, file_reader, r, lua_tostring(l, -1), NULL);
  fs_file_close(&r->file);
  free(r);
  lua_remove(l, -2); // the chunk name, under the function or error message
  return res;
}

// like vm_test_script(), for a file: compile it, print the result
bool vm_test_file(const char *path, const char *name) {
  int res = vm_loadfile(L, path, name);
  if (res == VM_FILE_NOT_FOUND) {
    serial("-- lua error:\r\ncan't open %s\r\n", path);
    return false;
  }
  if (res != LUA_OK) {
    serial("-- lua error:\r\n%s\r\n", lua_tostring(L, -1));
    lua_pop(L, 1);
    return false;
  }
  serial("-- compiled ok\r\n");
  lua_pop(L, 1); // the compiled chunk
  return true;
}

static int vm_dofile(lua_State *l, const char *path) {
  int res = vm_loadfile(l, path, path);
  if (res == LUA_OK)
    res = lua_pcall(l, 0, LUA_MULTRET, 0);
  return res;
}

// read an entire file from the filesystem,
// interpret it as lua code, returning the result
static int l_fs_run_file(lua_State *l) {
  const char *path = lua_tostring(l, 1);
  int res = vm_dofile(L, path);
  if (res == VM_FILE_NOT_FOUND) {
    lua_pushnil(l);
    return 0;
  }
  if (res != LUA_OK) {
    // TODO: handle errors...
    const char *err = lua_tostring(L, -1);
    serial("%s\n", (const uint8_t *)err);
  }
  return 1;
}

static int l_fs_write_file(lua_State *l) {
  const char *path = lua_tostring(l, 1);
  const char *data = lua_tostring(l, 2);

  const int flags = LFS_O_CREAT | LFS_O_WRONLY;
  lfs_file_t file;
  fs_remove(path); // ensure we get zero termination
  if (fs_file_open(&file, path, flags) == 0) {
    fs_file_write(&file, data, strlen(data));
    fs_file_close(&file);
    lua_pushboolean(l, 1);
  } else {
    lua_pushboolean(l, 0);
  }
  return 1;
}

static int l_fs_remove_file(lua_State *l) {
  const char *path = lua_tostring(l, 1);
  if (fs_remove(path) == 0) {
    lua_pushboolean(l, 1);
  } else {
    lua_pushboolean(l, 0);
  }
  return 1;
}

static int l_fs_list_files(lua_State *l) {
  uint32_t num_files;
  char **files = fs_get_file_list(&num_files);
  lua_createtable(l, num_files, 0);
  for (uint32_t i = 0; i < num_files; i++) {
    lua_pushstring(l, files[i]);
    free(files[i]);
    lua_rawseti(l, -2, (lua_Integer)(i + 1));
  }
  free(files);
  return 1;
}

int l_fs_file_size(lua_State *l) {
  const char *path = lua_tostring(l, 1);
  struct lfs_info st;
  if (fs_stat(path, &st) == LFS_ERR_OK) {
    lua_pushinteger(l, (lua_Integer)st.size);
    return 1;
  }
  return 0;
}

int l_fs_free_space(lua_State *l) {
  lua_pushinteger(l, (lua_Integer)fs_get_free_space());
  return 1;
}

int l_fs_reformat(lua_State *l) {
  (void)l;
  fs_reformat();
  return 0;
}

//////////////////////////////////////////////////////////////////

static const struct luaL_Reg main_lib[] = {{"help", l_help},
                                           {"print", l_my_print},
                                           {"dostring", l_dostring},
                                           {"midi_tx", l_midi_tx},
                                           {"midi_tx_many", l_midi_tx_many},
                                           {"midi_to_msg", l_midi_to_msg},
                                           {"midi_note_on", l_midi_note_on},
                                           {"midi_note_off", l_midi_note_off},
                                           {"midi_cc", l_midi_cc},
                                           {"metro_set", l_metro_set},
                                           {"get_time", l_get_time},
                                           {"device_id", l_device_id},
                                           {"device_version", l_device_version},
                                           {"device_reset", l_device_reset},

                                           {"fs_read_file", l_fs_read_file},
                                           {"fs_run_file", l_fs_run_file},
                                           {"fs_write_file", l_fs_write_file},
                                           {"fs_remove_file", l_fs_remove_file},
                                           {"fs_list_files", l_fs_list_files},
                                           {"fs_file_size", l_fs_file_size},
                                           {"fs_free_space", l_fs_free_space},
                                           {"fs_reformat", l_fs_reformat},

                                           {NULL, NULL}};

// collector pause, in percent of the heap left after the previous collection
#define VM_GC_PAUSE 120

// on failure just return NULL: lua then runs an emergency full collection
// and retries, and only if that fails too raises a "not enough memory"
// error, which reaches the script like any other error
void *my_alloc(void *ud, void *ptr, size_t osize, size_t nsize) {
  (void)ud;
  (void)osize;
  if (nsize == 0) {
    free(ptr);
    return NULL;
  }
  return realloc(ptr, nsize);
}

// errors outside any protected call (e.g. out of memory while starting up)
// can't be recovered from; report and restart rather than hang
static int vm_panic(lua_State *l) {
  const char *msg = lua_tostring(l, -1);
  serial("-- lua panic: %s\r\n", msg ? msg : "?");
  watchdog_reboot(0, 0, 1000);
  return 0;
}

void vm_init(bool run_script) {
  // L = luaL_newstate();
  L = lua_newstate(my_alloc, NULL, luaL_makeseed(NULL));
  if (L == NULL) {
    serial("vm_init fail!\r\n");
    return;
  }
  lua_atpanic(L, vm_panic);

  // lua's default pause (200%) waits until the heap has doubled since the
  // last collection before starting another; with a large script loaded
  // that is more than all of RAM. start the next cycle at 120% instead.
  lua_gc(L, LUA_GCPARAM, LUA_GCPPAUSE, VM_GC_PAUSE);

  luaL_openlibs(L);

  lua_getglobal(L, "_G");
  luaL_setfuncs(L, main_lib, 0);
  luaL_setfuncs(L, get_device_lib(), 0);
  lua_pop(L, 1);

  bool write_lib = false;

  // test if lib.lua present, write it if not
  lfs_file_t f;
  if (fs_file_open(&f, "lib.lua", LFS_O_RDONLY) != 0) {
    serial("-- lib.lua not found\r\n");
    write_lib = true;
  } else {
    char buf[24 + 1]; // +1: strstr() needs a terminated string
    const int32_t n = fs_file_read(&f, buf, 24);
    buf[n > 0 ? n : 0] = '\0';
    fs_file_close(&f);
    if(strstr(buf,III_VERSION)==NULL) {
      serial("-- lib.lua wrong version\r\n");
      write_lib=true;
    }
  }

  if (write_lib) {
    serial("-- init: writing lib.lua\r\n");
    // const unsigned int sz = (unsigned int)strlen(lua_init_code());
    const unsigned int sz = (unsigned int)strlen((const char *)lib_lua_data);
    char *buf = malloc(sz + 1);
    // strcpy(buf,lua_init_code());
    strcpy(buf, (const char *)lib_lua_data);
    buf[sz] = 0;
    lfs_file_t file;
    if (fs_file_open(&file, "lib.lua", LFS_O_CREAT | LFS_O_WRONLY) == 0) {
      fs_file_write(&file, buf, sz);
      fs_file_close(&file);
    } else {
      serial("-- init: writing lib.lua failed!!\r\n");
    }
    free(buf);
  }

  if (run_script) {
    // run lib.lua
    static struct lfs_info info; // ~260 bytes: keep it off the small C stack
    int res = VM_FILE_NOT_FOUND;
    if (fs_stat("lib.lua", &info) == LFS_ERR_OK) {
      serial("-- init: running lib\r\n");
      res = vm_dofile(L, "lib.lua");
      if (res != LUA_OK && res != VM_FILE_NOT_FOUND) {
        const char *err = lua_tostring(L, -1);
        serial("-- init: lib errors!\r\n%s\r\n", (const uint8_t *)err);
      }
    }
    if (res == VM_FILE_NOT_FOUND) {
      serial("-- init: lib fail!\r\n");
    }

    lua_gc(L, LUA_GCCOLLECT);

    // run init.lua if present
    const char *path = "init.lua";
    if (fs_stat(path, &info) == LFS_ERR_OK) {
      serial("-- init: running script\r\n");
      res = vm_dofile(L, path);
      if (res != LUA_OK && res != VM_FILE_NOT_FOUND) {
        serial("-- init: error running script\r\n");
        // TODO: handle errors better/differently?
        const char *err = lua_tostring(L, -1);
        serial("%s\r\n", (const uint8_t *)err);
      }
    } else {
      serial("-- init: no script\r\n");
    }
  } else {
    serial("-- init: skip script\r\n");
  }
}

void vm_deinit() {
  if (L == NULL) {
    return;
  }
  metro_cleanup();
  lua_close(L);
}

// like vm_test_file(), for source already in RAM
bool vm_test_buffer(const char *buf, size_t len, const char *name) {
  lua_pushfstring(L, "@%s", name); // errors read "file.lua:12:"
  int res = luaL_loadbuffer(L, buf, len, lua_tostring(L, -1));
  lua_remove(L, -2);
  if (res != LUA_OK) {
    serial("-- lua error:\r\n%s\r\n", lua_tostring(L, -1));
    lua_pop(L, 1);
    return false;
  }
  serial("-- compiled ok\r\n");
  lua_pop(L, 1); // the compiled chunk
  return true;
}

bool vm_test_script(const char *buf) {
  int res = luaL_loadstring(L, buf);
  if (res != LUA_OK) {
    serial("-- lua error:\r\n");
    const char *err = lua_tostring(L, -1);
    // skip the '[string "..."]:' prefix; other messages (e.g. out of memory)
    // have none, so print those whole
    const char *quote = (err && strlen(err) > 10) ? strchr(err + 10, '"') : NULL;
    if (quote != NULL && strlen(quote) >= 3)
      err = quote + 3;
    serial("%s\r\n", (const uint8_t *)err);
    // pop the error message
    lua_pop(L, 1);
    return false;
  } else {
    serial("-- compiled ok\r\n");
    // pop the compiled chunk
    /// (FIXME: should we be saving it instead?)
    lua_pop(L, 1);
    return true;
  }
}

bool vm_run_buffer(const char *buf) {
  int res = luaL_dostring(L, buf);
  if (res != LUA_OK) {
    const char *err = lua_tostring(L, -1);
    serial("-- lua error:\r\n%s", (const uint8_t *)err);
    // pop the error message
    lua_pop(L, 1);
    return false;
  } else {
    return true;
  }
}

//////////////////////////////////////////////////////////////////

void vm_handle_midi(uint8_t data1, uint8_t data2, uint8_t data3) {
  if (L == NULL) {
    return;
  }
  lua_getglobal(L, "event_midi");
  if (lua_isnil(L, -1)) {
    //serial("event_midi: %d %d %d\r\n", data1, data2, data3);
    lua_pop(L, 1);
    return;
  }
  lua_pushinteger(L, data1);
  lua_pushinteger(L, data2);
  lua_pushinteger(L, data3);
  l_report(L, docall(L, 3, 0));
}

void vm_handle_metro(int index, int count) {
  if (L == NULL)
    return;
  lua_getglobal(L, "metro_event");
  if (lua_isnil(L, -1)) {
    lua_pop(L, 1);
    return;
  }
  lua_pushinteger(L, index + 1);
  lua_pushinteger(L, count);
  l_report(L, docall(L, 2, 0));
}
