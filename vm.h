#ifndef PICO_WEAVE_VM_H
#define PICO_WEAVE_VM_H

#include "lua.h"
#include "pico/stdlib.h"

extern lua_State *L;

extern void vm_init(bool run_script);
extern void vm_deinit();
extern bool vm_test_script(const char *buf);
extern bool vm_run_buffer(const char *buf);
extern void vm_handle_midi(uint8_t data1, uint8_t data2, uint8_t data3);
extern void vm_handle_metro(int index, int stage);

extern int l_report(lua_State *L, int status);
extern int docall(lua_State *L, int narg, int nres);

#endif // PICO_WEAVE_VM_H
