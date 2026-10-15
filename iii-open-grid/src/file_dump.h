#pragma once

// og_dump_file(name): send a file exactly, in constant memory, for backups.
// Written against iii's fs/serial API only, so it can move into iii as is.

#include "lua.h"

int og_l_dump_file(lua_State *l);
