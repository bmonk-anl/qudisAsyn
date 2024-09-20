#include <stdio.h>
#include <iocsh.h>
#include <epicsExport.h>
#include <epicsString.h>
#include <epicsThread.h>
#include <asynOctetSyncIO.h>
#include <string.h>

#include <stdlib.h>
#include <unistd.h>
#include "qudis.h" // vendor supplied library

#include "qudisAsyn.h"

using namespace std;

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

void qudisAsyn::checkError(const char * context, int code)
{
  if ( code == QDS_OldValue ) {
    printf( "%s: %s\n", context, getMessage( code ) );
    return;
  }
  if ( code != QDS_Ok ) {
    printf( "Error calling %s: %s\n", context, getMessage( code ) );
  }
}

void qudisAsyn::pollPositions()
{
  double relPos[QDS_AXES_CNT], absPos[QDS_AXES_CNT];
  double temp, press, relHum, refractiveIndex;
  unsigned int i;
  temp = press = relHum = refractiveIndex = .0;

    int errorCode = QDS_getPositions(this->deviceNo, relPos, absPos);
    checkError( "QDS_getPositions", errorCode);
    printf( "%5d:    %9.1f  %9.1f  %9.1f  -  %9.1f  %9.1f  %9.1f \n", i,
             relPos[0], relPos[1], relPos[2], absPos[0], absPos[1], absPos[2] );

    setDoubleParam(absPos1Val_, absPos[0]);
    setDoubleParam(absPos2Val_, absPos[1]);
    setDoubleParam(absPos3Val_, absPos[2]);
    setDoubleParam(relPos1Val_, relPos[0]);
    setDoubleParam(relPos2Val_, relPos[1]);
    setDoubleParam(relPos3Val_, relPos[2]);


    getDoubleParam(absPos1Val_, &absPos[0]);
    getDoubleParam(absPos2Val_, &absPos[1]);
    getDoubleParam(absPos3Val_, &absPos[2]);
    getDoubleParam(relPos1Val_, &relPos[0]);
    getDoubleParam(relPos2Val_, &relPos[1]);
    getDoubleParam(relPos3Val_, &relPos[2]);

    printf( "RBV from asyn: %5d:    %9.1f  %9.1f  %9.1f  -  %9.1f  %9.1f  %9.1f \n", i,
             relPos[0], relPos[1], relPos[2], absPos[0], absPos[1], absPos[2] );

    // /* The AMU measurement frequency is 10 Hz. Only poll results in every
    //  * second pass of the loop.
    //  */
    //   errorCode = QDS_readAMU(deviceNo, &temp, &press, &relHum, &refractiveIndex);
    //   switch (errorCode)
    //   {
    //     case QDS_Ok:
    //       printf( "%5d:  T = %9.2f °C\n"
    //               "        p = %9.2f hPa\n"
    //               "       rH = %9.2f %%\n"
    //               "      -------------------\n"
    //               "        n = %12.9f\n", i,
    //               temp, press, relHum, refractiveIndex);
    //       break;
    //     case QDS_NotConnected:
    //     case QDS_Capability:
    //       // The AMU is either not connected or the capability is not unlocked.
    //       // printf("no AMU capability\n");
    //       break;
    //     default:
    //       checkError( "QDS_readAMU", errorCode);
    //   }
}

// This needs to come before the qudis constructor to avoid compiler errors
static void pollerThreadC(void * pPvt)
{
  qudisAsyn *pqudisAsyn = (qudisAsyn*)pPvt;
  pqudisAsyn->pollerThread();
}

qudisAsyn::qudisAsyn(const char *portName, const char *qudisAsynPortName) : asynPortDriver(portName, MAX_CONTROLLERS,
		asynInt32Mask | asynFloat64Mask | asynDrvUserMask | asynOctetMask,
		asynInt32Mask | asynFloat64Mask | asynOctetMask,
		ASYN_MULTIDEVICE | ASYN_CANBLOCK, 1, /* ASYN_CANBLOCK=0, ASYN_MULTIDEVICE=1, autoConnect=1 */
		0, 0), /* Default priority and stack size */
    pollTime_(DEFAULT_POLL_TIME)
{
	static const char *functionName = "qudisAsyn";
    asynStatus status;
	
	createParam(absPos1ValueString,   	asynParamFloat64, &absPos1Val_);
	createParam(absPos2ValueString,   	asynParamFloat64, &absPos2Val_);
	createParam(absPos3ValueString,   	asynParamFloat64, &absPos3Val_);

	createParam(relPos1ValueString,   	asynParamFloat64, &relPos1Val_);
	createParam(relPos2ValueString,   	asynParamFloat64, &relPos2Val_);
	createParam(relPos3ValueString,   	asynParamFloat64, &relPos3Val_);

	// Force the device to connect now
	connect(this->pasynUserSelf);

	// status = pasynOctetSyncIO->connect(qudisAsynPortName, 0, &pasynUserqudisAsyn_, NULL);
	
  //setIntegerParam(variable_, 1);
  
	// Start the poller
  epicsThreadCreate("qudisAsynPoller", 
      epicsThreadPriorityLow,
      epicsThreadGetStackSize(epicsThreadStackMedium),
      (EPICSTHREADFUNC)pollerThreadC,
      this);
	
  //epicsThreadSleep(5.0);
}

asynStatus qudisAsyn::connect(asynUser *pasynUser)
{
	asynStatus status;
	static const char *functionName = "connect";
	int errorCode;
	unsigned int devCount; // number of qudis devices available
	unsigned int devNo; 


    	asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
        	"%s:%s: Connecting...\n", driverName, functionName);

	// discover available devices. IfAll - both usb and ethernet
  	errorCode = QDS_discover(IfAll, &devCount);

  	if (devCount <= 0) {
		printf( "No devices found\n" );
		return asynError;
	}

	// search through available devices for desired ID
  	for (devNo = 0; devNo < devCount; devNo++) {
		int id = 0;
		char addr[20], serialNo[20];
    		errorCode = QDS_getDeviceInfo(devNo, &id, serialNo, addr);
    		checkError("QDS_getDeviceInfo", errorCode);
    		printf( "Device found: No=%d Id=%d SN=%s Addr=%s\n", devNo, id, serialNo, addr );   
		if (id == this->deviceId) {
			printf("connecting to device with ID %d\n", id);
			this->deviceNo = devNo;
			break;
		}
		// desired ID not found:
		if (devNo == devCount-1) {
			printf("ID not found\n");
			return asynError;
		}
	}
	

	this->lock();
	errorCode = QDS_disconnect(this->deviceNo); // disconnect first
	errorCode = QDS_connect(this->deviceNo);
	this->unlock();

  	checkError("QDS_connect", errorCode);

    /* We found the controller and everything is OK.  Signal to asynManager that we are connected. */
    status = pasynManager->exceptionConnect(this->pasynUserSelf);
    if (status) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
            "%s:%s: error calling pasynManager->exceptionConnect, error=%s\n",
            driverName, functionName, pasynUserSelf->errorMessage);
        return asynError;
    }

 	return asynSuccess;
}

asynStatus qudisAsyn::disconnect(asynUser *pasynUser)
{
	asynStatus status;
	static const char *functionName = "disconnect";
	int errorCode;

    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
        "%s:%s: Disconnecting...\n", driverName, functionName);

	this->lock();
  	errorCode = QDS_disconnect(this->deviceNo);
	this->unlock();

  	checkError("QDS_disconnect", errorCode);

    /* We found the controller and everything is OK.  Signal to asynManager that we are connected. */
    status = pasynManager->exceptionDisconnect(this->pasynUserSelf);
    if (status) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
            "%s:%s: error calling pasynManager->exceptionDisonnect, error=%s\n",
            driverName, functionName, pasynUserSelf->errorMessage);
        return asynError;
    }

 	return asynSuccess;
}



qudisAsyn::~qudisAsyn()
{
	// Force the controller to disconnect
	disconnect(this->pasynUserSelf);
}

/*
 * 
 * poller
 * 
 */
void qudisAsyn::pollerThread()
{
  /* This function runs in a separate thread.  It waits for the poll time. */
  static const char *functionName = "pollerThread";

  // Other variable declarations
    asynStatus comStatus;
  
  while (1)
  {
    
    lock();

    pollPositions();

    unlock();
    callParamCallbacks();
    epicsThreadSleep(pollTime_);

  }
}

/*
 *
 * writeInt32
 *
 */
// this function not needed currently
asynStatus qudisAsyn::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
	int function = pasynUser->reason;
	asynStatus status = asynSuccess;
	static const char *functionName = "writeInt32";

    asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
			"%s:%s, port %s, function = %d\n",
			driverName, functionName, this->portName, function);

	setIntegerParam(function, value);

	callParamCallbacks();

	if (status == 0) {
		asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
             "%s:%s, port %s, wrote %d\n",
             driverName, functionName, this->portName, value);
	} else {
		asynPrint(pasynUser, ASYN_TRACE_ERROR, 
             "%s:%s, port %s, ERROR writing %d, status=%d\n",
             driverName, functionName, this->portName, value, status);
	}
	
	return (status==0) ? asynSuccess : asynError;
}

void qudisAsyn::report(FILE *fp, int details)
{
    asynPortDriver::report(fp, details);
    fprintf(fp, "* Port: %s\n", 
        this->portName);
    fprintf(fp, "\n");
}

extern "C" int qudisAsynConfig(const char *portName, const char *qudisAsynPortName)
{
    qudisAsyn *pqudisAsyn = new qudisAsyn(portName, qudisAsynPortName);
    pqudisAsyn = NULL; /* This is just to avoid compiler warnings */
    return(asynSuccess);
}

static const iocshArg qudisAsynArg0 = { "Port name", iocshArgString};
static const iocshArg qudisAsynArg1 = { "qudisAsyn port name", iocshArgString};
static const iocshArg * const qudisAsynArgs[2] = {&qudisAsynArg0, &qudisAsynArg1};
static const iocshFuncDef qudisAsynFuncDef = {"qudisAsynConfig", 2, qudisAsynArgs};
static void qudisAsynCallFunc(const iocshArgBuf *args)
{
    qudisAsynConfig(args[0].sval, args[1].sval);
}

void drvqudisAsynRegister(void)
{
    iocshRegister(&qudisAsynFuncDef, qudisAsynCallFunc);
}

extern "C" {
    epicsExportRegistrar(drvqudisAsynRegister);
}
