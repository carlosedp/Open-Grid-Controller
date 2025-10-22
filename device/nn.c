/* n/n
TODO:
	debug LED ghosting
https://lumissil.com/assets/pdf/core/IS31FL3733B_DS.pdf
mux 0-5 sliders 6,7 xp
0 sda
1 scl
2 sdb
8 midi out
9 midi in
12 tx
13 rx
*/

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "pico/multicore.h"
#include "pico/unique_id.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "tusb.h"

#include "device.h"
#include "flash.h"
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "serial.h"
#include "vm.h"

void (*task)(void); // device_task pointer
void device_task0();
void device_task1();

// 101 0000 = 0x50
#define ADDR 0x50

#define PIN_SDA 0
#define PIN_SCL 1
#define PIN_SD 2
#define PIN_MUX0 16
#define PIN_KEY 6

#define PWM 0x01


bool refresh = true;
uint8_t intensity = 15;   // global led intensity
uint8_t map[193];

const uint8_t IGAMMA[16] =
	{ 0, 3, 6, 12, 19, 28, 40, 55, 72, 91, 113, 137, 163, 191, 222, 255 };
	//{12, 15, 19, 22, 26, 29, 33, 36, 40, 43, 47, 50, 54, 57, 61, 65};
const uint8_t MGAMMA[16] =
	{ 0, 3, 6, 12, 19, 28, 40, 55, 72, 91, 113, 137, 163, 191, 222, 255 };

uint16_t a_val[6][4];		// adc readings, six inputs, four stages (avg)
uint8_t a_pos = 0;			// which stage for avg
uint8_t pos_now[6] = {0,0,0,0,0};		// position currently
uint8_t pos_last[6] = {0,0,0,0,0};	// position of last sent packet
uint8_t scan = 0;										// current input read position

uint8_t key = 1;
uint8_t debounce_hold = 0;

struct repeating_timer delta_monitor;
bool delta_monitor_action(__unused struct repeating_timer *t);

#define NN_EVENTS_MAX 32
uint8_t nn_event_count = 0;
struct {
	uint8_t t;
	uint8_t x;
	uint8_t y;
} nn_events[NN_EVENTS_MAX];

void nn_add_event(uint8_t x, uint8_t y, uint8_t z);

void i2c_write_pair(uint8_t a, uint8_t b);
void i2c_write_block(uint8_t *val, uint8_t len);
void is31_init();
void is31_map();
void draw_single(uint8_t x, uint8_t y, uint8_t z);
void draw_all(uint8_t z);
void core1_entry();

void adc_handler();

void particle();
void plasma();


//////////////////////////////////////////////////////////////////

bool check_device_key() {
	gpio_init(PIN_KEY);
	gpio_set_dir(PIN_KEY, GPIO_IN);
	gpio_set_pulls(PIN_KEY, true, false);
	sleep_us(1);
	return gpio_get(PIN_KEY)==0;
}


void device_init() {
	// task setup
	//uint8_t mode = flash_read_mode();
	uint8_t mode = 0;
	if(mode==0) task = &device_task0;
	else task = &device_task1;

	// LED DRIVER
	// gpio
	gpio_init(PIN_SD);
	gpio_set_dir(PIN_SD, GPIO_OUT);
	gpio_put(PIN_SD, 1); // take led driver out of shutdown
	// i2c
	i2c_init(i2c_default, 400 * 1000);
	gpio_set_function(PIN_SDA, GPIO_FUNC_I2C);
	gpio_set_function(PIN_SCL, GPIO_FUNC_I2C);
	gpio_pull_up(PIN_SDA);
	gpio_pull_up(PIN_SCL);
	// led driver
	is31_init();
	// startup pattern
	if(mode>0) particle();
	//else if(strlen(flash_read_script())==0) plasma();
	else particle();

	// gpio
	for(uint8_t i=0;i<3;i++) {
		gpio_init(PIN_MUX0+i);
		gpio_set_dir(PIN_MUX0+i, GPIO_OUT);
		gpio_put(PIN_MUX0, 0);
	}
	// adc
	adc_init();
	adc_gpio_init(26);
	adc_select_input(0);
	adc_fifo_setup(true,false,1,false,false);
	adc_irq_set_enabled(true);
	adc_set_clkdiv(10000);
	irq_set_exclusive_handler(ADC_IRQ_FIFO, adc_handler);
	irq_set_enabled(ADC_IRQ_FIFO, true);
	adc_run(true);

	// loop refresh
	multicore_launch_core1(core1_entry);

	// input timer
	add_repeating_timer_ms(4, delta_monitor_action, NULL, &delta_monitor);
}

void core1_entry() { 
	multicore_lockout_victim_init();
	while(1) {
		if(refresh) {
			refresh = false;
			is31_map(); 
		}
		sleep_us(1000000/250);
	}
}

/////////////////////////////////
bool delta_monitor_action(__unused struct repeating_timer *t) {
	(void)t;
	// FADER
	for(uint8_t i=0;i<6;i++) {
		int8_t d = (int8_t)(pos_now[i] - pos_last[i]);
		if(d != 0) {
			nn_add_event(0,6-i,pos_now[i]);	
			pos_last[i] = pos_now[i];
		}
	}

	// CHECK KEY
	if(debounce_hold==0) {
		uint8_t v = gpio_get(PIN_KEY);
		if(v != key) {
			key = v;
			nn_add_event(1,1-v,0);
			debounce_hold = 3; // 3 * 2ms
		}
	} else debounce_hold--;
	return true;
}

/////////////////////////////////
void i2c_write_pair(uint8_t a, uint8_t b) {
	uint8_t d[2] = {a,b};
	i2c_write_blocking(i2c_default, ADDR, d, 2, false);
	//if(ret<0) dbg("i2c write fail\n");
}

void i2c_write_block(uint8_t *val, uint8_t len) {
	i2c_write_blocking(i2c_default, ADDR, val, len, false);
	//if(ret<0) dbg("i2c write fail\n");
}

/////////////////////////////////
void is31_init() {

	// config
	i2c_write_pair(0xFE,0xC5); // unlock
	i2c_write_pair(0xFD,0x03); // select register: function
	i2c_write_pair(0x01,0xFF); // global current: max
	i2c_write_pair(0x0F,0x00); // SW pullup: none
	i2c_write_pair(0x10,0x00); // CS pullup: none

	i2c_write_pair(0xFE,0xC5); // unlock
	i2c_write_pair(0xFD,0x00); // select register: frame
	for(uint8_t i=0;i<0x18;i++)
		i2c_write_pair(i,0xFF); // set LED state: on
	
	i2c_write_pair(0xFE,0xC5); // unlock
	i2c_write_pair(0xFD,0x03); // select register: function
	i2c_write_pair(0x00,0x01); // operation mode: leave shutdown (TODO: maybe tweak PFS)
	
	memset(map, 0, 193);
}

void is31_map() {
	// intensity
	i2c_write_pair(0xFE,0xC5);
	i2c_write_pair(0xFD,IGAMMA[intensity]);
	// pwm data
	i2c_write_pair(0xFE,0xC5);
	i2c_write_pair(0xFD,0x01);
	i2c_write_block(map,193);
}

/////////////////////////////////
void draw_single(uint8_t x, uint8_t y, uint8_t z) {
	//dbg("draw_single\n");
	uint8_t n = (uint8_t)(x*32+y);
	multicore_lockout_start_blocking();
	map[n+1]=MGAMMA[z];
	multicore_lockout_end_blocking();
}

void draw_all(uint8_t z) {
	//dbg("draw_all\n");
	multicore_lockout_start_blocking();
	memset( map, MGAMMA[z], 193 );
	multicore_lockout_end_blocking();
}


/////////////////////////////////
void adc_handler() {
	//FIXME fix avg algo, see arc. but it works.
	uint8_t n = scan;
	uint16_t result = 0;
	while(!adc_fifo_is_empty()) result = adc_fifo_get() >> 2; // reduce to 10 bit
	a_val[n][a_pos] = result;
	uint16_t a = 0;			// accumulator
	a = a_val[n][0];
	a += a_val[n][1];
	a += a_val[n][2];
	a += a_val[n][3];

	a = (a >> 2) >> 1; // first div by avg (4) then reduce to 9 bit
	
	if((a&1)==0) { // hysteresis
		pos_now[n] = (uint8_t)(a>>1);
	}

	scan++;
	if(scan == 6) {
		scan = 0;
		a_pos++;
		a_pos = a_pos & 0x03; // four stages of averaging
	}
  gpio_put_masked(0x7 << PIN_MUX0, scan << PIN_MUX0);
}



//////////////////////////////////////////////////////////////////
void nn_led(uint8_t x, uint8_t y, uint8_t z) {
	//if(x>5 || y>31) return; // don't need to check negatives because unsigned
	//if(z>15) z = 15; // just cap it
	draw_single(x,y,z);
}

void nn_led_all(uint8_t z) {
	if(z>15) z = 15; // just cap it
	draw_all(z);
}

void nn_refresh(void) {
	refresh = true;
}

void nn_add_event(uint8_t t, uint8_t x, uint8_t y) {
	if(nn_event_count < NN_EVENTS_MAX) {
		nn_events[nn_event_count].t = t;
		nn_events[nn_event_count].x = x;
		nn_events[nn_event_count].y = y;
		nn_event_count++;
	}
}

//////////////////////////////////////////////////////////////////

static int l_nn_led(lua_State *l) {
	uint8_t x = (uint8_t) lua_tointeger(l, 1) - 1;
	uint8_t y = (uint8_t) lua_tointeger(l, 2) - 1;
	uint8_t z = (uint8_t) lua_tointeger(l, 3);
	if(x>5 || y>31) return 0;
	if(z>15) z=15;
	nn_led(x,y,z);
	return 0;
}

static int l_nn_led_all(lua_State *l) {
	uint8_t z = (uint8_t) lua_tointeger(l, 1);
	nn_led_all(z);
	return 0;
}

static int l_nn_refresh(lua_State *l) {
	(void)l;
	nn_refresh();
	return 0;
}

static const struct luaL_Reg dlib[] = {
	{"nn_led", l_nn_led},
	{"nn_led_all", l_nn_led_all},
	{"nn_refresh", l_nn_refresh},
	{NULL, NULL}
};

const struct luaL_Reg *get_device_lib(void) {
	return dlib;
}

const char *device_help_str = "\
nn\n\
	event_nn(n,d)\n\
	event_nn_key(z)\n\
	nn_led(x,y,z)\n\
	nn_led_all(z)\n\
	nn_refresh()\n\
";

extern const char *device_help_txt() {
	return device_help_str;
}

void vm_handle_nn(uint8_t n, uint8_t d) {
	if (L == NULL) return;
	lua_getglobal(L, "event_nn");
	if (lua_isnil(L, -1)) { lua_pop(L, 1); return; }
	lua_pushinteger(L, n);
	lua_pushinteger(L, d);
	l_report(L, docall(L,2,0));
}

void vm_handle_nn_key(uint8_t z) {
	if (L == NULL) return;
	lua_getglobal(L, "event_nn_key");
	if (lua_isnil(L, -1)) { lua_pop(L, 1); return; }
	lua_pushinteger(L, z);
	l_report(L, docall(L,1,0));
}


//////////////////////////////////////////////////////////////////

void device_task() {
	task();	
}

void device_task0() {
	for(uint8_t i=0; i<nn_event_count; i++) {
		if(nn_events[i].t == 0) {
			vm_handle_nn(nn_events[i].x,nn_events[i].y);
		} else {
			vm_handle_nn_key(nn_events[i].x);
		}
	}
	nn_event_count = 0;
}


void device_task1() {
}

//////////////////////////////////////////////////////////////////

const char device_str1_0[] = "iii";
const char device_str1_1[] = "monome";

const char *device_str1() {
	//return flash_read_mode() ? device_str1_1 : device_str1_0;
	return device_str1_0;
}

const char device_str2_0[] = "iii nn";
const char device_str2_1[] = "nn";

const char *device_str2() {
	//return flash_read_mode() ? device_str2_1 : device_str2_0;
	return device_str2_0;
}

//////////////////////////////////////////////////////////////////

#define ROSC_RANDOMBIT_OFFSET _u(0x0000001c)
#define ROSC_BASE _u(0x40060000)

// return random number by ROSC between 0-2^N
// N should be in [0,32]
unsigned int rorand(int N)
{
	static volatile uint32_t *randbit_reg = (uint32_t *)(ROSC_BASE + ROSC_RANDOMBIT_OFFSET);
	unsigned int random = 0;
	for (int i = 0; i < N; i++) {
		unsigned int random_bit = 0x1 & (*randbit_reg);
		random = random << 1 | random_bit;
	}
	return random;
}

double r() { return (double)rorand(8)/(double)255; }

#define FRAMECOUNT 25
#define PS 32

struct {
	double x;
	double y;
	double dx;
	double dy;
	int here;
} par[PS];

void particle() {
	for(int i = 0; i<PS; i++) {
		par[i].x = 0;
		par[i].y = r();
		par[i].dx = r()*2;
		par[i].dy = r()*2;
		par[i].here = 1;
	}

	uint8_t frame = 0;
	double r[6][32]; // note we have to nest y first in the array
	while(frame++ < FRAMECOUNT) {
		memset(r,0,sizeof(double)*192);

		for(int i=0;i<PS;i++) {
			if(par[i].here) {
				int x = (int)floor(par[i].x);
				int y = (int)floor(par[i].y);
				double rx = par[i].x - x;
				double ry = par[i].y - y;

				r[y][x] += fabs(rx-0.5) + fabs(ry-0.5);
			}
		}

		for(int y=0;y<6;y++)
			for(int x=0;x<32;x++) {
				r[y][x] = MIN(r[y][x],1);
				r[y][x] = r[y][x] * (double)(FRAMECOUNT-frame)/(double)FRAMECOUNT;
				map[y*32+x] = MGAMMA[(int)(r[y][x] * 16)];
			}

		for(int i = 0; i<PS; i++) {
			par[i].dx *= 0.9;
			par[i].x += par[i].dx;
			par[i].dy *= 0.9;
			par[i].y += par[i].dy;
			par[i].here = (par[i].x < 32) && (par[i].y < 6);
		}

		is31_map();
		sleep_ms(20);
	}
}


void plasma() {
	double a = r()*4.0;
	double b = r()*8.0;
	double t = r()/3.0;
	double tt = r()/5.0;

	uint8_t frame = 0;
	while(frame++ < FRAMECOUNT) {
		for(int y=0;y<32;y++) {
			for(int x=0;x<6;x++) {
				map[y+x*32] = MGAMMA[(int)(cos(x*a+frame*t)+sin(y*b+frame*tt)*16*(double)(FRAMECOUNT-frame)/(double)FRAMECOUNT)];
			}
		}

		is31_map();
		sleep_ms(20);
	}
}
