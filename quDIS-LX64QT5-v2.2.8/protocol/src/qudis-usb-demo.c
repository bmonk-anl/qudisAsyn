/*******************************************************************************/
/*
 *  Project:        quDIS
 *
 *  Filename:       qudis-usb-demo.c
 *
 *  Purpose:        simple demo of quDIS interface usage over usb - Linux only!
 *
 *  Author:         G. Franke,  g.franke@n-hands.de
 *
 *  Comment:        Compilation commands:
 *                    Linux with GCC:      "gcc qudis-usb-emo.c -lusb-1.0 -o qudis-usb-demo"
 */
/*******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <libusb-1.0/libusb.h>
#include "ucprotocol.h"
#include "qudis_protocol.h"
#include "fx3-img.h"                     // Firmware for the fx3 USB chip
#define Sleep(t) usleep((t)*1000)        // sleep in ms

#define USB_VID_CURRENT      0x04B4      // Vendor-ID  quDIS
#define USB_PID_LOADER       0x00F3      // Product-ID bootloader (for USB chip)
#define USB_PID_CURRENT      0x00F1      // Product-ID quDIS
#define USB_IFC_NUMBER       0           // Interface number
#define USB_EP_SEND          0x01        // Endpoint number sending
#define USB_EP_RECV          0x81        // Endpoint number receiving
#define RECV_BUFSIZE         (64 * 1024) // Size of receive buffer
#define RECV_BLOCKSIZE       1024        // Space reserved for a telegram

/* ---------------------------------------------------
** Usb helper functions
** ---------------------------------------------------
*/

// Upload firmware to the built-in USB chip
static int usbUploadFx3( libusb_device_handle * handle )
{
  int err              = LIBUSB_SUCCESS;
  unsigned char * data = (unsigned char *) (fx3img + 4);  // skip header checking; remove const :(
  unsigned int    rest = fx3imgSize - 4;

  while ( !err && rest > 8 ) {
    unsigned int addr = * (int *) (data + 4);  // read sector header in data
    unsigned int slen = (* (int *) data) * 4;
    data += 8;
    rest -= 8;
    if ( slen > 0 ) {
      while ( slen > 0 ) {
        int block = slen < 4096 ? slen : 4096;
        libusb_control_transfer( handle, 0x40, 0xA0, addr & 0xffff, addr >> 16, data, block, 1000 );
        addr += block;
        data += block;
        slen -= block;
        rest -= block;
      }
    }
    else {
      Sleep( 1 );
      err = libusb_control_transfer( handle, 0x40, 0xA0, addr & 0xffff, addr >> 16, NULL, 0, 1000 );
      break;
    }
  }
  return err;
}

// open the nth device that matches VID/PID; return handle and libusb error code
static int usbUploadFirmware( void )
{
  libusb_device ** devList = NULL;
  libusb_device_handle * handle = NULL;
  int uploadCount = 0;
  ssize_t i;
  int err = libusb_init( NULL /* context */ );
  
  ssize_t totalDevs = libusb_get_device_list( NULL, &devList );
  for ( i = 0; !err && devList && i < totalDevs; i++ ) {
    libusb_device * device = devList[i];
    struct libusb_device_descriptor dd;
    libusb_get_device_descriptor( device, &dd );
    if ( dd .idVendor == USB_VID_CURRENT && dd .idProduct == USB_PID_LOADER ) {
      err = libusb_open( device, &handle );
      if ( !err ) {
        err = usbUploadFx3( handle );
        libusb_close( handle );
        uploadCount++;
      }
    }
  }
  libusb_free_device_list( devList, 1 );

  if ( uploadCount > 0 && !err ) {
    Sleep( 1000 );
  }
  return err;
}


// open the nth device that matches VID/PID; return handle and libusb error code
static int usbOpenDevice( int devNo, libusb_device_handle ** handle )
{
  libusb_device ** devList = NULL;
  *handle = NULL;
  ssize_t i;
  int err = libusb_init( NULL /* context */ );
    
  if ( !err ) {
    //    libusb_set_debug( NULL, 1 ); // Param. 2: Debug output. 0=Noner, 3=Maximum
    ssize_t totalDevs = libusb_get_device_list( NULL, &devList );
    int devCounter = 0;
    for ( i = 0; devList && i < totalDevs; i++ ) {
      libusb_device * device = devList[i];
      struct libusb_device_descriptor dd;
      libusb_get_device_descriptor( device, &dd );
      if ( dd .idVendor == USB_VID_CURRENT && dd .idProduct == USB_PID_CURRENT ) {
        if ( devCounter == devNo ) {  // the device we are looking for
          err = libusb_open( device, handle );
          if ( !err ) {
            err = libusb_claim_interface( *handle, USB_IFC_NUMBER );
          }
          break;
        }
        devCounter++;
      }
    }
    libusb_free_device_list( devList, 1 );
  }

  if ( !err && !*handle ) {
    err = 1;
  }

  return err;
}


// close usb device; handle will be invalid afterwards
static void usbCloseDevice( libusb_device_handle * handle )
{
  libusb_release_interface( handle, USB_IFC_NUMBER );
  libusb_close( handle );
}


// Receive a telegram that begins with a 32 bit length field
// return libusb error code
static int usbReceive( libusb_device_handle * handle, int timeout, UcTelegram * buf )
{
  // USB receive buffer and its state
  static unsigned char _recvBuffer[RECV_BUFSIZE];
  static int  _received = 0, _fetched = 0;

  int err = 0;
  if ( _fetched >= _received ) {
    _fetched = 0;
    // Buffer is empty; receive a new one
    err = libusb_bulk_transfer( handle, USB_EP_RECV, _recvBuffer, RECV_BUFSIZE,
                                &_received, timeout );
  }

  if ( !err && _received > _fetched ) {
    UcTelegram * tel  = (UcTelegram *) (_recvBuffer + _fetched);
    memcpy( buf, tel, tel ->length + sizeof( Int32 ) );
    _fetched += RECV_BLOCKSIZE;
  }

  return err;
}


// send a telegram
// return libusb error code
static int usbSend( libusb_device_handle * handle, int timeout, UcTelegram * tel )
{
  int transferred = 0, length = tel ->length + sizeof( Int32 );
  int err = libusb_bulk_transfer( handle, USB_EP_SEND, (unsigned char *) tel, length,
                                  &transferred, timeout );
  // should check here if transferred == length 
  return err;
}


/* ---------------------------------------------------
** Protocol helpers
** ---------------------------------------------------
*/

#define SEND_TIMEOUT    1000
#define RECV_TIMEOUT    1000

// Send a SET telegram; return 0 if ok
// return libusb error code
static int sendSet( libusb_device_handle * handle, Int32 address, Int32 index, Int32 value )
{
  UcSetTelegram request = { {
      sizeof( UcSetTelegram ) - sizeof( Int32 ), /* length */
      UC_SET,                                    /* opcode */
      address,                                   /* address */
      index,                                     /* index */
      0                                          /* corr. # */
    },{
      value                                      /* data[0] */
    }
 };
  return usbSend( handle, SEND_TIMEOUT, (UcTelegram *) &request );
}


// Send a GET telegram; return 0 if ok
static int sendGet( libusb_device_handle * handle, Int32 address, Int32 index, Int32 ctag )
{
  UcGetTelegram request = { {
      sizeof( UcGetTelegram ) - sizeof( Int32 ), /* length */
      UC_GET,                                    /* opcode */
      address,                                   /* address */
      index,                                     /* index */
      ctag                                       /* corr. # */
    } };
  return usbSend( handle, SEND_TIMEOUT, (UcTelegram *) &request );
}


// Send a GET telegram and wait for answer; ignore everything else
// return libusb error code
static int getSync( libusb_device_handle * handle, Int32 address, Int32 index, Int32 * response )
{
  static Int32 ctag = 0x99;
  int answerReceived = 0, size = 0;
  char buffer[UC_MAXSIZE];
  UcAckTelegram * answer = (UcAckTelegram *) buffer;
  int err = sendGet( handle, address, index, ctag );

  while ( !err && !answerReceived ) {
    err = usbReceive( handle, RECV_TIMEOUT, (UcTelegram *) answer );
    size = answer ->hdr .length + sizeof( Int32 );

    if ( err ) {
      fprintf( stderr, "\nReceive error/timeout: rc=%d.\n", err );
    }
    else if ( size < sizeof( UcTelegram ) ) {
      fprintf( stderr, "\nReceived implausible length: %d.\n", size );
      err = -101;
    }
    answerReceived = !err && answer ->hdr .correlationNumber == ctag;
  }

  if ( answerReceived ) {
    /*
    printf( "\nReceived: opcode=%02x address=%04x index=%02x ctag=%04x reason=%02x value=%08x\n",
            answer ->hdr .opcode, answer ->hdr .address, answer ->hdr .index,
            answer ->hdr .correlationNumber, answer ->reason, answer ->data[0] );
    */
    *response = answer ->data[0];
  }

  ctag++;
  return err;
}


// Wait for streaming telegrams; ignore everything else
// return libusb error code
static int listenForData( libusb_device_handle * handle, Int32 * channel,
                          Int32 * count, Int32 * index,  Int32 ** data )
{
  int dataReceived = 0, chNo = 0, size = 0, err = 0;
  char buffer[UC_MAXSIZE];
  UcTellTelegram * dataTel = (UcTellTelegram *) buffer;

  while ( !err && !dataReceived ) {
    err = usbReceive( handle, RECV_TIMEOUT, (UcTelegram *) dataTel );
    size = dataTel ->hdr .length + sizeof( Int32 );
   
    if ( err ) {
      fprintf( stderr, "\nReceive error/timeout: rc=%d.\n", err );
    }
    else if ( size < sizeof( UcTelegram ) ) {
      fprintf( stderr, "\nReceived implausible length: %d.\n", size );
      err = -101;
    }
    chNo = dataTel ->hdr .address - ID_CHAN_BASE;
    dataReceived = !err && chNo >= 0 && chNo < QDS_NUM_CHANNELS;
  }

  if ( dataReceived ) {
    *channel = chNo;
    *count   = (size - sizeof( UcTellTelegram ) + sizeof( Int32 )) / sizeof( Int32 );
    *index   = dataTel ->hdr .index;
    *data    = dataTel ->data;
  }
  return err;
}


// Configure a data channel
static int configureChannel( libusb_device_handle * handle,
                             Int32 channel, Int32 trigger, Int32 source )
{
  int err1 = sendSet( handle, ID_CHAN_SOURCE,    channel, source  );
  int err2 = sendSet( handle, ID_QDS_DSIZE_EVEN, channel, 1       );
  int err3 = sendSet( handle, ID_CHAN_TRIGGER,   channel, trigger );
  return err1 ? err1 : (err2 ? err2 : err3);
}


/* ---------------------------------------------------
** quDIS Demo
** ---------------------------------------------------
*/

#define MY_CHANNEL 2    /* arbitrary channel number */

static void checkError( int err, const char * context )
{
  if ( err ) {
    printf( "Error in %s - rc=%d - Aborting!\n", context, err );
    exit( 1 );
  }
}


// combine two numbers with 24 valid Bits to 64 bit
static double decode64BitFormat( Int32 lsw, Int32 msw, Int32 * marker )
{
  char * sp = (char*) &msw;
  long long int v = sp[2] >= 0 ? 0 : -1;  // Sign expansion
  char * vp = (char *) &v;
  /* Bit31 marks MSW; should be at odd positions */
  if ( (lsw & (1<<31)) || !(msw & (1<<31)) ) {
    printf( "Data misaligned!\n" );
  }
  memcpy( vp,     &lsw, 3 );
  memcpy( vp + 3, &msw, 3 );
  *marker = ((lsw >> 24) & 0x7F) | ((msw >> 17) & 0x3F80); // 2x7Bit
  return (double) v;
}



int main( int argc, char ** argv )
{
  int err = 0, i = 0, samples = 0;
  libusb_device_handle * handle;
  Int32 hwType = 0, hwId = 0, measState = QDS_MEAS_PREP;

  if ( argc < 2 ) {
    fprintf( stderr, "\nusage: %s <device number>\n\n", argv[0] );
    return 0;
  }

  printf( "quDIS USB demo program\n" );
  err = usbUploadFirmware();  // initialize USB chip if necessary
  checkError( err, "UploadFirmware" );

  err = usbOpenDevice( atoi( argv[1] ), &handle );
  checkError( err, "OpenDevice" );

  err = getSync( handle, ID_GET_HW_TYPE, 0, &hwType );
  checkError( err, "GetHwType" );
  err = getSync( handle, ID_GET_HW_ID, 0, &hwId );
  checkError( err, "GetHwID" );
  printf( "Connected, hardware type is %x, ID is %d\n", hwType, hwId );
  if ( (hwType & 0xFF00) != QDS_HWTYPE_1_0 ) {
    printf( "Device is no quDIS. Aborting!\n" );
    return 1;
  }

  /* Wait for Wavelength control initialized */
  while ( measState == QDS_MEAS_PREP ) {
    err = getSync( handle, ID_QDS_MEAS_STATE, 0, &measState );
    checkError( err, "GetMeasState" );
    if ( measState == QDS_MEAS_PREP ) {
      printf( "Still waiting for device preparation.\n" );
      Sleep( 1000 );
    }
  }
  if ( measState != QDS_MEAS_OK ) {
    printf( "Device preparation failed -- Aborting!\n" );
    return 1;
  }

  /* Set Sample time to minimum (40us) */
  err = sendSet( handle, ID_QDS_POS_AVG, 0, 0 );
  checkError( err, "SetSampleTime" );

  /* Configure Data Channel: relative position axis 1 */
  for ( i = 0; i < QDS_NUM_CHANNELS; ++i ) {
    err = configureChannel( handle, i, QDS_TRG_DISABLED, QDS_SRC_REL_1 );
    checkError( err, "ConfigureChannel silent" );
  }
  err = sendSet( handle, ID_QDS_TRG_ENABLE, 0, 1 );
  checkError( err, "TriggerEnable" );
  err = sendSet( handle, ID_QDS_TRG_RESET, 0, 1 );
  checkError( err, "TriggerReset" );
  err = sendSet( handle, ID_QDS_TRG_AUTORESET, 0, 1000 );
  checkError( err, "TriggerAutoReset" );
  err = configureChannel( handle, MY_CHANNEL, QDS_TRG_POS, QDS_SRC_REL_1 );
  checkError( err, "ConfigureChannel" );
  err = sendSet( handle, ID_QDS_INDEX_RESET, 0, 1 );
  checkError( err, "IndexReset" );
  err = sendSet( handle, ID_ASYNC_EN, 0, 1 );
  checkError( err, "AsyncEnable" );
  err = sendSet( handle, ID_DATA_EN, 0, 1 );
  checkError( err, "DataEnable" );

  /* Receive Data (25kHz) for 5 s*/
  int lastMarker = -1, count = 0;
  double dist, average = 0.;
  while ( samples < 125000 ) {
    Int32  i, chan, packetSize, index, marker, *data;
    err = listenForData( handle, &chan, &packetSize, &index, &data );
    checkError( err, "listenForData" );
    if ( chan == MY_CHANNEL ) {
      for ( i = 0; i < packetSize; i += 2 ) {
        dist = decode64BitFormat( data[i], data[i+1], &marker );
        /* Calculate average of positions between rising and falling marker edges */
        if ( (marker & 1) && !(lastMarker & 1) && lastMarker >= 0 ) {
          /* rising marker edge */
          average = 0.;
          count   = 0;
        }
        else if ( !(marker & 1) && (lastMarker & 1) && lastMarker >= 0 ) {
          /* falling marker edge */
          printf( "Sample %d Count %d Marker %d Average %g \n",
                  samples, count, marker, average / count );
        }
        average += dist;
        samples ++;
        count   ++;
        lastMarker = marker;
      }
    }
  }
  
  /* Stop and exit */
  configureChannel( handle, MY_CHANNEL, QDS_TRG_DISABLED, QDS_SRC_REL_1 );
  usbCloseDevice( handle );

  return err;
}
