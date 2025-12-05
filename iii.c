#include "iii.h"
#include "device.h"
#include "flash.h"
#include "fs.h"
#include "metro.h"
#include "midi.h"
#include "pico/stdlib.h"
#include "repl.h"
#include "serial.h"
#include "tusb.h"
#include "vm.h"

void iii_loop(bool run_script) {
  // must come after flash_init()
  fs_init();
  // !!! LFS fails to mount without this arbitrary delay;
  // !!! would be nice to know why / make it programmatic
  sleep_ms(50);
  fs_mount();

  metro_init();
  vm_init(run_script);
  repl_init();
  midi_set_rx_callback(vm_handle_midi);
  serial_set_rx_callback(repl_handle_bytes);

  while (1) {
    tud_task();
    midi_task();
    metro_task();
    serial_task();
    device_task();
  }
}
