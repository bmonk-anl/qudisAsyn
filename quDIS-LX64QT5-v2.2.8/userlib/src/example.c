/******************************************************************************
 *
 *  Project:        quDIS Control Library
 *
 *  Filename:       example.c
 *
 *  Purpose:        Simple example
 *
 *  Author:         N-Hands GmbH & Co KG
 */
/*****************************************************************************/
/* $Id: example.c,v 1.9 2021/12/16 09:32:35 cooper Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include "qudis.h"

#ifdef unix
#include <unistd.h>
#define SLEEP(x) usleep(x*1000)
#else
#include <windows.h>   /* for Sleep */
#define SLEEP(x) Sleep(x)
#endif

static double       _sampleTime    = 40e-6; /* Sample Time */

// Context for callback function
struct Context {
  const char * _tag;
  unsigned int _expectedIndex;
  double       _timestamp;
};

static struct Context contextRel = { "Rel", 0, -1 };
static struct Context contextAbs = { "Abs", 0, -1 };

static const char * getMessage( int code )
{
  switch( code ) {
  case QDS_Ok:           return "";
  case QDS_Error:        return "Unspecified error";
  case QDS_Timeout:      return "Communication timeout";
  case QDS_NotConnected: return "No active connection to device";
  case QDS_DriverError:  return "Error in comunication with driver";
  case QDS_DeviceLocked: return "Device is already in use by other";
  case QDS_Unknown:      return "Unknown error";
  case QDS_NoDevice:     return "Invalid device number in function call";
  case QDS_NoAxis:       return "Invalid axis number in function call";
  case QDS_ParamOutOfRg: return "A parameter exceeds the allowed range";
  case QDS_OldValue:     return "An old position value was returned.";
  default:               return "Unknown error code";
  }
}

static void checkError( const char * context, int code )
{
  if ( code == QDS_OldValue ) {
    printf( "%s: %s\n", context, getMessage( code ) );
    return;
  }
  if ( code != QDS_Ok ) {
    printf( "Error calling %s: %s\n", context, getMessage( code ) );
    /* It's not clean to exit here, should first close connections */
    exit( code );
  }
}

static int selectDevice()
{
  unsigned int devCount = 0, devNo = 0;
  int rc = QDS_discover( IfAll, &devCount );
  checkError( "QDS_discover", rc );
  if ( devCount <= 0 ) {
    printf( "No devices found\n" );
    exit( 0 );
  }

  for ( devNo = 0; devNo < devCount; devNo++ ) {
    int id = 0;
    char addr[20], serialNo[20];
    rc = QDS_getDeviceInfo( devNo, &id, serialNo, addr );
    checkError( "QDS_getDeviceInfo", rc );
    printf( "Device found: No=%d Id=%d SN=%s Addr=%s\n", devNo, id, serialNo, addr );   
  }

  devNo = 0;
  if ( devCount > 1 ) {
    printf( "Select device: " );
    devNo = getchar();
    devNo = devNo >= devCount ? 0 : devNo;
    printf( "\n" );
  }

  return devNo;
}

static void positionCallback( struct Context * c, unsigned int devNo, unsigned int count,
                              unsigned int index,  const double * const pos[QDS_AXES_CNT],
                              const Int32 * const mrk[QDS_AXES_CNT] )
{
  /* The callback function is somewhat complicated in order to detect missing packets
     and index overflow. Generally it would be sufficient to use
     _timestamp = (index + i) * _sampleTime;
  */
  unsigned int i;
  if ( c ->_timestamp < 0. ) {
    c ->_expectedIndex = index;
    c ->_timestamp     = 0;
  }

  if ( index != c ->_expectedIndex ) {
    unsigned int missing = index - c ->_expectedIndex;
    printf( "%s: Missing data: %d\n", c ->_tag, missing );
    if ( missing > 0 ) {
      c ->_timestamp += _sampleTime * missing;
    }
  }
  c ->_expectedIndex  = index + count;

  for ( i = 0; i < count; ++i ) {
    if ( i == 0 ) {  // don't print too much
      printf( "%s: %8.1f ms: %14.1f  %14.1f  %14.1f   %10d\n",
              c ->_tag, c ->_timestamp * 1000, pos[0][i], pos[1][i], pos[2][i], mrk[0][i] );
    }
    c ->_timestamp += _sampleTime;
  }
}

static void relCallback( unsigned int devNo, unsigned int count, unsigned int index,
                         const double * const pos[QDS_AXES_CNT], const Int32 * const mrk[QDS_AXES_CNT] )
{
  positionCallback( &contextRel, devNo, count, index, pos, mrk );
}

static void absCallback( unsigned int devNo, unsigned int count, unsigned int index,
                         const double * const pos[QDS_AXES_CNT], const Int32 * const mrk[QDS_AXES_CNT] )
{
  positionCallback( &contextAbs, devNo, count, index, pos, mrk );
}


static void pollPositions( int devNo )
{
  double relPos[QDS_AXES_CNT], absPos[QDS_AXES_CNT];
  double temp, press, relHum, refractiveIndex;
  unsigned int i;
  temp = press = relHum = refractiveIndex = .0;

  printf( "\nPolling positions [nm]\n" );
  for ( i = 0; i < 20; ++i ) {
    int rc = QDS_getPositions( devNo, relPos, absPos );
    checkError( "QDS_getPositions", rc );
    printf( "%5d:    %9.1f  %9.1f  %9.1f  -  %9.1f  %9.1f  %9.1f \n", i,
            relPos[0], relPos[1], relPos[2], absPos[0], absPos[1], absPos[2] );

    /* The AMU measurement frequency is 10 Hz. Only poll results in every
     * second pass of the loop.
     */
    if ( i % 2 )
    {
      rc = QDS_readAMU( devNo, &temp, &press, &relHum, &refractiveIndex );
      switch (rc)
      {
        case QDS_Ok:
          printf( "%5d:  T = %9.2f °C\n"
                  "        p = %9.2f hPa\n"
                  "       rH = %9.2f %%\n"
                  "      -------------------\n"
                  "        n = %12.9f\n", i,
                  temp, press, relHum, refractiveIndex);
          break;
        case QDS_NotConnected:
        case QDS_Capability:
          // The AMU is either not connected or the capability is not unlocked.
          break;
        default:
          checkError( "QDS_readAMU", rc );
      }
    }
    SLEEP( 50 );
  }
}


static void pollPositionBuffer( int devNo )
{
  double *relPos[QDS_AXES_CNT], *absPos[QDS_AXES_CNT];
  unsigned int lossR[QDS_AXES_CNT], lossA[QDS_AXES_CNT];
  unsigned int i, counter;
  static const unsigned int values = 10000;

  counter = 0;

  for ( i = 0; i < QDS_AXES_CNT; ++i ) {
    relPos[i] = (double*)malloc( values * sizeof(double) );
    absPos[i] = (double*)malloc( values * sizeof(double) );
  }

  QDS_configureBuffer( devNo, values );

  printf( "\nPolling positions [nm]\n" );
  for ( i = 0; i < 20; ++i ) {
    unsigned int read = values;
    int rc = QDS_readBuffer( devNo, relPos, absPos, &read );
    checkError( "QDS_readBuffer", rc );

    QDS_getLostData( devNo, lossR, lossA );
    for ( unsigned int i = 0; i < read; ++i )
    {
       printf( "%5d:    %9.1f  %9.1f  %9.1f  -  %9.1f  %9.1f  %9.1f", ++counter,
               relPos[0][i], relPos[1][i], relPos[2][i], 
               absPos[0][i], absPos[1][i], absPos[2][i] );
       printf(" [%d|%d|%d]\n", lossR[0], lossR[1], lossR[2]);
    }
    SLEEP( 50 );
  }

}


static void streamPositions( int devNo )
{
  int rc = QDS_Ok;
  printf( "\nStreaming Positions [nm]\n" );
  rc = QDS_setPositionCallback( devNo, relCallback, absCallback );
  checkError( "QDS_setPositionCallback", rc );
  SLEEP( 500 );
  rc = QDS_setPositionCallback( devNo, NULL, NULL );
  checkError( "QDS_setPositionCallback", rc );
}


static void testGetFunctions( int devNo )
{
  int rc = QDS_Ok, features = 0;
  unsigned int lbSt = 0, axes = 0;
  rc = QDS_getDeviceConfig( devNo, &axes, &features );
  checkError( "QDS_getDeviceConfig", rc );
  rc = QDS_getSampleTime( devNo, &lbSt );
  checkError( "QDS_getSampleTime", rc );
  printf( "Number of axes: %d - Feature flags: 0x%x - Sample Time = %f ms\n",
          axes, features, 40e-3 * (1<<lbSt) );
}


static int waitForDevReady( unsigned int devNo )
{
  int rc = QDS_Ok, ok = 0, finished = 0;
  QDS_LCtlState state;
  while ( !finished ) {
    rc = QDS_getLambdaControlState( devNo, &state );
    checkError( "QDS_getLambdaControlState", rc );
    switch ( state ) {
    case LcPrep:
      printf( "waiting for wavelength calibration...\n" );
      SLEEP( 1000 );
      break;
    case LcOk:
      ok = 1;
      finished = 1;
      break;
    case LcFail:
    default:
      printf( "Wavelength calibration failed!\n" );
      finished = 1;
      break;
    }
  }
  return ok;
}


int main( int argc, char ** argv )
{
  int devNo = 0, rc = QDS_Ok, lbSampleTime = 0;/* 40us * 2^0 = 40us */
  unsigned int axis = 0;
 
  printf( "quDIS example program\n" );
  printf( "Usage: %s <lbSampleTime>\n", argv[0] );
  if ( argc > 1 ) {
    lbSampleTime = atoi( argv[1] );
  }
  _sampleTime = 40.e-6 * (1<<lbSampleTime);
  
  devNo = selectDevice();
  rc = QDS_connect( devNo );
  checkError( "QDS_connect", rc );
  rc = QDS_enableMarkers( devNo,  Mrk1Cnt, 0 );
  checkError( "QDS_enableMarkers", rc );
  rc = QDS_setSampleTime( devNo, lbSampleTime );
  checkError( "QDS_setSampleTime", rc );

  if ( waitForDevReady( devNo ) ) {
    testGetFunctions( devNo );
    for ( axis = 0; axis < QDS_AXES_CNT; ++axis ) {
      QDS_resetRelPosition( devNo, axis );
    }
    pollPositions(    devNo );
    pollPositionBuffer( devNo );
    streamPositions(  devNo );
  }

  rc = QDS_disconnect( devNo );
  checkError( "QDS_disconnect", rc );
  return 0;
}
