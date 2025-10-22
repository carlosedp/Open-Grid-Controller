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
#include "lauxlib.h"
#include "serial.h"
#include "util.h"
#include "vm.h"

void (*task)(void); // device_task pointer
void device_task0();
void device_task1();

#define PIN_SDA 0
#define PIN_SCL 1
#define PIN_SD 2
#define PIN_KEY 6
#define PIN_MUX0 16

#define PWM 0x01

#define IS0 0x34
#define IS1 0x37

bool refresh = true;
uint8_t intensity = 15;   // global led intensity
uint8_t map0[129];         // led values, native to driver + index 0 PWM cmd
uint8_t map1[129];         // led values, native to driver + index 0 PWM cmd

const uint8_t IGAMMA[16] =
	{12, 15, 19, 22, 26, 29, 33, 36, 40, 43, 47, 50, 54, 57, 61, 65};
const uint8_t MGAMMA[16] =
	{ 0, 3, 6, 12, 19, 28, 40, 55, 72, 91, 113, 137, 163, 191, 222, 255 };

// >>> for i in range(0,64):
//...     print(((i&0x3)<<4)+((i&0x3C)>>2))
// manually rotated last 16 segments to the front, which rotates the ring 90 degrees CCW
// below is reversed! manual edit below that.
//const uint8_t remap[64] = { 4, 20, 36, 52, 5, 21, 37, 53, 6, 22, 38, 54, 7, 23, 39, 55, 8, 24, 40, 56, 9, 25, 41, 57, 10, 26, 42, 58, 11, 27, 43, 59, 12, 28, 44, 60, 13, 29, 45, 61, 14, 30, 46, 62, 15, 31, 47, 63, 0, 16, 32, 48, 1, 17, 33, 49, 2, 18, 34, 50, 3, 19, 35, 51 };
const uint8_t remap[64] = { 4, 51, 35, 19, 3, 50, 34, 18, 2, 49, 33, 17, 1, 48, 32, 16, 0, 63, 47, 31, 15, 62, 46, 30, 14, 61, 45, 29, 13, 60, 44, 28, 12, 59, 43, 27, 11, 58, 42, 26, 10, 57, 41, 25, 9, 56, 40, 24, 8, 55, 39, 23, 7, 54, 38, 22, 6, 53, 37, 21, 5, 52, 36, 20 };

uint8_t map[4][64];
uint8_t *rm[4][64];

// 3 bits of avg = 8 stages
#define AVGS 4
uint16_t adc[8] = {0,0,0,0,0,0,0,0}; // raw readings 11 bit
uint32_t acc[8] = {0,0,0,0,0,0,0,0}; // accumulator for smoothing
uint32_t pos_now[4] = {0,0,0,0};		// position currently
uint32_t pos_last[4] = {0,0,0,0};	// position of last sent packet
uint16_t state[8] = { 0,0,0,0,0,0,0,0 };	// positions of quadrature: smoothed/reduced
uint8_t scan = 0;													// current read position
uint16_t res[4] = {1,1,1,1};		// ticks per delta
int32_t accum[4] = {0,0,0,0};		// accumulator for resolution


uint8_t key = 1;
uint8_t debounce_hold = 0;

struct repeating_timer delta_monitor;
bool delta_monitor_action(__unused struct repeating_timer *t);

#define ARC_EVENTS_MAX 32
uint8_t arc_event_count = 0;
struct {
	uint8_t t;
	uint8_t n;
	int8_t d;
} arc_events[ARC_EVENTS_MAX];

void arc_add_event(uint8_t t, uint8_t n, int8_t d);

void is31_init();
void is31_map();
void core1_entry();
void draw_ring_single(uint8_t i, uint8_t p, uint8_t z);
void draw_ring_all(uint8_t i, uint8_t z);

void adc_handler();
uint32_t angle_360(int x, int y);


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
	// build ring pointer map
	for(uint8_t i=0;i<4;i++) {
		for(uint8_t x=0;x<64;x++) {
			if(i<2) { rm[i][x] = &map0[0] + 1 + (i*64) + remap[x]; }
			else { rm[i][x] = &map1[0] + 1 + ((i-2)*64) + remap[x]; }
		}
	}

	// task setup
	uint8_t mode = flash_read_mode();
	if(mode==0) task = &device_task0;
	else task = &device_task1;

	// KEY
	gpio_init(PIN_KEY);
	gpio_set_dir(PIN_KEY, GPIO_IN);
	gpio_set_pulls(PIN_KEY, true, false);

	// QUAD POT
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
	adc_set_clkdiv(10000); // how often the adc is called
	irq_set_exclusive_handler(ADC_IRQ_FIFO, adc_handler);
	irq_set_enabled(ADC_IRQ_FIFO, true);
	adc_run(true);

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
	//TODO mode startup animation etc
	//else if(strlen(flash_read_script())==0) plasma();
	else sleep_ms(1); // let the adc averages fill up

	// BEGIN
	// loop refresh
	multicore_launch_core1(core1_entry);
	// reset/fill adc state
	for(uint8_t i=0;i<8;i++){
		acc[i] = adc[i] << AVGS;
		state[i] = (uint16_t)acc[i] >> AVGS;
	}
	for(uint8_t i=0;i<4;i++) { 
		uint32_t a = angle_360((state[i*2+1])<<4,(state[i*2])<<4)>>13;
		pos_now[i] = a;
		pos_last[i] = a;
	}
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
	// DELTA
	for(uint8_t i=0;i<4;i++) {
		uint8_t n = i<<1;
		uint32_t a = angle_360((state[n+1])<<4,(state[n])<<4)>>13;
		pos_now[i] = a;
		int32_t d = (int32_t)(pos_now[i] - pos_last[i]);
		if(d>1||d<-1) {
			int16_t dd = (int16_t)d/2;
			if(dd!=0) {
				pos_last[i] = (uint16_t)((int16_t)pos_last[i]+(dd*2));
				if(res[i]==1) arc_add_event(0,i,(int8_t)dd);
				else {
					// resolution reduction
					if(dd>512) dd = 1024-dd;
					else if(dd<-512) dd = 1024+dd;
					accum[i] = accum[i] + dd;
					if(abs(accum[i]) >= res[i]) {
						int32_t tick;
						tick = accum[i] / res[i];
						accum[i] = accum[i] - tick * res[i];
						arc_add_event(0,i,(int8_t)tick);
					}
				}
			}
		}
	}

	// CHECK KEY
	if(debounce_hold==0) {
		uint8_t v = gpio_get(PIN_KEY);
		if(v != key) {
			key = v;
			arc_add_event(1,1-v,0);
			debounce_hold = 3; // 3 * 2ms
		}
	} else debounce_hold--;
	return true;
}

/////////////////////////////////
void i2c_write_pair(uint8_t a, uint8_t addr, uint8_t b) {
	uint8_t d[2] = {a,b};
	i2c_write_blocking(i2c_default, addr, d, 2, false);
}
void i2c_write_block(uint8_t *val, uint8_t addr, uint8_t len) {
	i2c_write_blocking(i2c_default, addr, val, len, false);
}

/////////////////////////////////
void is31_init() {
	// config
	i2c_write_pair(0xA0, IS0, 0x11); // 8x16, shutdown off
	i2c_write_pair(0xA0, IS1, 0x11); // 8x16, shutdown off
	i2c_write_pair(0xA1, IS0, 0x40); // max Global Current (intensity: 65)
	i2c_write_pair(0xA1, IS1, 0x40); // max Global Current
	i2c_write_pair(0xB2, IS0, 0x02); // PWM FREQ
	i2c_write_pair(0xB2, IS1, 0x02); // PWM FREQ
	i2c_write_pair(0xB0, IS0, 0x91); // resistor settings
	i2c_write_pair(0xB0, IS1, 0x91); // resistor settings

	for( int i=0; i<16; i++ ) {
		i2c_write_pair( (uint8_t)(0x90+i), IS0, 0xFF ); // scaling
		i2c_write_pair( (uint8_t)(0x90+i), IS1, 0xFF ); // scaling
	}

	// clear
	map0[0] = PWM;
	map1[0] = PWM;
	memset( &(map0[1]), 0, 128 );
	memset( &(map1[1]), 0, 128 );
	i2c_write_block( map0, IS0, 1+128 );
	i2c_write_block( map1, IS1, 1+128 );
}

/////////////////////////////////
void is31_map() {
	//dbg("+");
	// intensity
	i2c_write_pair(0xA1, IS0, IGAMMA[intensity]);
	i2c_write_pair(0xA1, IS1, IGAMMA[intensity]);
	// map
	i2c_write_block(map0, IS0, 129);
	i2c_write_block(map1, IS1, 129);
}

/////////////////////////////////
void draw_ring_single(uint8_t i, uint8_t p, uint8_t z) {
	//dbg("draw_ring_single\n");
	// FIXME are lockouts necessary?? tested ok??
	//multicore_lockout_start_blocking();
	*(rm[i][p]) = MGAMMA[z];
	//multicore_lockout_end_blocking();
}

/*void draw_all(uint8_t z) {
	//dbg("draw_all\n");
	//multicore_lockout_start_blocking();
	memset( &(map0[1]), MGAMMA[z], 128 );
	memset( &(map1[1]), MGAMMA[z], 128 );
	//multicore_lockout_end_blocking();
}*/

void draw_ring_all(uint8_t i, uint8_t z) {
	//dbg("draw_ring_all\n");
	// FIXME are lockouts necessary?? tested ok??
	//multicore_lockout_start_blocking();
	if(i<2) { memset( &(map0[1+(i*64)]), MGAMMA[z], 64 );
	} else { memset( &(map1[1+(i-2)*64]), MGAMMA[z], 64 ); }
	//multicore_lockout_end_blocking();
}


/////////////////////////////////
void adc_handler() {
	uint8_t n = scan;
	// advance the MUX early for next sampling
	scan++;
	scan = scan & 0x7; // eight pins to sample
  gpio_put_masked(0x7 << PIN_MUX0, scan << PIN_MUX0);

	while(!adc_fifo_is_empty()) adc[n] = adc_fifo_get() >> 1; // reduce to 11 bit
	acc[n] = acc[n] - (acc[n] >> AVGS) + adc[n];
	state[n] = (uint16_t)acc[n] >> AVGS;
}

// inputs expected in [0,32767] range
#define U_BITS 15 // change this to select input range

#define U_TWO ((1<<(U_BITS+1))-1)
#define U_ONE_HALF (((3*(1<<U_BITS))>>1)-1)
#define U_ONE ((1<<U_BITS)-1)
#define U_HALF ((1<<(U_BITS-1))-1)
#define U_SHIFT24 ((2 * U_BITS) - 24)

#define U24_ONE 0xFFFFFF
#define U24_HALF 0x7FFFFF
#define U24_ONE_1 0x1000000
#define U24_ONE_THIRD (0x1000000 / 3)
#define U24_ONE_SIXTH (0x1000000 / 6)

uint32_t angle_360(int a, int b){
	// interpolant coefficient
	int c; // [0,U_TWO]
	if(b > U_HALF){
		c = 2 * (U_ONE - b);
	} else {
		c = b * 2;
	}

	// shift quadrants into (0..1) range
	int aa = a; // [0,U_ONE]
	int bb = b; // [0,U_ONE]
							// flips
	if(b > U_HALF){
		aa = U_ONE - aa;
	}
	if(a < U_HALF){
		bb = U_ONE - bb;
	}
	// shifts
	if(b > U_HALF){
		aa += U_HALF;
	} else if(a > U_HALF){
		aa -= U_HALF;
	} else{ // a < U_HALF
		aa += U_ONE_HALF;
	}
	if(a < U_HALF){
		bb += U_ONE;
	}
	// down-scale /2
	aa >>= 1;
	bb >>= 1;

	// linear interpolate
	return (uint32_t)((aa<<U_BITS) + (c*(bb-aa))) >> U_SHIFT24; // 24bit output!!!
}


//////////////////////////////////////////////////////////////////
void arc_res(uint8_t n, uint16_t r) {
	res[n] = (uint16_t)clamp(r,1,1024);
	accum[n] = 0;
}

void arc_led(uint8_t x, uint8_t y, uint8_t z) {
	map[x][y]=z;
	draw_ring_single(x,y,z);
}

void arc_led_rel(uint8_t x, uint8_t y, int8_t z, uint8_t zmin, uint8_t zmax) {
	map[x][y] = (uint8_t)clamp(z+map[x][y],zmin,zmax);
	draw_ring_single(x,y,map[x][y]);
}

void arc_led_all(uint8_t y, uint8_t z) {
	for(uint8_t n=0;n<64;n++) map[y][n]=z;
	draw_ring_all(y,z);
}

void arc_refresh(void) {
	refresh = true;
}


void arc_add_event(uint8_t t, uint8_t n, int8_t d) {
	if(arc_event_count < ARC_EVENTS_MAX) {
		arc_events[arc_event_count].t = t;
		arc_events[arc_event_count].n = n;
		arc_events[arc_event_count].d = d;
		arc_event_count++;
	} else {
    const char *s = "arc event queue full!\n";
    tud_cdc_n_write(0,(const uint8_t *)s, strlen(s));
		tud_cdc_n_write_flush(0);
		//serial_tx_str("arc queue full!\r\n");
	}
}


//////////////////////////////////////////////////////////////////

static int l_arc_res(lua_State *l) {
	uint8_t n = (uint8_t) lua_tointeger(l, 1) - 1;
	uint16_t r = (uint16_t) lua_tointeger(l, 2);
	arc_res(n,r);
	return 0;
}

static int l_arc_led(lua_State *l) {
	uint8_t x = (uint8_t) lua_tointeger(l, 1) - 1;
	uint8_t y = (uint8_t) lua_tointeger(l, 2) - 1;
	int z = (int)lua_tointeger(l, 3);
	z = clamp(z,0,15);
	if(x>3) return 0;
	if(y>63) return 0;
	arc_led(x,y,(uint8_t)z);
	return 0;
}

static int l_arc_led_rel(lua_State *l) {
	int nargs = lua_gettop(l);
	uint8_t x = (uint8_t) lua_tointeger(l, 1) - 1;
	uint8_t y = (uint8_t) lua_tointeger(l, 2) - 1;
	int8_t z = (int8_t) lua_tointeger(l, 3);
	if(x>3) return 0;
	if(y>63) return 0;
	uint8_t zmin = 0;
	uint8_t zmax = 15;
	if(nargs>3) zmin = (uint8_t) lua_tointeger(l, 4);
	if(nargs>4) zmax = (uint8_t) lua_tointeger(l, 5);
	arc_led_rel(x,y,z,zmin,zmax);
	return 0;
}

static int l_arc_led_get(lua_State *l) {
	uint8_t x = (uint8_t) lua_tointeger(l, 1) - 1;
	uint8_t y = (uint8_t) lua_tointeger(l, 2) - 1;
	if(x>3) return 0;
	if(y>63) return 0;
	lua_pushinteger(l, map[x][y]);
	return 1;
}

static int l_arc_led_ring(lua_State *l) {
	uint8_t y = (uint8_t) lua_tointeger(l, 1) - 1;
	int z = (int)lua_tointeger(l, 2);
	if(y>3) return 0;
	z = clamp(z,0,15);
	arc_led_all(y,(uint8_t)z);
	return 0;
}

static int l_arc_led_all(lua_State *l) {
	int z = (int)lua_tointeger(l, 2);
	z = clamp(z,0,15);
	for(uint8_t i=0;i<4;i++) arc_led_all(i,(uint8_t)z);
	return 0;
}

static int l_arc_refresh(lua_State *l) {
	(void)l;
	arc_refresh();
	return 0;
}

static const struct luaL_Reg dlib[] = {
	{"arc_res", l_arc_res},
	{"arc_led", l_arc_led},
	{"arc_led_rel", l_arc_led_rel},
	{"arc_led_ring", l_arc_led_ring},
	{"arc_led_all", l_arc_led_all},
	{"arc_refresh", l_arc_refresh},
	{"arc_led_get", l_arc_led_get},
	{NULL, NULL}
};

const struct luaL_Reg *get_device_lib(void) {
	return dlib;
}

const char *device_help_str = "\
arc\n\
	event_arc(n,d)\n\
	event_arc_key(z)\n\
	arc_res(n,div)\n\
	arc_led(x,y,z)\n\
	arc_led_rel(x,y,z,zmin,zmax)\n\
	arc_led_all(x,z)\n\
	arc_refresh()\n\
";

extern const char *device_help_txt() {
	return device_help_str;
}


void vm_handle_arc_delta(uint8_t n, int8_t d) {
	if (L == NULL) return;
	lua_getglobal(L, "event_arc");
	if (lua_isnil(L, -1)) {
		serial("event_arc: %d %d\r\n",n+1,d);
		lua_pop(L, 1); return; }
	lua_pushinteger(L, n+1); // 1-based
	lua_pushinteger(L, d);
	l_report(L, docall(L,2,0));
}

void vm_handle_arc_key(uint8_t z) {
	if (L == NULL) return;
	lua_getglobal(L, "event_arc_key");
	if (lua_isnil(L, -1)) {
		serial("event_arc_key: %d\r\n",z);
		lua_pop(L, 1); return; }
	lua_pushinteger(L, z);
	l_report(L, docall(L,1,0));
}




//////////////////////////////////////////////////////////////////

void device_task() {
	task();	
}

uint8_t evc = 0;
void device_task0() {
	if(arc_event_count>0) {
		if(arc_events[evc].t == 0) {
			vm_handle_arc_delta(arc_events[evc].n,arc_events[evc].d);
		} else {
			vm_handle_arc_key(arc_events[evc].n);
		}
		evc++;
		if(evc==arc_event_count) {
			evc = 0;
			arc_event_count = 0;
		}
	}
}

#define RXBUFLEN 256
uint8_t rxr = 0;
uint8_t rxw = 0;
uint8_t rx[RXBUFLEN];
void cmd_parse();
uint8_t txw = 0;

void device_task1() {
	if ( tud_cdc_n_available(0) ) {
		uint8_t n = (uint8_t)tud_cdc_n_read(0, rx+rxw, RXBUFLEN-rxw);
		rxw += n;
	}
	if(rxr != rxw) {
		cmd_parse();
		if(rxr==rxw) {
			rxr = 0;
			rxw = 0;
		}
	}

	if(arc_event_count != txw) {
		if( tud_cdc_n_write_available(0)>3) {
			// delta
			if(arc_events[txw].t == 0) {
				static uint8_t p[3];
				p[0] = 0x50;
				p[1] = arc_events[txw].n;
				p[2] = (uint8_t)arc_events[txw].d;
				tud_cdc_n_write(0, p, 3);
				tud_cdc_n_write_flush(0);
				txw++;
			} 
			// key
			else {
				static uint8_t p[2];
				p[0] = (uint8_t)(0x51 + arc_events[txw].n);
				p[1] = 0;
				tud_cdc_n_write(0, p, 2);
				tud_cdc_n_write_flush(0);
				txw++;
			}
			if(txw==arc_event_count) {
				txw=0;
				arc_event_count=0;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////

const char device_str1_0[] = "iii";
const char device_str1_1[] = "monome";

const char *device_str1() {
	return flash_read_mode() ? device_str1_1 : device_str1_0;
}

const char device_str2_0[] = "iii arc";
const char device_str2_1[] = "arc";

const char *device_str2() {
	return flash_read_mode() ? device_str2_1 : device_str2_0;
}

//////////////////////////////////////////////////////////////////

typedef enum {
      SYS_QUERY     = 0x00
    , SYS_QUERY_ID  = 0x01
    , SYS_GET_SIZE  = 0x05
    , LED_OFF       = 0x10
    , LED_ON        = 0x11
    , LED_ALL_OFF   = 0x12
    , LED_ALL_ON    = 0x13
    , LED_MAP       = 0x14
    , LED_ROW       = 0x15
    , LED_COL       = 0x16
    , LED_INTENSITY = 0x17 
    , LED_LEVEL     = 0x18
    , LED_LEVEL_ALL = 0x19
    , LED_LEVEL_MAP = 0x1A
    , LED_LEVEL_ROW = 0x1B
    , LED_LEVEL_COL = 0x1C
		, RING_SET = 0x90
		, RING_ALL = 0x91
		, RING_MAP = 0x92
		, RING_RANGE = 0x93
} Command;

// length is inclusive of cmd byte
static uint8_t get_cmd_length( uint8_t cmd ) {
	switch(cmd) {
		case SYS_QUERY:       return 1;
		case SYS_QUERY_ID:    return 1;
		case SYS_GET_SIZE:    return 1;
		case LED_OFF:         return 3;
		case LED_ON:          return 3;
		case LED_ALL_OFF:     return 1;
		case LED_ALL_ON:      return 1;
		case LED_MAP:         return 11;
		case LED_ROW:         return 4;
		case LED_COL:         return 4;
		case LED_INTENSITY:   return 2;
		case LED_LEVEL:       return 4;
		case LED_LEVEL_ALL:   return 2;
		case LED_LEVEL_MAP:   return 35;
		case LED_LEVEL_ROW:   return 7;
		case LED_LEVEL_COL:   return 7;
		case RING_SET:				return 4;
		case RING_ALL:				return 3;
		case RING_MAP:				return 34;
		case RING_RANGE:			return 5;
	}
	return 0;
}

void cmd_parse() {
	uint8_t* b = rx;
	b+=rxr;
  uint8_t cmdlen = get_cmd_length(b[0]);
  if( cmdlen == 0 ) {
		rxr = 0;
		rxw = 0;
    //dbg("=cmdfail for %d\n",b[0]); // abandon
		return;
	}
	else if(cmdlen > (rxw - rxr)) {
		//dbg("= PART = r%d w%d\n",rxr,rxw);
		return;
  } else { // check the whole packet is ready
    switch(b[0]) {
      case SYS_QUERY:{
        //dbg("SYS_QUERY\n");
        uint8_t packet[3] = {0x0, 5, 4};
        tud_cdc_n_write(0, packet, 3 ); // led-rings: 4
				tud_cdc_n_write_flush(0);
        packet[1] = 9; // {0,9,4}
        tud_cdc_n_write(0, packet, 3 ); // encoders: 4
				tud_cdc_n_write_flush(0);
        break;}
      case SYS_QUERY_ID:{
        //dbg("SYS_QUERY_ID\n");
        uint8_t packet[33] = " monome arc";
        //memset(packet, 0, 33);
        packet[0] = 0x1;
        //strcpy(&packet[1], "monome arc");
        tud_cdc_n_write(0,packet, 33);
				tud_cdc_n_write_flush(0);
        break;}
      case SYS_GET_SIZE:{
        //dbg("SYS_GET_SIZE\n");
        uint8_t packet[3] = {0x03, 0, 0};
        tud_cdc_n_write(0,packet, 3); // report grid size
				tud_cdc_n_write_flush(0);
        break;}

			case LED_ALL_OFF:
				break;
      case LED_INTENSITY:
				intensity = b[1];
        break;
      case RING_SET:
				//dbg("ring_set %d %d %d\r\n",b[1],b[2],b[3]);
        //draw_ring_single(b[1], b[2], b[3]);
				//multicore_lockout_start_blocking();
				*(rm[b[1]][b[2]]) = MGAMMA[b[3]];
				refresh=true;
				//multicore_lockout_end_blocking();
        break;
      case RING_ALL:
				//draw_ring_all(b[1],b[2]);
				//multicore_lockout_start_blocking();
				if(b[1]<2) { memset( &(map0[1+(b[1]*64)]), MGAMMA[b[2]], 64 );
				} else { memset( &(map1[1+(b[1]-2)*64]), MGAMMA[b[2]], 64 ); }
				refresh=true;
				//multicore_lockout_end_blocking();
        //draw_all(b[1]);
        break;
			case LED_LEVEL_MAP:
				break;
		  case RING_MAP: {
				uint8_t n = b[1] & 0x03;
				//dbg("ring map: %d\n",n);
				//multicore_lockout_start_blocking();
				for(int p=0;p<32;p++) {
					uint8_t x = MGAMMA[b[p+2] & 0xF]; 
					uint8_t y = MGAMMA[b[p+2] >> 4];
					*(rm[n][p*2]) = y;
					*(rm[n][(p*2)+1]) = x;
					//dbg("%d = %d\n%d = %d\n",p*2, x, (p*2)+1, y);
					//(*map)[remap[p*2]] = MGAMMA[b[p*2+2] & 0xF];
					//(*map)[remap[p*2+1]] = MGAMMA[b[p*2+2] >> 4];
				}
				refresh=true;
				//multicore_lockout_end_blocking();
				break;}
			case RING_RANGE: {
				/*for(uint8_t n=b[2]&0x3f;n!=b[3]&0x3f;n=(n-1)&0x3f) {
					draw_ring_single(b[1]&0x3,n,b[4]&0xf);
				}*/
				uint8_t n = b[1]&0x3;
				uint8_t x1 = b[2] & 0x3f;
				uint8_t x2 = b[3] & 0x3f;
				uint8_t a = MGAMMA[b[4] & 0xf];
				//multicore_lockout_start_blocking();
				for(uint8_t i=x1; i!=x2; i=(i+1)&0x3f) {
					*(rm[n][i]) = a;
				}
				*(rm[n][x2]) = a; // inclusive
				refresh=true;
				//multicore_lockout_end_blocking();
				break;}

	    default: {
				//dbg("cmd_parse failed to find command\n");
			}
		}
		rxr += cmdlen;
		//dbg("*");
	}
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
#define PS 24

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
	double r[16][16]; // note we have to nest y first in the array
	while(frame++ < FRAMECOUNT) {
		memset(r,0,sizeof(double)*256);

		for(int i=0;i<PS;i++) {
			if(par[i].here) {
				int x = (int)floor(par[i].x);
				int y = (int)floor(par[i].y);
				double rx = par[i].x - x;
				double ry = par[i].y - y;

				r[y][x] += fabs(rx-0.5) + fabs(ry-0.5);
			}
		}

		for(int y=0;y<16;y++)
			for(int x=0;x<16;x++) {
				r[y][x] = MIN(r[y][x],1);
				r[y][x] = r[y][x] * (double)(FRAMECOUNT-frame)/(double)FRAMECOUNT;
				if(y<8) map0[1+y*16+x] = MGAMMA[(int)(r[y][x] * 16)];
				else map1[1+(y-8)*16+x] = MGAMMA[(int)(r[y][x] * 16)];
			}

		for(int i = 0; i<PS; i++) {
			par[i].dx *= 0.9;
			par[i].x += par[i].dx;
			par[i].dy *= 0.9;
			par[i].y += par[i].dy;
			par[i].here = (par[i].x < 16) && (par[i].y < 16);
		}

		is31_map();
		sleep_ms(20);
	}
}


void plasma() {
	double a = r()*7.0;
	double b = r()*8.0;
	double t = r()/13.0;
	double tt = r()/15.0;

	uint8_t frame = 0;
	while(frame++ < FRAMECOUNT) {
		for(int y=0;y<2;y++)
			for(int x=0;x<64;x++) {
				map0[1+y*64+x] = MGAMMA[(int)(cos(x*a+frame*t)+sin(y*b+frame*tt)*16*(double)(FRAMECOUNT-frame)/(double)FRAMECOUNT)];
			}
		for(int y=2;y<4;y++)
			for(int x=0;x<64;x++) {
				map1[1+(y-2)*64+x] = MGAMMA[(int)(cos(x*a+frame*t)+sin(y*b+frame*tt)*16*(double)(FRAMECOUNT-frame)/(double)FRAMECOUNT)];
			}

		is31_map();
		sleep_ms(20);
	}
}
