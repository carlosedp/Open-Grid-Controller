// Open Grid firmware: monome iii (Lua) on the RP2040, plus a plain monome
// serial mode for norns/serialosc. Device glue for third_party/iii/device.h,
// modelled on monome's iii-grid-2022.
//
// Mode is kept in iii's flash status byte: 0 = iii, 1 = monome serial.
// Switch modes by holding the top-left key while plugging in, holding the
// BOOT button for 2 s while running, or calling og_set_mode() from Lua.

#define DEVICE_VERSION "og-0.2.7"

#include <string.h>

#include "hardware/structs/ioqspi.h"
#include "hardware/structs/sio.h"
#include "hardware/sync.h"
#include "hardware/watchdog.h"
#include "pico/bootrom.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "tusb.h"

#include "lauxlib.h"
#include "lua.h"

#include "device.h"
#include "flash.h"
#include "iii.h"
#include "serial.h"
#include "util.h"
#include "vm.h"

#include "grid.h"
#include "heap_stats.h"
#include "monome_serial.h"
#include "status_led.h"

#define MODE_III 0
#define MODE_SERIAL 1

// Key held at power-up to toggle the mode (iii-grid-2022 uses a hidden key).
#define MODE_KEY_X 0
#define MODE_KEY_Y 0
// Keypad boards need time to boot after power is applied.
#define KEYPAD_BOOT_WAIT_MS 500
// Keep holding this long after switching to iii to skip init.lua (clean boot).
#define CLEAN_BOOT_HOLD_MS 1000

#define BOOTSEL_POLL_MS 100
#define BOOTSEL_HOLD_MS 2000
// After power-up, a BOOT press within this window means "clean boot".
#define BOOT_WINDOW_MS 1000

// LED mirror for diii's virtual grid: at most one frame per this interval.
#define MIRROR_INTERVAL_MS 33

static uint8_t mode;

//////////////////////////////////////////////////////////////////
// boot / mode handling

bool check_device_key() {
  bool pressed = false;
  return og_keypad_read_key_raw(MODE_KEY_X, MODE_KEY_Y, &pressed) && pressed;
}

// Light the first row as a progress bar while the clean-boot hold counts up.
static void show_hold_progress(uint32_t elapsed_ms) {
  const uint32_t lit = elapsed_ms * OG_GRID_W / CLEAN_BOOT_HOLD_MS;
  for (uint8_t x = 0; x < OG_GRID_W; x++) og_grid_led(x, 0, x < lit ? 8 : 0);
  og_grid_flush_now();
}

// Persist a new mode and reboot. Safe to call once core 1 is running.
static void set_mode_and_reboot(uint8_t m) {
  multicore_lockout_start_blocking();
  const uint32_t ints = save_and_disable_interrupts();
  flash_write_mode(m);
  restore_interrupts(ints);
  multicore_lockout_end_blocking();
  watchdog_reboot(0, 0, 10);
  while (true) tight_loop_contents();
}

// The BOOT button shares the flash chip-select line; it can be read by
// briefly releasing CS, with nothing on either core executing from flash.
static bool __no_inline_not_in_flash_func(read_bootsel_raw)(void) {
  const uint cs_index = 1;
  hw_write_masked(&ioqspi_hw->io[cs_index].ctrl,
                  GPIO_OVERRIDE_LOW << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                  IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);
  for (volatile int i = 0; i < 1000; ++i) {
  }
  const bool pressed = !(sio_hw->gpio_hi_in & (1u << cs_index));
  hw_write_masked(&ioqspi_hw->io[cs_index].ctrl,
                  GPIO_OVERRIDE_NORMAL << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                  IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);
  return pressed;
}

// Before core 1 starts there is nothing to lock out.
static bool bootsel_pressed_early(void) {
  const uint32_t ints = save_and_disable_interrupts();
  const bool pressed = read_bootsel_raw();
  restore_interrupts(ints);
  return pressed;
}

// Recovery without keypads: holding BOOT while plugging in enters the ROM
// bootloader, so instead watch for a press right after power-up. A press
// means "clean boot": iii mode with init.lua skipped, so a script that hangs
// at startup can't lock you out of the REPL.
static bool boot_button_window(void) {
  og_status_set_base(OG_COLOR_BOOT_WINDOW);
  og_status_task();
  const absolute_time_t until = make_timeout_time_ms(BOOT_WINDOW_MS);
  while (!time_reached(until)) {
    if (bootsel_pressed_early()) return true;
    sleep_ms(10);
  }
  return false;
}

static bool bootsel_pressed(void) {
  multicore_lockout_start_blocking();
  const uint32_t ints = save_and_disable_interrupts();
  const bool pressed = read_bootsel_raw();
  restore_interrupts(ints);
  multicore_lockout_end_blocking();
  return pressed;
}

static void bootsel_task(void) {
  static absolute_time_t next_poll;
  static uint32_t held_ms = 0;
  // Give core 1 time to register for lockout before the first poll.
  if (is_nil_time(next_poll)) next_poll = make_timeout_time_ms(1000);
  if (!time_reached(next_poll)) return;
  next_poll = make_timeout_time_ms(BOOTSEL_POLL_MS);

  held_ms = bootsel_pressed() ? held_ms + BOOTSEL_POLL_MS : 0;
  if (held_ms > 0) og_status_flash(OG_COLOR_MODE_SWITCH, 2 * BOOTSEL_POLL_MS);
  if (held_ms >= BOOTSEL_HOLD_MS) {
    set_mode_and_reboot(mode == MODE_III ? MODE_SERIAL : MODE_III);
  }
}

// Opening the serial port at 1200 baud reboots into the USB bootloader, so
// `pio run -t upload` (and the Arduino IDE convention) works without the
// BOOT button.
void tud_cdc_line_coding_cb(uint8_t itf, cdc_line_coding_t const *coding) {
  (void)itf;
  if (coding->bit_rate == 1200) rom_reset_usb_boot(0, 0);
}

// Quick left-to-right sweep on the grid when iii starts (skipped without keypads).
static void boot_sweep(void) {
  if (!og_keypad_connected_mask()) return;
  for (int x = 0; x <= OG_GRID_W; x++) {
    for (uint8_t y = 0; y < OG_GRID_H; y++) {
      if (x < OG_GRID_W) og_grid_led((uint8_t)x, y, 10);
      if (x > 0) og_grid_led((uint8_t)(x - 1), y, 2);
      if (x > 1) og_grid_led((uint8_t)(x - 2), y, 0);
    }
    og_grid_refresh();
    sleep_ms(15);
  }
  og_grid_led_all(0);
  og_grid_refresh();
}

int main(void) {
  flash_init();
  og_status_init();
  og_grid_init();
  og_keypad_wait_ready(KEYPAD_BOOT_WAIT_MS);

  // Same rules as monome's grids: holding the key at power-up toggles the
  // mode; when switching to iii, keep holding to also skip init.lua.
  bool run_script = true;
  mode = flash_read_mode();
  if (boot_button_window()) {
    if (mode != MODE_III) flash_write_mode(MODE_III);
    mode = MODE_III;
    run_script = false;
  } else if (check_device_key()) {
    if (mode == MODE_SERIAL) {
      flash_write_mode(MODE_III);
      mode = MODE_III;
      const absolute_time_t start = get_absolute_time();
      uint32_t held = 0;
      while (check_device_key() && held < CLEAN_BOOT_HOLD_MS) {
        show_hold_progress(held);
        sleep_ms(10);
        held = (uint32_t)(absolute_time_diff_us(start, get_absolute_time()) / 1000);
      }
      if (held >= CLEAN_BOOT_HOLD_MS) run_script = false;
      og_grid_led_all(0);
      og_grid_flush_now();
    } else {
      flash_write_mode(MODE_SERIAL);
      mode = MODE_SERIAL;
    }
  }

  og_status_set_base(mode == MODE_SERIAL ? OG_COLOR_SERIAL
                     : run_script         ? OG_COLOR_III
                                          : OG_COLOR_III_CLEAN);
  og_status_task();

  tud_init(BOARD_TUD_RHPORT);
  og_grid_start();

  if (mode == MODE_III) {
    boot_sweep();
    iii_loop(run_script);  // never returns; calls device_task()
  }
  while (true) {
    tud_task();
    device_task();
  }
}

//////////////////////////////////////////////////////////////////
// iii device interface

void device_init() {}

static void vm_handle_grid_key(uint8_t x, uint8_t y, uint8_t z) {
  if (L == NULL) return;
  lua_getglobal(L, "event_grid");
  if (lua_isnil(L, -1)) {
    lua_pop(L, 1);
    return;
  }
  lua_pushinteger(L, x + 1);  // 1-based
  lua_pushinteger(L, y + 1);
  lua_pushinteger(L, z);
  if (l_report(L, docall(L, 3, 0)) != LUA_OK) og_status_flash(OG_COLOR_ERROR, 300);
}

static bool mirror_on = false;

// Send the LED frame as "^^grid(<128 hex levels>)", diii's event-line format.
static void mirror_task(void) {
  static absolute_time_t next;
  if (!mirror_on) return;
  if (!tud_cdc_connected()) {
    mirror_on = false;  // diii went away; it re-enables on connect
    return;
  }
  if (!time_reached(next) || !og_grid_take_changed()) return;
  next = make_timeout_time_ms(MIRROR_INTERVAL_MS);

  static const char hex[] = "0123456789abcdef";
  uint8_t levels[OG_KEY_COUNT];
  char line[sizeof("^^grid()\r\n") + OG_KEY_COUNT];
  og_grid_snapshot(levels);
  char *p = line;
  memcpy(p, "^^grid(", 7);
  p += 7;
  for (int i = 0; i < OG_KEY_COUNT; i++) *p++ = hex[levels[i] & 0xF];
  memcpy(p, ")\r\n", 3);
  p += 3;
  serial_tx((const uint8_t *)line, (uint32_t)(p - line));
}

void device_task() {
  bootsel_task();
  og_heap_task();
  og_status_task();
  if (mode == MODE_SERIAL) {
    monome_serial_task();
    return;
  }
  // One event per pass so USB and metros keep running under key bursts.
  og_key_event_t e;
  if (og_grid_pop_event(&e)) {
    if (e.z) og_status_flash(OG_COLOR_KEY, 30);
    vm_handle_grid_key(e.x, e.y, e.z);
  }
  mirror_task();
}

const char *device_id() { return "grid"; }
const char *device_version() { return DEVICE_VERSION; }
const char *device_str1() { return mode == MODE_SERIAL ? "monome" : "Open Grid"; }
const char *device_str2() { return mode == MODE_SERIAL ? "grid" : "iii open grid"; }

//////////////////////////////////////////////////////////////////
// lua: grid (same API as monome's iii grids)

static int l_grid_led(lua_State *l) {
  const uint8_t x = (uint8_t)lua_tointeger(l, 1) - 1;  // 1-based, negatives wrap and fail
  const uint8_t y = (uint8_t)lua_tointeger(l, 2) - 1;
  int z = (int)lua_tointeger(l, 3);
  const bool rel = lua_toboolean(l, 4);
  if (x >= OG_GRID_W || y >= OG_GRID_H) return 0;
  if (rel) z += og_grid_led_get(x, y);
  og_grid_led(x, y, (uint8_t)clamp(z, 0, 15));
  return 0;
}

static int l_grid_led_get(lua_State *l) {
  const uint8_t x = ((uint8_t)lua_tointeger(l, 1) - 1) & (OG_GRID_W - 1);
  const uint8_t y = ((uint8_t)lua_tointeger(l, 2) - 1) & (OG_GRID_H - 1);
  lua_pushinteger(l, og_grid_led_get(x, y));
  return 1;
}

static int l_grid_led_all(lua_State *l) {
  const int z = (int)lua_tointeger(l, 1);
  const bool rel = lua_toboolean(l, 2);
  if (!rel) {
    og_grid_led_all((uint8_t)clamp(z, 0, 15));
    return 0;
  }
  for (uint8_t y = 0; y < OG_GRID_H; y++) {
    for (uint8_t x = 0; x < OG_GRID_W; x++) {
      og_grid_led(x, y, (uint8_t)clamp(z + og_grid_led_get(x, y), 0, 15));
    }
  }
  return 0;
}

static int l_grid_intensity(lua_State *l) {
  og_grid_set_intensity((uint8_t)clamp((int)lua_tointeger(l, 1), 0, 15));
  return 0;
}

static int l_grid_refresh(lua_State *l) {
  (void)l;
  og_grid_refresh();
  return 0;
}

static int l_grid_size_x(lua_State *l) {
  lua_pushinteger(l, OG_GRID_W);
  return 1;
}

static int l_grid_size_y(lua_State *l) {
  lua_pushinteger(l, OG_GRID_H);
  return 1;
}

//////////////////////////////////////////////////////////////////
// lua: open grid extras

static int l_og_set_mode(lua_State *l) {
  const int m = (int)lua_tointeger(l, 1);
  if (m != MODE_III && m != MODE_SERIAL) return 0;
  set_mode_and_reboot((uint8_t)m);
  return 0;
}

static int l_og_keypads(lua_State *l) {
  lua_pushinteger(l, og_keypad_connected_mask());
  return 1;
}

// Prints C heap stats (KB) and returns them in bytes:
// free, largest, used, peak, total, high_water. og_mem(true) only returns them.
static int l_og_mem(lua_State *l) {
  og_heap_stats_t h;
  og_heap_stats(&h);
  if (!lua_toboolean(l, 1)) {
    serial("heap KB: free %.1f, largest block %.1f, used %.1f, peak %.1f, "
           "high water %.1f of %.1f\r\n",
           h.free / 1024.0, h.largest / 1024.0, h.used / 1024.0, h.peak / 1024.0,
           h.high_water / 1024.0, h.total / 1024.0);
  }
  lua_pushinteger(l, h.free);
  lua_pushinteger(l, h.largest);
  lua_pushinteger(l, h.used);
  lua_pushinteger(l, h.peak);
  lua_pushinteger(l, h.total);
  lua_pushinteger(l, h.high_water);
  return 6;
}

// og_mirror(true) streams LED frames as ^^grid(...) lines (used by diii's virtual grid).
static int l_og_mirror(lua_State *l) {
  mirror_on = lua_toboolean(l, 1);
  if (mirror_on) og_grid_refresh();  // send a first frame right away
  return 0;
}

// og_key(x, y, z): inject a key event as if it came from the grid (1-based).
static int l_og_key(lua_State *l) {
  const int x = (int)lua_tointeger(l, 1) - 1;
  const int y = (int)lua_tointeger(l, 2) - 1;
  if (x < 0 || y < 0) return 0;
  og_grid_inject_key((uint8_t)x, (uint8_t)y, (uint8_t)(lua_tointeger(l, 3) ? 1 : 0));
  return 0;
}

// og_led(r, g, b) sets the status LED (0-255 each); og_led() returns it to the mode color.
static int l_og_led(lua_State *l) {
  if (lua_gettop(l) == 0) {
    og_status_clear_user();
    return 0;
  }
  og_status_set_user(OG_RGB(clamp((int)lua_tointeger(l, 1), 0, 255),
                            clamp((int)lua_tointeger(l, 2), 0, 255),
                            clamp((int)lua_tointeger(l, 3), 0, 255)));
  return 0;
}

static const struct luaL_Reg device_lib[] = {
    {"grid_led", l_grid_led},
    {"grid_led_get", l_grid_led_get},
    {"grid_led_all", l_grid_led_all},
    {"grid_intensity", l_grid_intensity},
    {"grid_refresh", l_grid_refresh},
    {"grid_size_x", l_grid_size_x},
    {"grid_size_y", l_grid_size_y},
    {"og_set_mode", l_og_set_mode},
    {"og_keypads", l_og_keypads},
    {"og_mem", l_og_mem},
    {"og_mirror", l_og_mirror},
    {"og_key", l_og_key},
    {"og_led", l_og_led},
    {NULL, NULL},
};

const struct luaL_Reg *get_device_lib(void) { return device_lib; }

static const char *device_help_str =
    "grid\n"
    "  event_grid(x,y,z)\n"
    "  grid_led_all(z,rel)\n"
    "  grid_led(x,y,z,rel)\n"
    "  grid_led_get(x,y)\n"
    "  grid_intensity(z)\n"
    "  grid_refresh()\n"
    "  grid_size_x()\n"
    "  grid_size_y()\n"
    "open grid\n"
    "  og_set_mode(m)  0 = iii, 1 = monome serial (reboots)\n"
    "  og_keypads()    bitmask of keypad boards online\n"
    "  og_mem(quiet)   C heap: free, largest block, used, peak, total\n"
    "  og_key(x,y,z)   inject a key event, as if pressed on the grid\n"
    "  og_led(r,g,b)   status LED color (0-255); og_led() = mode color\n"
    "  og_mirror(on)   stream LED frames as ^^grid(...) for diii\n";

const char *device_help_txt() { return device_help_str; }
