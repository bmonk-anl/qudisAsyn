/*
 * 
 * 
 * 
 */

#include <asynPortDriver.h>

static const char *driverName = "qudisAsyn";

#define MAX_CONTROLLERS	1
#define DEFAULT_POLL_TIME 0.1

#define DEFAULT_CONTROLLER_TIMEOUT 2.0

/* These are the drvInfo strings that are used to identify the parameters.
 * They are used by asyn clients, including standard asyn device support */

#define absPos1ValueString	"ABS_POS1_VALUE"	    /* asynFloat64 r/o */
#define absPos2ValueString	"ABS_POS2_VALUE"	    /* asynFloat64 r/o */
#define absPos3ValueString	"ABS_POS3_VALUE"	    /* asynFloat64 r/o */

#define relPos1ValueString	"REL_POS1_VALUE"	    /* asynFloat64 r/o */
#define relPos2ValueString	"REL_POS2_VALUE"	    /* asynFloat64 r/o */
#define relPos3ValueString	"REL_POS3_VALUE"	    /* asynFloat64 r/o */

/*
 * Class definition for the qudisAsyn class
 */
class qudisAsyn: public asynPortDriver {
public:
    qudisAsyn(const char *portName, const char *qudisAsynPortName);
    virtual ~qudisAsyn();
    
    /* These are the methods that we override from asynPortDriver */
    // virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    // virtual asynStatus readInt32(asynUser *pasynUser, epicsInt32 *value);
    // virtual asynStatus readFloat64(asynUser *pasynUser, epicsFloat64 *value);
    // virtual asynStatus writeFloat64(asynUser *pasynUser, epicsFloat64 value);
    // value - pointer to string
    // maxChars - max num of characters to read
    //
    // virtual asynStatus readOctet(asynUser *pasynUser, char* value, size_t maxChars, 
    //    size_t* nActual, int* eomReason);
    // virtual asynStatus disconnect(asynUser *pasynUser);
    // virtual asynStatus connect(asynUser *pasynUser);
    // These should be private but are called from C

    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);

    virtual asynStatus connect(asynUser *pasynUser);
    virtual asynStatus disconnect(asynUser *pasynUser);
    virtual void pollerThread(void);

	void pollPositions();

protected:
    // "float" index values
    int absPos1Val_;
    int absPos2Val_;
    int absPos3Val_;

    int relPos1Val_;
    int relPos2Val_;
    int relPos3Val_;

	#define FIRST_QUDIS_PARAM pos1Val_;
	#define LAST_QUDIS_PARAM pos1Val_;

    asynUser* pasynUserqudisAsyn_;

private:

	void report(FILE *fp, int details);

	double pollTime_;

	unsigned int deviceId = 101;
	unsigned int deviceNo = 0;

	void checkError(const char * context, int code);

  
};

// #define NUM_PARAMS ((int)(&LAST_QUDIS_PARAM - &FIRST_QUDIS_PARAM + 1))
#define NUM_PARAMS 6


