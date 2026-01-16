#ifndef PICO_WEAVE_DEVICE_H
#define PICO_WEAVE_DEVICE_H

#include "lua.h"

extern bool check_device_key();
extern void device_init();
extern void device_task();
extern void device_handle_serial(uint8_t *data, uint32_t len);
extern const struct luaL_Reg *get_device_lib(void);
extern const char *device_str1();
extern const char *device_str2();
extern const char *device_help_txt();
extern const char *device_id();

#endif // PICO_WEAVE_DEVICE_H
