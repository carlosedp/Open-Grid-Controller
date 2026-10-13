#include "hardware/watchdog.h"
#include "pico/bootrom.h"

#include "fs.h"
#include "repl.h"
#include "serial.h"
#include "vm.h"

//------------------------------------------------
//--- constants and state

//--- uploads (^^s ... ^^f / ^^w)
// received lines collect in a small RAM chunk; once that fills, they stream
// into a temporary file, so an upload never needs a buffer the size of the
// script (and scripts aren't limited by such a buffer)
#define UPLOAD_CHUNK_SIZE 512
#define UPLOAD_TMP_FILE ".upload.tmp"
static char upload_chunk[UPLOAD_CHUNK_SIZE + 1]; // +1 for a terminating '\0'
static uint32_t upload_len = 0;    // bytes waiting in upload_chunk
static uint32_t upload_total = 0;  // bytes received in this upload
static bool upload_spilled = false; // upload_file is open and holds data
static bool upload_ok = true;       // false after a temp file write error
static lfs_file_t upload_file;
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

// end the current upload; the temp file is removed
static void reset_script_rx() {
  if (upload_spilled) {
    fs_file_close(&upload_file);
    upload_spilled = false;
  }
  fs_remove(UPLOAD_TMP_FILE);
  upload_len = 0;
  upload_total = 0;
  upload_ok = true;
  in_rx_script = false;
}

// move upload_chunk into the temp file (opening it on first use)
static bool upload_flush() {
  if (upload_len == 0)
    return true;
  if (!upload_spilled) {
    const int flags = LFS_O_CREAT | LFS_O_WRONLY | LFS_O_TRUNC;
    if (fs_file_open(&upload_file, UPLOAD_TMP_FILE, flags) != 0)
      return false;
    upload_spilled = true;
  }
  if (fs_file_write(&upload_file, upload_chunk, upload_len) != (int32_t)upload_len)
    return false;
  upload_len = 0;
  return true;
}

static void upload_append(const char *data, uint32_t len) {
  upload_total += len;
  while (len > 0 && upload_ok) {
    if (upload_len == UPLOAD_CHUNK_SIZE && !upload_flush()) {
      upload_ok = false;
      break;
    }
    uint32_t n = UPLOAD_CHUNK_SIZE - upload_len;
    if (n > len)
      n = len;
    memcpy(upload_chunk + upload_len, data, n);
    upload_len += n;
    data += n;
    len -= n;
  }
}

// write the received script to `filename`, if it compiles
static void upload_write_file() {
  if (!upload_spilled) {
    // small enough to have stayed in RAM
    upload_chunk[upload_len] = '\0';
    if (!vm_test_buffer(upload_chunk, upload_len, filename)) {
      serial("-- compilation fail!\r\n");
      return;
    }
    lfs_file_t file;
    const int flags = LFS_O_CREAT | LFS_O_WRONLY | LFS_O_TRUNC;
    if (fs_file_open(&file, filename, flags) == 0) {
      // +1 to write \0
      fs_file_write(&file, upload_chunk, upload_len + 1);
      fs_file_close(&file);
      serial("-- file written: %s\r\n", filename);
    } else {
      serial("-- file write fail!\r\n");
    }
    return;
  }

  // streamed: finish the temp file (ending in \0, like files written
  // above), compile-check it from flash, then rename it into place
  upload_append("", 1);
  const bool written = upload_ok && upload_flush();
  fs_file_close(&upload_file);
  upload_spilled = false;
  if (!written) {
    serial("-- file write fail!\r\n");
  } else if (!vm_test_file(UPLOAD_TMP_FILE, filename)) {
    serial("-- compilation fail!\r\n");
  } else if (fs_rename(UPLOAD_TMP_FILE, filename) == 0) {
    serial("-- file written: %s\r\n", filename);
  } else {
    serial("-- file write fail!\r\n");
  }
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
    reset_script_rx(); // drop any unfinished upload
    in_rx_script = true;
    serial("-- receiving data\r\n");
    break;

  case REPL_CMD_END_FILENAME:
    if(in_rx_script) {
      if (upload_total < 1) {
        serial("-- no data received\r\n");
      } else if (upload_spilled || upload_len >= sizeof(filename)) {
        serial("-- bad filename!\r\n");
      } else {
        // need to strip the \r\n
        char *name = upload_chunk;
        uint32_t i = upload_len;
        name[i] = '\0';
        while (i > 0 && (name[i - 1] == '\r' || name[i - 1] == '\n')) {
          i--;
          name[i] = '\0';
        }
        serial("-- set filename: %s\r\n", name);
        // test if filename is ok
        const int flags = LFS_O_CREAT | LFS_O_WRONLY;
        lfs_file_t file;
        if (fs_file_open(&file, name, flags) == 0) {
          fs_file_close(&file);
          if(fs_file_size(&file)==0) fs_remove(name);
          strcpy(filename, name);
        } else {
          serial("-- bad filename!\r\n");
        }
      }
      reset_script_rx();
    } else {
      serial("-- file name end without start");
    }
    break;

  case REPL_CMD_END_WRITE:
    if(in_rx_script) {
      serial("-- write file\r\n");
      if (upload_total < 1) {
        serial("-- no data received\r\n");
      } else {
        // test compilation of received script, print any errors
        upload_write_file();
      }
      reset_script_rx();
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
        // add line to the upload
        upload_append(line_buf, line_buf_pos);
        upload_append("\n", 1);
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
