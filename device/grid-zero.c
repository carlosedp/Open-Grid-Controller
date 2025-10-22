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
#include "util.h"
#include "vm.h"

void (*task)(void); // device_task pointer
void device_task0();
void device_task1();

#define GRID_X 16
#define GRID_Y 16
#define GRID_XMASK 15
#define GRID_YMASK 15

#define PIN_SDA 0
#define PIN_SCL 1
#define PIN_SD 2
#define PIN_MUX0 16

#define PWM 0x01

#define KEY_HI 0xf00
#define KEY_LO 0xd00

#define IS0 0x34
#define IS1 0x37

bool refresh = true;
uint8_t intensity = 15;   // global led intensity
uint8_t map0[129];        // led values, native to driver + index 0 PWM cmd
uint8_t map1[129];        // led values, native to driver + index 0 PWM cmd
uint8_t mmap[256];

const uint8_t IGAMMA[16] =
	{12, 15, 19, 22, 26, 29, 33, 36, 40, 43, 47, 50, 54, 57, 61, 65};
const uint8_t MGAMMA[16] =
	{ 0, 3, 6, 12, 19, 28, 40, 55, 72, 91, 113, 137, 163, 191, 222, 255 };

bool state[256];          // stored key state
uint8_t scan_x = 0;
uint8_t scan_y = 0;
uint8_t scan_a = 0;

#define GRID_EVENTS_MAX 32
uint8_t grid_event_count = 0;
struct {
	uint8_t x;
	uint8_t y;
	uint8_t z;
} grid_events[GRID_EVENTS_MAX];

void grid_add_event(uint8_t x, uint8_t y, uint8_t z);

void i2c_write_pair(uint8_t addr, uint8_t a, uint8_t b);
void i2c_write_block(uint8_t addr, uint8_t *val, uint8_t len);
void is31_init();
void is31_map();
void draw_single(uint8_t x, uint8_t y, uint8_t z);
void draw_row(uint8_t xoff, uint8_t width, uint8_t y, uint8_t* data);
void draw_all(uint8_t z);
void core1_entry();

void adc_handler();

void particle();
void plasma();


//////////////////////////////////////////////////////////////////

bool check_device_key() {
	for(uint8_t i=0;i<6;i++) {
		gpio_init(PIN_MUX0+i);
		gpio_set_dir(PIN_MUX0+i, GPIO_OUT);
		gpio_put(PIN_MUX0, 0);
	}
	adc_init();
	adc_gpio_init(26);
	adc_select_input(0);
	return (adc_read() < KEY_LO);
}


void device_init() {
	// task setup
	uint8_t mode = flash_read_mode();
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
	else plasma();
	// loop refresh
	multicore_launch_core1(core1_entry);

	// KEY SCAN
	// gpio
	for(uint8_t i=0;i<6;i++) {
		gpio_init(PIN_MUX0+i);
		gpio_set_dir(PIN_MUX0+i, GPIO_OUT);
		gpio_put(PIN_MUX0, 0);
	}
	// mem
	memset(state,true,sizeof(bool)*256); 
	// adc
	adc_init();
	adc_gpio_init(26);
	//adc_gpio_init(27);
	adc_set_round_robin(0xF);
	adc_select_input(0);
	adc_fifo_setup(true,false,1,false,false);
	adc_irq_set_enabled(true);
	adc_set_clkdiv(10000);
	irq_set_exclusive_handler(ADC_IRQ_FIFO, adc_handler);
	irq_set_enabled(ADC_IRQ_FIFO, true);
	adc_run(true);
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
void i2c_write_pair(uint8_t addr, uint8_t a, uint8_t b) {
	uint8_t d[2] = {a,b};
	//int ret = i2c_write_blocking(i2c_default, 0x34, d, 2, false);
	i2c_write_blocking(i2c_default, addr, d, 2, false);
	//if(ret<0) dbg("i2c write fail\n");
	//dbg("i2c: wrote %d bytes\n", ret);
}

void i2c_write_block(uint8_t addr, uint8_t *val, uint8_t len) {
	//int ret = i2c_write_blocking(i2c_default, 0x34, val, len, false);
	i2c_write_blocking(i2c_default, addr, val, len, false);
	//if(ret<0) dbg("i2c write fail\n");
}

/////////////////////////////////
void is31_init() {
	// config
	i2c_write_pair(IS0,0xA0,0x11); // 8x16, shutdown off
	i2c_write_pair(IS0,0xA1,0x40); // max Global Current
	i2c_write_pair(IS0,0xB2,0x02); // PWM FREQ
	i2c_write_pair(IS0,0xB0,0x91); // resistor settings
	i2c_write_pair(IS1,0xA0,0x11); // 8x16, shutdown off
	i2c_write_pair(IS1,0xA1,0x40); // max Global Current
	i2c_write_pair(IS1,0xB2,0x02); // PWM FREQ
	i2c_write_pair(IS1,0xB0,0x91); // resistor settings

	for(uint8_t i=0; i<16; i++) {
		i2c_write_pair(IS0,0x90+i,0xFF); // scaling
		i2c_write_pair(IS1,0x90+i,0xFF); // scaling
	}

	// clear
	map0[0] = PWM;
	map1[0] = PWM;
	memset(&(map0[1]), 0, 128);
	memset(&(map1[1]), 0, 128);
	i2c_write_block(IS0,map0,1+128);
	i2c_write_block(IS1,map1,1+128);
}

void is31_map() {
	i2c_write_pair(IS0,0xA1,IGAMMA[intensity]);
	i2c_write_pair(IS1,0xA1,IGAMMA[intensity]);
	i2c_write_block(IS0,map0,129);
	i2c_write_block(IS1,map1,129);
}

/////////////////////////////////
void draw_single(uint8_t x, uint8_t y, uint8_t z) {
	//dbg("draw_single\n");
	if(y<8) {
		uint8_t n = (uint8_t)(1+x+y*16); // +1 is for PWM at index 0
		//multicore_lockout_start_blocking();
		map0[n]=MGAMMA[z];
		//multicore_lockout_end_blocking();
	} else {
		uint8_t n = (uint8_t)(1+x+(y-8)*16); // +1 is for PWM at index 0
		//multicore_lockout_start_blocking();
		map1[n]=MGAMMA[z];
		//multicore_lockout_end_blocking();
	}
}

void draw_row(uint8_t xoff, uint8_t width, uint8_t y, uint8_t* data) {
	//dbg("draw_row\n");
	if(y<8) {
		int n = 1+xoff+(y*16); // +1 is for PWM at index 0
		//multicore_lockout_start_blocking();
		for(int i=0;i<width;i++) map0[n++]=MGAMMA[data[i]];
		//multicore_lockout_end_blocking();
	} else {
		int n = 1+xoff+((y-8)*16); // +1 is for PWM at index 0
		//multicore_lockout_start_blocking();
		for(int i=0;i<width;i++) map1[n++]=MGAMMA[data[i]];
		//multicore_lockout_end_blocking();
	}
}

void draw_all(uint8_t z) {
	//dbg("draw_all\n");
	//multicore_lockout_start_blocking();
	memset( &(map0[1]), MGAMMA[z], 128 );
	memset( &(map1[1]), MGAMMA[z], 128 );
	//multicore_lockout_end_blocking();
}


/////////////////////////////////
const uint8_t scan_o[4] = {128,8,0,136}; // why needed swap idx 0 and 2 ??
void adc_handler() {
	uint16_t result = 0;
	while(!adc_fifo_is_empty())
		result = adc_fifo_get();

	// adding this because somehow the roundrobin loses track:
	scan_a = 3-(uint8_t)adc_get_selected_input();

	//uint8_t n = (uint8_t)(scan_x+(scan_y*16)+(scan_a*8));
  uint8_t n = (uint8_t)(scan_x + (scan_y * 16) + scan_o[scan_a]);
	//dbg("%d\t%d\t%d\t%d\t%d\t%d\t0x%03x\n", t, n, scan_x, scan_y, scan_a, a, result);
	if(state[n] && (result < KEY_LO)) {
		state[n] = false;
		grid_add_event((uint8_t)(n%16),(uint8_t)n/16,1);
		//vm_handle_grid_key((uint8_t)(scan_x+scan_a*8),scan_y,1);
		//dbg("1 %d %d\t%d\n",scan_x,scan_y,n);
	}
	else if(!state[n] && (result > KEY_HI)) {
		state[n] = true;
		grid_add_event((uint8_t)(n%16),(uint8_t)n/16,0);
		//vm_handle_grid_key((uint8_t)(scan_x+scan_a*8),scan_y,0);
		//dbg("0 %d %d\t%d\n",scan_x,scan_y,n);
	}
	// advance mux
	/*scan_a ^= 1;
	if(scan_a == 0) {
		scan_x = (scan_x+1) & 0x7;
		if(scan_x == 0) {
			scan_y = (scan_y+1) & 0x7;
		}
		gpio_put_masked(0x3F << PIN_MUX0, (uint32_t)(((scan_x << 3) + scan_y) << PIN_MUX0));
	}*/

  // advance round robin counter
  scan_a++;
  if(scan_a > 3) scan_a = 0;
  // advance mux
  if(scan_a == 0) {
    scan_x = (uint8_t)((scan_x+1) % 8);
    if(scan_x == 0) {
      scan_y = (uint8_t)((scan_y+1) % 8);
    }
    // OPTIMIZED (update ports at once)
    gpio_put_masked(0x3F << PIN_MUX0, (uint32_t)(((scan_x << 3) + scan_y) << PIN_MUX0));
  }
}



//////////////////////////////////////////////////////////////////

void grid_add_event(uint8_t x, uint8_t y, uint8_t z) {
	if(grid_event_count < GRID_EVENTS_MAX) {
		grid_events[grid_event_count].x = x;
		grid_events[grid_event_count].y = y;
		grid_events[grid_event_count].z = z;
		grid_event_count++;
	}
}


//////////////////////////////////////////////////////////////////

static int l_grid_led(lua_State *l) {
	uint8_t x = (uint8_t) lua_tointeger(l, 1) - 1; // 1-based
	uint8_t y = (uint8_t) lua_tointeger(l, 2) - 1; // 1-based
	uint8_t z = (uint8_t) lua_tointeger(l, 3);
	if(x>GRID_XMASK || y>GRID_YMASK) return 0; // negatives wrap and fail
	if(z>15) z = 15; // just cap it
	draw_single(x,y,z);
	mmap[y*16+x]=z; // for rel
	return 0;
}

static int l_grid_led_rel(lua_State *l) {
	int nargs = lua_gettop(l);
	uint8_t x = (uint8_t) lua_tointeger(l, 1) - 1;
	uint8_t y = (uint8_t) lua_tointeger(l, 2) - 1;
	int8_t z = (int8_t) lua_tointeger(l, 3);
	if(x>GRID_XMASK || y>GRID_YMASK) return 0; // negatives wrap and fail
	uint8_t zmin = 0;
	uint8_t zmax = 15;
	if(nargs>3) zmin = (uint8_t) lua_tointeger(l, 4);
	if(nargs>4) zmax = (uint8_t) lua_tointeger(l, 5);
	mmap[y*16+x] = (uint8_t)clamp(z+mmap[y*16+x],zmin,zmax);
	draw_single(x,y,mmap[y*16+x]);
	return 0;
}

static int l_grid_led_get(lua_State *l) {
	uint8_t x = (uint8_t) lua_tointeger(l, 1) - 1;
	uint8_t y = (uint8_t) lua_tointeger(l, 2) - 1;
	x = x & GRID_XMASK;
	y = y & GRID_YMASK;
	lua_pushinteger(l, mmap[y*16+x]);
	return 1;
}

static int l_grid_led_all(lua_State *l) {
	uint8_t z = (uint8_t) lua_tointeger(l, 1);
	if(z>15) z = 15; // just cap it
	draw_all(z);
	for(uint16_t i=0;i<256;i++) mmap[i]=z; // for rel
	return 0;
}

static int l_grid_refresh(lua_State *l) {
	(void)l;
	refresh = true;
	return 0;
}

static int l_grid_size_x(lua_State *l) {
	lua_pushinteger(l, GRID_X);
	return 1;
}

static int l_grid_size_y(lua_State *l) {
	lua_pushinteger(l, GRID_Y);
	return 1;
}

static const struct luaL_Reg dlib[] = {
	{"grid_led", l_grid_led},
	{"grid_led_rel", l_grid_led_rel},
	{"grid_led_get", l_grid_led_get},
	{"grid_led_all", l_grid_led_all},
	{"grid_refresh", l_grid_refresh},
	{"grid_size_x", l_grid_size_x},
	{"grid_size_y", l_grid_size_y},
	{NULL, NULL}
};

const struct luaL_Reg *get_device_lib(void) {
	return dlib;
}

const char *device_help_str = "\
grid\n\
	event_grid(x,y,z)\n\
	grid_led_all(z)\n\
	grid_led(x,y,z)\n\
	grid_led_rel(x,y,z,zmin,zmax)\n\
	grid_led_get(x,y)\n\
	grid_refresh()\n\
	grid_size_x()\n\
	grid_size_y()\n\
";

extern const char *device_help_txt() {
	return device_help_str;
}


void vm_handle_grid_key(uint8_t x, uint8_t y, uint8_t z) {
	if (L == NULL) return;
	lua_getglobal(L, "event_grid");
	if (lua_isnil(L, -1)) {
		serial("event_grid: %d %d %d\r\n",x+1,y+1,z);
		lua_pop(L, 1); return; }
	lua_pushinteger(L, x+1); // 1-based
	lua_pushinteger(L, y+1); // 1-based
	lua_pushinteger(L, z);
	l_report(L, docall(L,3,0));
}


//////////////////////////////////////////////////////////////////

void device_task() {
	task();	
}

static uint8_t ewr = 0;
void device_task0() {
	if(grid_event_count>0) {
		vm_handle_grid_key(grid_events[ewr].x,grid_events[ewr].y,grid_events[ewr].z);
		ewr++;
		if(ewr==grid_event_count) {
			grid_event_count=0;
			ewr = 0;
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

	if(grid_event_count != txw) {
		if( tud_cdc_n_write_available(0)>3) {
			static uint8_t p[3];
			p[0] = grid_events[txw].z ? 0x21 : 0x20;
			p[1] = grid_events[txw].x;
			p[2] = grid_events[txw].y;
			tud_cdc_n_write(0, p, 3);
			tud_cdc_n_write_flush(0);
			txw++;
			//serial_tx(p,3);
			if(txw==grid_event_count) {
				txw=0;
				grid_event_count=0;
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

const char device_str2_0[] = "iii grid";
const char device_str2_1[] = "grid";

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
	}
	return 0;
}

void cmd_parse() {
	uint8_t* b = rx;
	b+=rxr;
  uint8_t cmd = b[0];
  uint8_t cmdlen = get_cmd_length(cmd);
  if( cmdlen == 0 ) {
		rxr=0;
		rxw=0;
    //dbg("=cmdfail for %d\n",b[0]); // abandon
		return;
	}
	else if(cmdlen > (rxw - rxr)) {
		//dbg("= PART = r%d w%d\n",rxr,rxw);
		return;
  } else { // check the whole packet is ready
    switch(cmd) {
      case SYS_QUERY:{
        //dbg("SYS_QUERY\n");
        uint8_t packet[3] = {0x0, 1, 4};
				tud_cdc_n_write(0, packet, 3);
				tud_cdc_n_write_flush(0);
        //serial_tx( packet, 3 ); // 4* (8*8) led grid's attached
        packet[1] = 4; // {0,2,2}
				tud_cdc_n_write(0, packet, 3);
				tud_cdc_n_write_flush(0);
        //serial_tx( packet, 3 ); // 4* (8*8) key grid's attached
        break;}
      case SYS_QUERY_ID:{
        //dbg("SYS_QUERY_ID\n");
        char packet[33];
        memset(packet, 0, 33);
        packet[0] = 0x1;
        strcpy(&packet[1], "monome zero");
				tud_cdc_n_write(0, packet, 33);
				tud_cdc_n_write_flush(0);
        //serial_tx((uint8_t*)packet, 33);
        break;}
      case SYS_GET_SIZE:{
        //dbg("SYS_GET_SIZE\n");
        uint8_t packet[3] = {0x03, 16, 16};
				tud_cdc_n_write(0, packet, 3);
				tud_cdc_n_write_flush(0);
        //serial_tx(packet, 3); // report grid size
        break;}

      case LED_OFF:
        draw_single(b[1], b[2], 0);
				refresh = true;
        break;
      case LED_ON:
        draw_single(b[1], b[2], 15);
				refresh = true;
        break;
      case LED_ALL_OFF:
        draw_all(0);
				refresh = true;
        break;
      case LED_ALL_ON:
        draw_all(15);
				refresh = true;
        break;
      case LED_MAP:{
        uint8_t xoff = b[1] & 0x8;
        uint8_t yoff = b[2] & 0x8;
        for( uint8_t y=0; y<8; y++ ){
          uint8_t brights[8];
          for( uint8_t x=0; x<8; x++ ){
            brights[x] = ((b[3+y]>>x) & 1) * 15;
          }
          draw_row(xoff, 8, y+yoff, brights);
        }
				refresh = true;
        break;}
      case LED_ROW:{
        uint8_t xoff = b[1] & 0x8; // floor to 0/8
				uint8_t n = (uint8_t)(b[2]*16 + xoff);
				//multicore_lockout_start_blocking();
        for( uint8_t i=0; i<8; i++ ){
          map0[1+n++] = MGAMMA[((b[3]>>i) & 1) * 15];
        }
				//multicore_lockout_end_blocking();
				refresh = true;
        break;}
      case LED_COL:{
        uint8_t yoff = b[2] & 0x8; // floor to 0/8
        for( uint8_t i=0; i<8; i++ ){
          draw_single( b[1]
          , yoff+i
          , ((b[3]>>i) & 1) * 15);
        }
				refresh = true;
        break;}
      case LED_INTENSITY:
				intensity = b[1];
				refresh = true;
        break;
      case LED_LEVEL:
        draw_single(b[1], b[2], b[3]);
				refresh = true;
        break;
      case LED_LEVEL_ALL:
        draw_all(b[1]);
				refresh = true;
        break;
			case LED_LEVEL_MAP:{
				uint8_t xoff = b[1] & 0x8;
				uint8_t yoff = b[2] & 0x8;
				uint8_t (*map)[129];
				if(yoff == 0) {
					map = &map0;
				} else {
					map = &map1;
				}
				//multicore_lockout_start_blocking();
				for( uint8_t y=0; y<8; y++ ){
					for( uint8_t xx=0; xx<4; xx++ ){
						uint8_t z = b[3+xx+y*4];
						uint8_t xxx = (uint8_t)(xoff+xx*2 + y*16);
						uint8_t zzz = z >> 4;
						(*map)[1+xxx] = MGAMMA[zzz];
						xxx++;
						zzz = z & 0xF;
						(*map)[1+xxx] = MGAMMA[zzz];
					}
				}
				//multicore_lockout_end_blocking();
				refresh = true;
				break;}
			case LED_LEVEL_ROW:{
				uint8_t xoff = b[1] & 0x8; // floor to 0/8
				uint8_t brights[8];
				for( int xx=0; xx<4; xx++ ){
					brights[xx*2] = b[3+xx] >> 4;
					brights[xx*2+1] = b[3+xx] & 0xF;
				}
				draw_row(xoff, 8, b[2], brights);
				refresh = true;
				break;}
			case LED_LEVEL_COL:{
				uint8_t yoff = b[2] & 0x8; // floor to 0/8
				for( int yy=0; yy<4; yy++ ){
					draw_single( b[1]
						, (uint8_t)(yoff+yy*2)
						, b[3+yy] >> 4 );
					draw_single( b[1]
						, (uint8_t)(yoff+yy*2+1)
						, b[3+yy] & 0xF);
				}
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
	double a = r()*4.0;
	double b = r()*8.0;
	double t = r()/3.0;
	double tt = r()/5.0;

	uint8_t frame = 0;
	while(frame++ < FRAMECOUNT) {
		for(int y=0;y<8;y++)
			for(int x=0;x<16;x++) {
				map0[1+y*16+x] = MGAMMA[(int)(cos(x*a+frame*t)+sin(y*b+frame*tt)*16*(double)(FRAMECOUNT-frame)/(double)FRAMECOUNT)];
			}
		for(int y=8;y<16;y++)
			for(int x=0;x<16;x++) {
				map1[1+(y-8)*16+x] = MGAMMA[(int)(cos(x*a+frame*t)+sin(y*b+frame*tt)*16*(double)(FRAMECOUNT-frame)/(double)FRAMECOUNT)];
			}

		is31_map();
		sleep_ms(20);
	}
}
