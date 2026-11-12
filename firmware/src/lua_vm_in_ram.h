// Force-included (-include) into every Lua library source when OG_LUA_VM_IN_RAM
// is on; see CMakeLists.txt.
//
// Places the interpreter loop, luaV_execute (~11 KB), in RAM instead of
// running it from flash through the 16 KB XIP cache, without editing
// third_party/lua: GCC keeps the section attribute of this first declaration
// when lvm.c defines the function, and pico-sdk's linker script copies
// .time_critical* into RAM at boot. Costs that much heap.

struct lua_State;
struct CallInfo;
void luaV_execute(struct lua_State *L, struct CallInfo *ci)
    __attribute__((section(".time_critical.luaV_execute")));
