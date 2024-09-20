// commands to use:
// connect/disconnect
// enableMarkers?
// setSampleTime
//

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

// return error type
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

// print available devices and get user input to choose which one
static int selectDevice()
{
  unsigned int devCount = 0, devNo = 0;

  // QDS_discover(QDS_InterfaceType ifaces, unsigned int* devCount)
  //
  // IfAll - device connected any type of way
  // puts # of devices found into devCount
  // returns error code
  int rc = QDS_discover( IfAll, &devCount );
  // print error if one
  checkError( "QDS_discover", rc );
  if ( devCount <= 0 ) {
    printf( "No devices found\n" );
    exit( 0 );
  }

  // loop thru devices and print info (get id, serialNo, and addr (IP or string "USB")
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
    devNo = getchar(); // get user input for number
    devNo = devNo >= devCount ? 0 : devNo; // default 0
    printf( "\n" );
  }

  return devNo;
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

int main( int argc, char ** argv )
{
	int devNo = 0, rc = QDS_Ok, lbSampleTime = 0;/* 40us * 2^0 = 40us */
	unsigned int axis = 0;
	
	devNo = selectDevice();
  	rc = QDS_disconnect( devNo );
  	rc = QDS_connect( devNo );
  	checkError( "QDS_connect", rc );

	pollPositions(devNo);

  	rc = QDS_disconnect( devNo );
  	checkError( "QDS_disconnect", rc );

	return 0;
}
