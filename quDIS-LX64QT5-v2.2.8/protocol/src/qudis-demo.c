/*******************************************************************************/
/*
 *  Project:        quDIS
 *
 *  Filename:       qudis-demo.c
 *
 *  Purpose:        simple demo of quDIS interface usage
 *
 *  Author:         G. Franke,  g.franke@n-hands.de
 *
 *  Comment:        Compilation commands:
 *                    Windows with Visual Studio: "cl  qudis-demo.c ws2_32.lib"
 *                    Windows with minGW32:       "gcc qudis-demo.c -l ws2_32 -o qudis-demo.exe"
 *                    Linux with GCC:             "gcc qudis-demo.c -o qudis-demo"
 */
/*******************************************************************************/

#ifdef unix
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#define closesocket close
#define SET_INADDR(in_var,adr_val)  (in_var) .s_addr = (adr_val)
#define GET_INADDR(in_var)          (in_var) .s_addr
#define Sleep(t)                    usleep((t)*1000)

#else

#include <windows.h>
#define SET_INADDR(in_var,adr_val)  (in_var) .S_un .S_addr = (adr_val)
#define GET_INADDR(in_var)          (in_var) .S_un .S_addr
typedef struct fd_set fd_set;
#endif

#include <stdio.h>
#include "ucprotocol.h"
#include "qudis_protocol.h"


/* ---------------------------------------------------
** Networking helper functions
** ---------------------------------------------------
*/

// Initialisation (Windows specific)
static void tcpInit()
{
#ifndef unix
  static int winsockInit = 0;
  if ( !winsockInit ) {
    WORD wVersionRequested = MAKEWORD( 2, 2 );
    WSADATA wsaData;
    WSAStartup( wVersionRequested, &wsaData );
    winsockInit = 1;
  }
#endif
}


// Connect to host:port; return socket or -1 on error
static int tcpConnect( unsigned long host, unsigned short port )
{
  int sk;
  struct sockaddr_in adr;
  adr .sin_family = AF_INET;
  adr .sin_port   = htons( port );
  SET_INADDR( adr .sin_addr, host );
  sk = (int) socket( PF_INET, SOCK_STREAM, 0 );
  if ( sk <= 0 ) {
    fprintf( stderr, "Connect to controller: socket() failed\n" );
    return -1;
  }
  if ( connect( sk, (struct sockaddr *) &adr, sizeof( struct sockaddr_in ) ) < 0 ) {
    fprintf( stderr, "Connect to controller: connect() failed\n" );
    closesocket( sk );
    return -1;
  }
  return sk;
}


// Connect to host:port; return socket or -1 on error
static int tcpConnectByName( const char * host, unsigned short port )
{
  struct hostent * hp = gethostbyname( host );
  if ( !hp ) {
    fprintf( stderr, "Connect to controller: gethostbyname() failed\n" );
    return -1;
  }

  return tcpConnect( * (unsigned long *) (hp ->h_addr), port );
}


// Hard disconnect
static void tcpDisconnect( int sock )
{
  closesocket( sock );
}


// Receive a data packet that begins with a 32 bit length field;
// return 0 or negative on error
static int tcpReceive( int sock, int timeout, UcTelegram * buf )
{
  int bytes = 1, received = 0, selrc = 0, err = 0, expected = sizeof( Int32 );
  fd_set  fds;
  struct timeval to;
  FD_ZERO( &fds );
  FD_SET( sock, &fds );
  to .tv_sec  = timeout / 1000;
  to .tv_usec = (timeout % 1000) * 1000;

  selrc = select( sock + 1, &fds, 0, 0, &to );

  if ( selrc > 0 ) {
    while ( expected > received && bytes > 0 && !err ) {
      bytes     = recv( sock, (char*) buf + received, expected - received, 0 );
      received += bytes;
      if ( received == sizeof( Int32 ) ){
        expected = sizeof( Int32 ) + * (Int32*) buf;  /* Length has been received */
        if (expected > UC_MAXSIZE){
          fprintf( stderr, "Receive from controller: Telegram too long\n" );
          err = -101;
        }
      }
    }

    if ( bytes <= 0 ) {
      fprintf( stderr, "Receive from controller: recv() failed\n" );
      err = -1;
    }
  }
  else if ( selrc < 0 ) {
    fprintf( stderr, "Receive from controller: select() failed\n" );
    err = -1;
  }

  return err;
}


// Send a data packet
// return 0 if ok, -1 otherwise
static int tcpSend( int sock, Int32 length, void * data )
{
  int rc = send( sock, (char*) data, length, 0 );
  if ( rc != length ) {
    fprintf( stderr, "Send to controller: send() failed\n" );
    return -1;
  }
  return 0;
}


/* ---------------------------------------------------
** Protocol helpers
** ---------------------------------------------------
*/

#define RECV_TIMEOUT    1000

// Send a SET telegram; return 0 if ok
static int sendSet( int sock, Int32 address, Int32 index, Int32 value )
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
  return tcpSend( sock, sizeof( request ), &request );
}


// Send a GET telegram; return 0 if ok
static int sendGet( int sock, Int32 address, Int32 index, Int32 ctag )
{
  UcGetTelegram request = { {
      sizeof( UcGetTelegram ) - sizeof( Int32 ), /* length */
      UC_GET,                                    /* opcode */
      address,                                   /* address */
      index,                                     /* index */
      ctag                                       /* corr. # */
    } };
  return tcpSend( sock, sizeof( request ), &request );
}


// Send a GET telegram and wait for answer; ignore everything else
static int getSync( int sock, Int32 address, Int32 index, Int32 * response )
{
  static Int32 ctag = 0x99;
  int answerReceived = 0, size = 0;
  char buffer[UC_MAXSIZE];
  UcAckTelegram * answer = (UcAckTelegram *) buffer;
  int err = sendGet( sock, address, index, ctag );

  while ( !err && !answerReceived ) {
    err = tcpReceive( sock, RECV_TIMEOUT, (UcTelegram *) answer );
    size = answer ->hdr .length + sizeof( Int32 );

    if ( err ) {
      fprintf( stderr, "\nReceive timeout.\n" );
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
static int listenForData( int sock, Int32 * channel, Int32 * count, Int32 * index, Int32 ** data )
{
  int dataReceived = 0, chNo = 0, size = 0, err = 0;
  char buffer[UC_MAXSIZE];
  UcTellTelegram * dataTel = (UcTellTelegram *) buffer;

  while ( !err && !dataReceived ) {
    err = tcpReceive( sock, RECV_TIMEOUT, (UcTelegram *) dataTel );
    size = dataTel ->hdr .length + sizeof( Int32 );
   
    if ( err ) {
      fprintf( stderr, "\nReceive timeout.\n" );
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
static int configureChannel( int handle, Int32 channel, Int32 trigger, Int32 source )
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
  int handle = 0;   // is in fact a socket
  Int32 hwType = 0, hwId = 0, measState = QDS_MEAS_PREP;

  if ( argc < 2 ) {
    fprintf( stderr, "\nusage: %s <Ip-Address>\n\n", argv[0] );
    return 0;
  }

  printf( "quDIS TCP/IP demo program\n" );
  tcpInit();
  handle = tcpConnectByName( argv[1], QDS_TCP_PORT );
  checkError( handle <= 0, "ConnectByName" );

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
  tcpDisconnect( handle );
  
  return err;
}
