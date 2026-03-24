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
  lua_pushnumber(l, (double)((double)us / 1000000.0));
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

// read an entire file from the filesystem,
// interpret it as lua code, returning the result
static int l_fs_run_file(lua_State *l) {
  const char *path = lua_tostring(l, 1);
  const int flags = LFS_O_RDONLY;
  lfs_file_t file;
  if (fs_file_open(&file, path, flags) == 0) {
    const unsigned int sz = (unsigned int)fs_file_size(&file);
    char *buf = malloc(sz + 1);
    if (buf == NULL)
      serial("malloc fail!\r\n");
    buf[sz] = 0;
    fs_file_read(&file, buf, sz);
    fs_file_close(&file);
    //int top = lua_gettop(L);
    int res = luaL_dostring(L, buf);
    free(buf);
    if (res != LUA_OK) {
      // TODO: handle errors...
      const char *err = lua_tostring(L, -1);
      serial("%s\n", (const uint8_t *)err);
    }
    //return lua_gettop(L) - top;
    return 1;
  } else {
    // file not found
    lua_pushnil(l);
  }
  return 0;
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
    lua_rawseti(l, -2, i + 1);
  }
  free(files);
  return 1;
}

int l_fs_file_size(lua_State *l) {
  const char *path = lua_tostring(l, 1);
  struct lfs_info st;
  if (fs_stat(path, &st) == LFS_ERR_OK) {
    lua_pushinteger(l, st.size);
    return 1;
  }
  return 0;
}

int l_fs_free_space(lua_State *l) {
  lua_pushinteger(l, fs_get_free_space());
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
                                           {"metro_set", l_metro_set},
                                           {"get_time", l_get_time},
                                           {"device_id", l_device_id},
                                           {"device_version", l_device_version},

                                           {"fs_read_file", l_fs_read_file},
                                           {"fs_run_file", l_fs_run_file},
                                           {"fs_write_file", l_fs_write_file},
                                           {"fs_remove_file", l_fs_remove_file},
                                           {"fs_list_files", l_fs_list_files},
                                           {"fs_file_size", l_fs_file_size},
                                           {"fs_free_space", l_fs_free_space},
                                           {"fs_reformat", l_fs_reformat},

                                           {NULL, NULL}};

void *my_alloc(void *ud, void *ptr, size_t osize, size_t nsize) {
  (void)ud;
  (void)osize;
  if (nsize == 0) {
    free(ptr);
    return NULL;
  } else {
    void *x = realloc(ptr, nsize);
    if (x == NULL) {
      serial("-- out of memory!\r\n");
      watchdog_reboot(0, 0, 1000);
      // TODO: actually do something else?
    }
    return x;
  }
}

void vm_init(bool run_script) {
  // L = luaL_newstate();
  L = lua_newstate(my_alloc, NULL, luaL_makeseed(NULL));
  if (L == NULL) {
    serial("vm_init fail!\r\n");
    return;
  }

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
    char buf[24];
    fs_file_read(&f, buf, 24);
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
    if (fs_file_open(&f, "lib.lua", LFS_O_RDONLY) == 0) {
      const unsigned int sz = (unsigned int)fs_file_size(&f);
      char *buf = malloc(sz + 1);
      buf[sz] = '\0';
      fs_file_read(&f, buf, sz);
      fs_file_close(&f);
      serial("-- init: running lib\r\n");
      int res = luaL_dostring(L, buf);
      free(buf);
      if (res != LUA_OK) {
        const char *err = lua_tostring(L, -1);
        serial("-- init: lib errors!\r\n%s\r\n", (const uint8_t *)err);
      }
    } else {
      serial("-- init: lib fail!\r\n");
    }

    // run init.lua if present
    const char *path = "init.lua";
    const int flags = LFS_O_RDONLY;
    lfs_file_t file;
    if (fs_file_open(&file, path, flags) == 0) {
      const unsigned int sz = (unsigned int)fs_file_size(&file);
      char *buf = malloc(sz + 1);
      buf[sz] = '\0';
      fs_file_read(&file, buf, sz);
      fs_file_close(&file);
      serial("-- init: running script\r\n");
      int res = luaL_dostring(L, buf);
      free(buf);
      if (res != LUA_OK) {
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

bool vm_test_script(const char *buf) {
  int res = luaL_loadstring(L, buf);
  if (res != LUA_OK) {
    serial("-- lua error:\r\n");
    const char *err = lua_tostring(L, -1);
    err = strchr(err + 10, '"');
    err += 3;
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
