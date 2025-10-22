// blink for rp2040 (standard, not pico W)

#include "pico/multicore.h"

#include "device.h"
#include "flash.h"
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "vm.h"


void vm_handle_blink(uint16_t c);

bool tick = false;
uint16_t count = 0;

void core1_entry() {
	multicore_lockout_victim_init();
	while(1) {
		sleep_ms(1000);
		tick = true;
	}
}

bool check_device_key() {
	return false;
}

void device_init() {
	gpio_init(PICO_DEFAULT_LED_PIN);
  gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
	multicore_launch_core1(core1_entry);
}

void device_task(void) {
	if(tick) {
		tick = false;
		count++;
		vm_handle_blink(count);
	}
}

//////////////////////////////////////////////////////////////////

static int l_blink(lua_State *l) {
	uint8_t led_state = (uint8_t) lua_tointeger(l,1);
	gpio_put(PICO_DEFAULT_LED_PIN, led_state);
	return 0;
}

static const struct luaL_Reg dlib[] = {
	{"blink", l_blink},
	{NULL, NULL}
};

const struct luaL_Reg *get_device_lib(void) {
	return dlib;
}

const char *device_help_str = "\
blink\n\
	event_blink() -- 1 sec timer\n\
	blink(z)\n\
";

extern const char *device_help_txt() {
	return device_help_str;
}

void vm_handle_blink(uint16_t c) {
	if (L == NULL) return;
	lua_getglobal(L, "event_blink");
	if (lua_isnil(L, -1)) { lua_pop(L, 1); return; }
	lua_pushinteger(L, c);
	l_report(L, docall(L,1,0));
}

//////////////////////////////////////////////////////////////////

const char device_str1_0[] = "iii";
const char device_str1_1[] = "monome";

const char *device_str1() {
	return flash_read_mode() ? device_str1_1 : device_str1_0;
}

const char device_str2_0[] = "iii blink";
const char device_str2_1[] = "blink";

const char *device_str2() {
	return flash_read_mode() ? device_str2_1 : device_str2_0;
}

//////////////////////////////////////////////////////////////////


