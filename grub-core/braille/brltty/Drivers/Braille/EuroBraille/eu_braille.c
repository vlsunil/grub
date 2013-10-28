/*
 * BRLTTY - A background process providing access to the console screen (when in
 *          text mode) for a blind person using a refreshable braille display.
 *
 * Copyright (C) 1995-2012 by The BRLTTY Developers.
 *
 * BRLTTY comes with ABSOLUTELY NO WARRANTY.
 *
 * This is free software, placed under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any
 * later version. Please see the file LICENSE-GPL for details.
 *
 * Web Page: http://mielke.cc/brltty/
 *
 * This software is maintained by Dave Mielke <dave@mielke.cc>.
 */

/**
 ** brl.c -- EuroBraille core driver file.
 ** Made by Yannick PLASSIARD and Olivier BERT
 */

#include "prologue.h"

typedef enum {
  PARM_PROTOCOL
}		DriverParameter;

#define BRLPARMS "protocol"


#include <stdio.h>
#include <string.h>

#include "message.h"
#include "log.h"

#define BRL_HAVE_PACKET_IO
#include "brl_driver.h"
#include "parse.h"
#include "timing.h"
#include "io_generic.h"

#include	"eu_protocol.h"


BEGIN_KEY_TABLE_LIST
  &KEY_TABLE_DEFINITION(clio),
  &KEY_TABLE_DEFINITION(iris),
  &KEY_TABLE_DEFINITION(esys_small),
  &KEY_TABLE_DEFINITION(esys_medium),
  &KEY_TABLE_DEFINITION(esys_large),
  &KEY_TABLE_DEFINITION(esytime),
END_KEY_TABLE_LIST

static GioEndpoint *gioEndpoint = NULL;
const InputOutputOperations *io = NULL;
static const ProtocolOperations *protocol = NULL;

static inline void
updateWriteDelay (BrailleDisplay *brl, size_t count) {
  brl->writeDelay += gioGetMillisecondsToTransfer(gioEndpoint, count);
}

static int
awaitInput_generic (int timeout) {
  return gioAwaitInput(gioEndpoint, timeout);
}

static int
readByte_generic (BrailleDisplay *brl, unsigned char *byte, int wait) {
  return gioReadByte(gioEndpoint, byte, wait);
}

static ssize_t
writeData_generic (BrailleDisplay *brl, const void *data, size_t length) {
  updateWriteDelay(brl, length);
  return gioWriteData(gioEndpoint, data, length);
}

static ssize_t
writeData_USB (BrailleDisplay *brl, const void *data, size_t length) {
  size_t offset = 0;

  while (offset < length) {
    unsigned char report[64];
    size_t count = length - offset;

    if (count > sizeof(report)) {
      count = sizeof(report);
    } else {
      memset(&report[count], 0X55, (sizeof(report) - count));
    }
    memcpy(report, data+offset, count);

    updateWriteDelay(brl, sizeof(report));
    if (gioSetHidReport(gioEndpoint, 0, report, sizeof(report)) < 0) return -1;

    offset += count;
  }

  return length;
}

static const InputOutputOperations serialOperations = {
  .awaitInput = awaitInput_generic,
  .readByte = readByte_generic,
  .writeData = writeData_generic
};

static const InputOutputOperations usbOperations = {
  .protocol = &esysirisProtocolOperations,
  .awaitInput = awaitInput_generic,
  .readByte = readByte_generic,
  .writeData = writeData_USB
};

static const InputOutputOperations bluetoothOperations = {
  .protocol = &esysirisProtocolOperations,
  .awaitInput = awaitInput_generic,
  .readByte = readByte_generic,
  .writeData = writeData_generic
};

static int
connectResource (const char *identifier) {
  static const SerialParameters serialParameters = {
    SERIAL_DEFAULT_PARAMETERS,
    .baud = 9600,
    .parity = SERIAL_PARITY_EVEN
  };

  static const UsbChannelDefinition usbChannelDefinitions[] = {
    { /* Esys, version < 3.0, without SD card */
      .vendor=0XC251, .product=0X1122,
      .configuration=1, .interface=0, .alternative=0,
      .inputEndpoint=1, .outputEndpoint=0
    }
    ,
    { /* reserved */
      .vendor=0XC251, .product=0X1123,
      .configuration=1, .interface=0, .alternative=0,
      .inputEndpoint=1, .outputEndpoint=0
    }
    ,
    { /* Esys, version < 3.0, with SD card */
      .vendor=0XC251, .product=0X1124,
      .configuration=1, .interface=0, .alternative=0,
      .inputEndpoint=1, .outputEndpoint=0
    }
    ,
    { /* reserved */
      .vendor=0XC251, .product=0X1125,
      .configuration=1, .interface=0, .alternative=0,
      .inputEndpoint=1, .outputEndpoint=0
    }
    ,
    { /* Esys, version >= 3.0, without SD card */
      .vendor=0XC251, .product=0X1126,
      .configuration=1, .interface=0, .alternative=0,
      .inputEndpoint=1, .outputEndpoint=0
    }
    ,
    { /* reserved */
      .vendor=0XC251, .product=0X1127,
      .configuration=1, .interface=0, .alternative=0,
      .inputEndpoint=1, .outputEndpoint=0
    }
    ,
    { /* Esys, version >= 3.0, with SD card */
      .vendor=0XC251, .product=0X1128,
      .configuration=1, .interface=0, .alternative=0,
      .inputEndpoint=1, .outputEndpoint=0
    }
    ,
    { /* reserved */
      .vendor=0XC251, .product=0X1129,
      .configuration=1, .interface=0, .alternative=0,
      .inputEndpoint=1, .outputEndpoint=0
    }
    ,
    { /* reserved */
      .vendor=0XC251, .product=0X112A,
      .configuration=1, .interface=0, .alternative=0,
      .inputEndpoint=1, .outputEndpoint=0
    }
    ,
    { /* reserved */
      .vendor=0XC251, .product=0X112B,
      .configuration=1, .interface=0, .alternative=0,
      .inputEndpoint=1, .outputEndpoint=0
    }
    ,
    { /* reserved */
      .vendor=0XC251, .product=0X112C,
      .configuration=1, .interface=0, .alternative=0,
      .inputEndpoint=1, .outputEndpoint=0
    }
    ,
    { /* reserved */
      .vendor=0XC251, .product=0X112D,
      .configuration=1, .interface=0, .alternative=0,
      .inputEndpoint=1, .outputEndpoint=0
    }
    ,
    { /* reserved */
      .vendor=0XC251, .product=0X112E,
      .configuration=1, .interface=0, .alternative=0,
      .inputEndpoint=1, .outputEndpoint=0
    }
    ,
    { /* reserved */
      .vendor=0XC251, .product=0X112F,
      .configuration=1, .interface=0, .alternative=0,
      .inputEndpoint=1, .outputEndpoint=0
    }
    ,
    { /* Esytime */
      .vendor=0XC251, .product=0X1130,
      .configuration=1, .interface=0, .alternative=0,
      .inputEndpoint=1, .outputEndpoint=0
    }
    ,
    { /* reserved */
      .vendor=0XC251, .product=0X1131,
      .configuration=1, .interface=0, .alternative=0,
      .inputEndpoint=1, .outputEndpoint=0
    }
    ,
    { /* reserved */
      .vendor=0XC251, .product=0X1132,
      .configuration=1, .interface=0, .alternative=0,
      .inputEndpoint=1, .outputEndpoint=0
    }
    ,
    { .vendor=0 }
  };

  GioDescriptor descriptor;
  gioInitializeDescriptor(&descriptor);

  descriptor.serial.parameters = &serialParameters;
  descriptor.serial.options.applicationData = &serialOperations;

  descriptor.usb.channelDefinitions = usbChannelDefinitions;
  descriptor.usb.options.applicationData = &usbOperations;

  descriptor.bluetooth.channelNumber = 1;
  descriptor.bluetooth.options.applicationData = &bluetoothOperations;

  if ((gioEndpoint = gioConnectResource(identifier, &descriptor))) {
    io = gioGetApplicationData(gioEndpoint);
    return 1;
  }

  return 0;
}

static void
disconnectResource (void) {
  if (gioEndpoint) {
    gioDisconnectResource(gioEndpoint);
    gioEndpoint = NULL;
  }
}

static int
brl_construct (BrailleDisplay *brl, char **parameters, const char *device) {
  io = NULL;
  protocol = NULL;
  makeOutputTable(dotsTable_ISO11548_1);

  if (parameters[PARM_PROTOCOL]) {
    static const char *const choices[] = {
      "auto",
      "azerbraille", "clio", "eurobraille",
      "notebraille", "pupibraille", "scriba",
      "esys", "esytime", "iris",
      NULL
    };

    static const ProtocolOperations *const protocols[] = {
      NULL, // auto

      &clioProtocolOperations, // azerbraille
      &clioProtocolOperations, // clio
      &clioProtocolOperations, // eurobraille
      &clioProtocolOperations, // notebraille
      &clioProtocolOperations, // pupibraille
      &clioProtocolOperations, // scriba

      &esysirisProtocolOperations, // esys
      &esysirisProtocolOperations, // esytime
      &esysirisProtocolOperations  // iris
    };

    unsigned int choice;

    if (!validateChoice(&choice, parameters[PARM_PROTOCOL], choices)) {
      logMessage(LOG_ERR, "unknown EuroBraille protocol: %s", 
                 parameters[PARM_PROTOCOL]);
      choice = 0;
    }

    protocol = protocols[choice];
  }

  if (connectResource(device)) {
    if (protocol) {
      if (!io->protocol || (io->protocol == protocol)) {
        if (protocol->initializeDevice(brl)) return 1;
      } else {
        logMessage(LOG_ERR, "protocol not supported by device: %s", protocol->protocolName);
      }
    } else if (io->protocol) {
      protocol = io->protocol;
      if (protocol->initializeDevice(brl)) return 1;
    } else {
      static const ProtocolOperations *const protocols[] = {
        &esysirisProtocolOperations, &clioProtocolOperations,
        NULL
      };
      const ProtocolOperations *const *p = protocols;

      while (*p) {
        const ProtocolOperations *protocol = *p++;

        logMessage(LOG_NOTICE, "trying protocol: %s", protocol->protocolName);
        if (protocol->initializeDevice(brl)) return 1;
	approximateDelay(700);
      }
    }

    disconnectResource();
  }

  return 0;
}

static void
brl_destruct (BrailleDisplay *brl) {
  if (protocol)
    {
      protocol = NULL;
    }
  disconnectResource();
}

#ifdef BRL_HAVE_PACKET_IO
static ssize_t
brl_readPacket (BrailleDisplay *brl, void *buffer, size_t size) {
  if (!protocol || !io)
    return (-1);
  return protocol->readPacket(brl, buffer, size);
}

static ssize_t
brl_writePacket (BrailleDisplay *brl, const void *packet, size_t length) {
  if (!protocol || !io)
    return (-1);
  return protocol->writePacket(brl, packet, length);
}

static int
brl_reset (BrailleDisplay *brl) {
  if (!protocol || !io)
    return (-1);
  return protocol->resetDevice(brl);
}
#endif /* BRL_HAVE_PACKET_IO */

static int
brl_writeWindow (BrailleDisplay *brl, const wchar_t *text) {
  if (!protocol) return 1;

  if (text)
    if (!protocol->writeVisual(brl, text))
      return 0;

  return protocol->writeWindow(brl);
}

static int
brl_readCommand (BrailleDisplay *brl, KeyTableCommandContext context) {
  if (protocol)
    return protocol->readCommand(brl, context);
  return EOF;
}