// USB descriptors, chosen by the mode stored in flash:
//   iii mode    : CDC (REPL) + MIDI, VID:PID cafe:1101
//   serial mode : CDC only,          VID:PID cafe:1110
//
// These are the IDs monome's iii grids use. monome's hosted diii only offers
// ports matching cafe:1101, and 0xCAFE is TinyUSB's shared test VID. A
// pid.codes allocation for Open Grid is on the roadmap.
//
// Based on the TinyUSB examples (MIT, Copyright (c) 2019 Ha Thach) and
// monome's iii-grid-2022.

#include <stdio.h>
#include <string.h>

#include "pico/unique_id.h"
#include "tusb.h"

#include "device.h"
#include "flash.h"

#define USB_VID 0xCAFE
#define USB_BCD 0x0200

static const tusb_desc_device_t desc_device_iii = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = USB_BCD,
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = USB_VID,
    .idProduct = 0x1101,
    .bcdDevice = 0x0100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01,
};

static const tusb_desc_device_t desc_device_serial = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = USB_BCD,
    .bDeviceClass = TUSB_CLASS_CDC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = USB_VID,
    .idProduct = 0x1110,
    .bcdDevice = 0x0100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01,
};

static bool serial_mode(void) { return flash_read_mode() == 1; }

uint8_t const *tud_descriptor_device_cb(void) {
  return (uint8_t const *)(serial_mode() ? &desc_device_serial : &desc_device_iii);
}

enum { ITF_III_CDC, ITF_III_CDC_DATA, ITF_III_MIDI, ITF_III_MIDI_STREAMING, ITF_III_TOTAL };
enum { ITF_SERIAL_CDC, ITF_SERIAL_CDC_DATA, ITF_SERIAL_TOTAL };

#define EPNUM_CDC_NOTIF 0x81
#define EPNUM_CDC_OUT 0x02
#define EPNUM_CDC_IN 0x82
#define EPNUM_MIDI_OUT 0x05
#define EPNUM_MIDI_IN 0x85

#define CONFIG_III_LEN (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_MIDI_DESC_LEN)
#define CONFIG_SERIAL_LEN (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN)

static const uint8_t desc_configuration_iii[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_III_TOTAL, 0, CONFIG_III_LEN, 0x00, 100),
    TUD_CDC_DESCRIPTOR(ITF_III_CDC, 4, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),
    TUD_MIDI_DESCRIPTOR(ITF_III_MIDI, 0, EPNUM_MIDI_OUT, EPNUM_MIDI_IN, 64),
};

static const uint8_t desc_configuration_serial[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_SERIAL_TOTAL, 0, CONFIG_SERIAL_LEN, 0x00, 100),
    TUD_CDC_DESCRIPTOR(ITF_SERIAL_CDC, 4, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),
};

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
  (void)index;
  return serial_mode() ? desc_configuration_serial : desc_configuration_iii;
}

enum { STRID_LANGID = 0, STRID_MANUFACTURER, STRID_PRODUCT, STRID_SERIAL, STRID_CDC, STRID_MIDI };

static uint16_t desc_str[32 + 1];

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  (void)langid;
  char serial[12];
  const char *str;

  switch (index) {
    case STRID_LANGID:
      desc_str[1] = 0x0409;  // English
      desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | 4);
      return desc_str;
    case STRID_MANUFACTURER:
      str = device_str1();
      break;
    case STRID_PRODUCT:
      str = device_str2();
      break;
    case STRID_SERIAL: {
      // serialosc looks for monome-style serials: 'm' + 7 digits.
      pico_unique_board_id_t id;
      pico_get_unique_board_id(&id);
      const uint32_t n =
          ((uint32_t)id.id[7] << 24) | ((uint32_t)id.id[6] << 16) | ((uint32_t)id.id[5] << 8) | id.id[4];
      snprintf(serial, sizeof(serial), serial_mode() ? "m%07lu" : "%08lu",
               (unsigned long)(n % (serial_mode() ? 10000000u : 100000000u)));
      str = serial;
      break;
    }
    case STRID_CDC:
      str = "serial";
      break;
    case STRID_MIDI:
      str = "midi";
      break;
    default:
      return NULL;
  }

  size_t count = strlen(str);
  if (count > 32) count = 32;
  for (size_t i = 0; i < count; i++) desc_str[1 + i] = (uint16_t)str[i];
  desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * count + 2));
  return desc_str;
}
