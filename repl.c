#include "hardware/watchdog.h"
#include "pico/bootrom.h"

#include "fs.h"
#include "repl.h"
#include "serial.h"
#include "vm.h"

//------------------------------------------------
//--- constants and state

//--- input buffers
#define SCRIPT_BUFFER_SIZE 32767
// #define SCRIPT_BUFFER_SIZE 16383
//  extra byte for null terminator
//static char script_buf[SCRIPT_BUFFER_SIZE + 1];
static char *script_buf;
static uint32_t script_rx_pos = 0;
static char filename[256] = "init.lua";

// FIXME: made the buffer huge for import/export flash via prints
// could more sensibly do this at a lower level and save RAM
// FIXED?? this isn't necessary, linebuffer is for repl only, file stuff is managed with another buffer that is malloc'd. idk what a sensible linebuffer size is though, so 512 seems fine
// FIXME: choose linebuffer size sensibly!
//#define LINE_BUFFER_SIZE 16383
#define LINE_BUFFER_SIZE 512
static char line_buf[LINE_BUFFER_SIZE + 1];
static uint32_t line_buf_pos = 0;

//--- other state
bool in_rx_script = false;

enum {
  REPL_CMD_CLEAN = 'C',
  REPL_CMD_INIT = 'I',
  REPL_CMD_RESET = 'R',
  REPL_CMD_START_DATA = 'S',
  REPL_CMD_END_FILENAME = 'F',
  REPL_CMD_END_WRITE = 'W',
  REPL_CMD_PRINT = 'P',
  REPL_CMD_GET_FILENAME = 'G',
  REPL_CMD_BOOTLOAD = 'B'
};

//------------------------------------------------
//---- static helper functions

static void reset_script_rx() {
  script_rx_pos = 0;
  in_rx_script = false;
}

//------------------------------------------------
//--- external functions

void repl_init() {
  // nothing to do
}

bool handle_command(char cmd) {
  switch (cmd) {
  case REPL_CMD_CLEAN:
    serial("-- re-init with no script\r\n");
    vm_deinit();
    vm_init(false);
    break;

  case REPL_CMD_INIT:
    serial("-- re-init\r\n");
    vm_deinit();
    vm_init(true);
    break;

  case REPL_CMD_BOOTLOAD:
    serial("-- launching bootloader...\r\n");
    rom_reset_usb_boot(0, 0);
    break;

  case REPL_CMD_RESET:
    serial("-- reset device...\r\n");
    watchdog_reboot(0, 0, 1000);
    break;

  case REPL_CMD_START_DATA:
    script_buf = malloc(SCRIPT_BUFFER_SIZE + 1);
    if(script_buf == NULL) {
      serial("-- memory allocation fail!\r\n");
    } else {
      in_rx_script = true;
      script_rx_pos = 0;
      serial("-- receiving data\r\n");
    }
    break;

  case REPL_CMD_END_FILENAME:
    if(in_rx_script) {
      script_buf[script_rx_pos] = '\0';
      if (script_rx_pos < 1) {
        serial("-- no data received\r\n");
      } else {
        // need to strip the \r\n
        script_buf[script_rx_pos] = '\0';
        uint8_t i = (uint8_t)script_rx_pos;
        while (i > 0 &&
               (script_buf[i - 1] == '\r' || script_buf[i - 1] == '\n')) {
          i--;
          script_buf[i] = '\0';
        }
        serial("-- set filename: %s\r\n", script_buf);
        // test if filename is ok
        const int flags = LFS_O_CREAT | LFS_O_WRONLY;
        lfs_file_t file;
        if (fs_file_open(&file, script_buf, flags) == 0) {
          fs_file_close(&file);
          if(fs_file_size(&file)==0) fs_remove(script_buf);
          strcpy(filename, script_buf);
        } else {
          serial("-- bad filename!\r\n");
        }
      }
      reset_script_rx();
      free(script_buf);
    } else {
      serial("-- file name end without start");
    }
    break;

  case REPL_CMD_END_WRITE:
    if(in_rx_script) {
      serial("-- write file\r\n");
      script_buf[script_rx_pos] = '\0';
      if (script_rx_pos < 1) {
        serial("-- no data received\r\n");
      } else {
        // test compilation of received script, print any errors
        if (vm_test_script(script_buf)) {
          lfs_file_t file;
          if (fs_file_open(&file, filename, LFS_O_CREAT | LFS_O_WRONLY) == 0) {
            // +1 to write \0
            fs_file_write(&file, script_buf, strlen(script_buf) + 1);
            fs_file_close(&file);
            serial("-- file written: %s\r\n", filename);
          } else {
            serial("-- file write fail!\r\n");
          }
        } else {
          serial("-- compilation fail!\r\n");
        }
      }
      reset_script_rx();
      free(script_buf);
    } else {
      serial("-- file write end without start");
    }
    break;

  case REPL_CMD_GET_FILENAME:
    serial("-- filename: %s\r\n", filename);
    break;

  case REPL_CMD_PRINT:
    lfs_file_t rfile;
    if (fs_file_open(&rfile, filename, LFS_O_RDONLY) == 0) {
      const unsigned int sz = (unsigned int)fs_file_size(&rfile);
      if (sz > 0) {
        char *buf = malloc(sz + 1);
        buf[sz] = 0;
        fs_file_read(&rfile, buf, sz);
        serial(buf);
        free(buf);
      } else {
        serial("-- empty file\r\n");
      }
      fs_file_close(&rfile);
    } else {
      serial("-- no file\r\n");
    }
    break;

  default:
    serial("-- unknown command\r\n");
    return false;
  }
  return true;
}

void repl_handle_byte(uint8_t b) {
  if (b == '\n' || b == '\r') {
    // if (b == '\n') {
    if (line_buf[0] == '^' && line_buf[1] == '^') {
      char c = line_buf[2];
      if (c >= 97 && c <= 122) {
        // convert lowercase to uppercase ascii
        c -= 32;
      }
      if (!handle_command(c)) {
        serial("-- unknown command\r\n");
      }
      line_buf[2] = 0;
    } else {
      // not a command
      if (in_rx_script) {
        // add line to script buffer
        if (script_rx_pos + line_buf_pos < (SCRIPT_BUFFER_SIZE - 1)) {
          memcpy(script_buf + script_rx_pos, line_buf, line_buf_pos);
          script_buf[script_rx_pos + line_buf_pos] = '\n';
          script_rx_pos += line_buf_pos + 1;
        } else {
          serial("-- script buffer full!\r\n");
          reset_script_rx();
        }
      } else {
        // compile and execute the line as lua source, printing any errors
        line_buf[line_buf_pos] = '\0';
        vm_run_buffer(line_buf);
      }
    }
    line_buf_pos = 0;
    // ECHO
    // printf("\r\n");
    return;
  }

  // ECHO
  // non-line-ending characters
  //serial("%c", b);
  if (line_buf_pos < LINE_BUFFER_SIZE) {
    line_buf[line_buf_pos++] = b;
  } else {
    // good chance we're getting sent serialosc gibberish?
    // so let's abandon it
    // serial("-- line buffer full!\r\n");
    line_buf_pos = 0;
  }
}

void repl_handle_bytes(uint8_t *data, uint32_t len) {
  if (len < 1)
    return;

  for (unsigned int i = 0; i < len; i++) {
    repl_handle_byte(data[i]);
  }
}
