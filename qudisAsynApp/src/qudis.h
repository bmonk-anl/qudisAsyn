/*****************************************************************************
 *
 *  Project:        quDIS Control Library
 *
 *  Filename:       qudis.h
 *
 *  Author:         NHands GmbH & Co KG
 */
/*****************************************************************************/
/** @mainpage Custom Programming Library for quDIS
 *
 *  @ref qudis.h "quDIS Control Library" is a library that allows
 *  custom programming for the quDIS interferometer.  
 *  It can manage multiple devices that are connected to the PC via USB
 *  or ethernet.
 *
 *  Use \ref QDS_discover to discover devices on USB or ethernet.
 *  Inspect them with \ref QDS_getDeviceInfo and connect to selected devices
 *  using \ref QDS_connect. After connecting, all the other functions for
 *  configuration and position can be called.
 *
 *  Positions can be read out with \ref QDS_getPositions. When using C(++),
 *  \ref QDS_setPositionCallback can be used for a more efficient position
 *  streaming. When finished, close the connection with \ref QDS_disconnect.
 *
 *  Documentation for the functions can be found \ref qudis.h "here".
 */
/*****************************************************************************/
/** @file qudis.h
 *
 *  @brief Control and acquisition functions for quDIS
 *  
 *  Defines functions for connecting and controlling the quDIS device.
 *  The functions are not thread safe!
 */
/******************************************************************/
/* $Id: qudis.h,v 1.15 2021/12/16 09:32:35 cooper Exp $ */

#ifndef __QUDIS_H__
#define __QUDIS_H__

/** @name Function declarations
 *
 *  Technical definitions, mainly for the Windows DLL interface.
 *  @{
 */
#ifdef __cplusplus
#define EXTC extern "C"                    /**< Storage class for C++                */
#else
#define EXTC extern                        /**< Storage class for C                  */
#endif

#ifdef unix
#define QDS_API EXTC                       /**< Not required for Unix                */
#define WINCC                              /**< Not required for Unix                */
#else
#define WINCC __stdcall                    /**< Calling convention for Windows       */
#ifdef  QDS_EXPORTS
#define QDS_API EXTC __declspec(dllexport) /**< For internal use of this header    */
#else
#define QDS_API EXTC __declspec(dllimport) /**< For external use of this header    */
#endif
#endif
/** @} */

#ifdef _MSC_VER
typedef __int32 Int32;          /*!< 32-Bit-Integer fuer MSVC      */
#else
#include <inttypes.h>
typedef int32_t  Int32;         /*!< 32-Bit-Integer fuer GCC       */
#endif


/** @name Device constants
 *
 *  Properties of the quDIS
 *  @{
 */
#define QDS_AXES_CNT       3             /**< Number of axes                         */
/** @} */

/** @name Return values of the functions
 *
 *  All functions of this lib - as far as they can fail - return
 *  one of these constants for success control.
 *  @{
 */
#define QDS_Ok             0             /**< No error                               */
#define QDS_Error         -1             /**< Unspecified error                      */
#define QDS_Timeout        1             /**< Communication timeout                  */
#define QDS_NotConnected   2             /**< No active connection to device         */
#define QDS_DriverError    3             /**< Error in communication with driver     */
#define QDS_DeviceLocked   7             /**< Device is already in use by other      */
#define QDS_Unknown        8             /**< Unknown error                          */
#define QDS_NoDevice       9             /**< Invalid device number in function call */
#define QDS_NoAxis        10             /**< Invalid axis number in function call   */
#define QDS_ParamOutOfRg  11             /**< A parameter exceeds the allowed range  */
#define QDS_Capability    12             /**< The capability needed is not unlocked  */
#define QDS_OldValue      13             /**< The read value was already returned.   */
/** @} */


/** @name quDIS feature flags
 *  @anchor FFlags
 *
 *  The flags describe optional features of the quDIS device.
 *  They are used by @ref QDS_getDeviceConfig .
 *  @{
 */
#define QDS_FeatureAbsolute 0x01         /**< Absolute distance measurement enabled  */
#define QDS_FeatureEthernet 0x02         /**< TCP/IP support enabled                 */
#define QDS_FeaturePilot    0x04         /**< Device has a built in pilot laser      */
/** @} */
 

typedef int bln32;                       /**< Boolean; to avoid troubles with
                                              incomplete C99 implementations         */

/** Interface types
 */
typedef enum { IfNone = 0x00,            /**< Device invalid / not connected         */
               IfUsb3 = 0x10,            /**< Device connected via USB               */
               IfTcp  = 0x02,            /**< Device connected via ethernet (TCP/IP) */
               IfAll  = 0x12             /**< All physical interfaces                */
} QDS_InterfaceType;


/** Laser wavelength control status
 */
typedef enum { LcPrep = 0x00,            /**< Wavelength control: preparing          */
               LcOk   = 0x01,            /**< Wavelength control: ok                 */
               LcFail = 0x02             /**< Wavelength control: failed             */
} QDS_LCtlState;


/** Marker modes
 */
typedef enum { MrkOff  = 0x00,           /**< Marker feature off                     */
               Mrk1Cnt = 0x01,           /**< One single marker input, with counter  */
               MrkNone = 0x02            /**< Reserved for future marker modes       */
} QDS_MrkMode;


/** AMU modes
 */
typedef enum { AMUdisable = 0x00,           /**< Disable AMU correction */
               AMUenable  = 0x03,           /**< Enable AMU correction  */
} QDS_AMUmode;


/** Signal quality measures
 */
typedef struct {
  double contrast;                       /**< Interferogram contrast in % of optimum */
  double reserved1;                      /**< Reserved for future use                */
  double reserved2;                      /**< Reserved for future use                */
  double reserved3;                      /**< Reserved for future use                */
} QDS_QMeasure;


/** Position callback function
 *
 *  A function of this type can be registered as callback function for new
 *  position values with @ref QDS_setPositionCallback. This mechanism allows
 *  to achieve much higher data rates than polling every single sample.
 *
 *  The position measurements are taken on a regular timebase, the results are
 *  buffered for a short time and packed together to fit the requirements of the
 *  transport medium. When the packet arrives at the PC, the callback function is
 *  called as soon as possible. If the PC cannot process the data fast enough, the
 *  delay between measurement and call will increase with the filling level of the
 *  device internal buffers. Data packets will get lost when the buffers are full.
 *
 *  markers returns the value of a counter that counts rising and falling
 *  level of an external input signal. See @ref QDS_enableMarkers.
 *
 *  The index counts the data since the begin of the measurement, i.e. it is
 *  incremented from call to call by length. It also counts data that have been
 *  lost due to performance problems of the control PC. To avoid overflow, the
 *  index is resetted from time to time.
 *
 *  The buffers that contain the data are static and will be overwritten in
 *  the next call. It must not be free()'d or used by the application to
 *  store data.
 *  @param  devNo        Number of the device that produced the data
 *  @param  length       Number of triples of position values
 *  @param  index        Sequence number of the first position of the packet
 *  @param  positions    Array of three pointers, pointing to arrays of
 *                       positions [nm] of axes 1, 2, and 3. In angular mode
 *                       (see @ref QDS_enableAngularMode), arrays 2 and 3
 *                       contain angle values 1-2 and 1-3 in uDegrees.
 *  @param  markers      Marker counter at the time of data acquisition
 */
typedef void (* QDS_PositionCallback) ( unsigned int   devNo,
                                        unsigned int   length,
                                        unsigned int   index,
                                        const double * const positions[QDS_AXES_CNT],
                                        const Int32  * const markers[QDS_AXES_CNT]   );


/** Discover devices
 *
 *  The function searches for connected QDS devices on USB and LAN and
 *  initializes internal data structures per device. Devices that are in use
 *  by another application or PC are not found.
 *  The function must be called before connecting to a device and must not be
 *  called as long as any devices are connected.
 *
 *  The number of devices found is returned. In subsequent functions, devices
 *  are identified by a sequence number that must be less than the number returned.
 *  @param   ifaces    Interfaces where devices are to be searched
 *  @param   devCount  Output: number of devices found
 *  @return            Error code
 */
QDS_API int WINCC QDS_discover( QDS_InterfaceType ifaces,
                                unsigned int    * devCount );


/** Device information
 *
 *  Returns available information about a device. The function can not be
 *  called before @ref QDS_discover but the devices don't have to be
 *  @ref QDS_connect "connected" . All Pointers to output parameters may
 *  be zero to ignore the respective value.
 *  @param   devNo     Sequence number of the device
 *  @param   id        Output: programmed hardware ID of the device
 *  @param   serialNo  Output: The device's serial number. The string buffer
 *                     should be NULL or at least 16 bytes long.
 *  @param   address   Output: The device's interface address if applicable.
 *                     Returns the IP address in dotted-decimal notation or the
 *                     string "USB", respectively. The string buffer should be
 *                     NULL or at least 16 bytes long.
 *  @return            Error code
 */
QDS_API int WINCC QDS_getDeviceInfo( unsigned int devNo,
                                     int        * id,
                                     char       * serialNo,
                                     char       * address );


/** Register IP device in external Network
 *
 *  @ref QDS_discover is able to find devices connected via TCP/IP
 *  in the same network segment, but it can't "look through" routers.
 *  To connect devices in external networks, reachable by routing,
 *  the IP addresses of those devices have to be registered prior to
 *  calling @ref QDS_discover. The function registers one device and can
 *  be called several times.
 *
 *  The function will return QDS_Ok if the name resolution succeeds
 *  (QDS_NoDevice otherwise); it doesn't test if the device is reachable.
 *  Registered and reachable devices will be found by @ref QDS_discover.
 *  @param    hostname  Hostname or IP Address in dotted decimal notation
 *                      of the device to register.
 *  @return             Error code. QDS_NoDevice means here that the
 *                      hostname could not be resolved. A return code of 0
 *                      doesn't guarantee that the device is reachable.
 */
QDS_API int WINCC QDS_registerExternalIp( const char * hostname );


/** Connect device
 *
 *  Initializes and connects the selected device.
 *  This has to be done before any access to control variables or measured data.
 *  @param  devNo      Sequence number of the device
 *  @return            Error code
 */
QDS_API int WINCC QDS_connect( unsigned int devNo );


/** Disconnect device
 *
 *  Closes the connection to the device.
 *  @param  devNo      Sequence number of the device
 *  @return            Error code
 */
QDS_API int WINCC QDS_disconnect( unsigned int devNo );


/** Read device configuration
 *
 *  Reads static device configuration data
 *  @param  devNo      Sequence number of the device
 *  @param  axisCount  Output: Number of enabled axes
 *  @param  features   Output: Bitfield of enabled features,
 *                     see @ref FFlags "QDS Feature Flags"
 *  @return            Error code
 */
QDS_API int WINCC QDS_getDeviceConfig( unsigned int   devNo,
                                       unsigned int * axisCount,
                                       int          * features   );


/** Get wavelength control status
 *
 *  Reads the status of the laser wavelength control.
 *  After starting, the device is calibrating the laser temperature
 *  for some time. In this time, no reliable position information
 *  is available. Caused by external influences, such as extreme
 *  environmental temperature, the wavelength control may fail
 *  to hold the wavelength.
 *  @param  devNo      Sequence number of the device
 *  @param  state      Output: wavelength control status
 *  @return            Error code
 */
QDS_API int WINCC QDS_getLambdaControlState( unsigned int    devNo,
                                             QDS_LCtlState * state );


/** Set sample time
 *
 *  Sets the sample time, i.e. the inverse frequency of position
 *  updates. Internally, the device is sampling with 25kHz and can
 *  calculate averages over powers of 2 of internal samples.
 *  The sample time affects relative and absolute positions of all axes.
 *  @param  devNo      Sequence number of the device
 *  @param  lbSampleTm Logarithmic sample time. The actual sample time
 *                     will be (40us * 2^lbSampleTm). Range = 0...16
 *  @return            Error code
 */
QDS_API int WINCC QDS_setSampleTime( unsigned int devNo,
                                     unsigned int lbSampleTm );


/** Read back sample time
 *
 *  Reads the sample time as set by @ref QDS_setSampleTime.
 *  @param  devNo      Sequence number of the device
 *  @param  lbSampleTm Output: Logarithmic sample time. The actual 
 *                     sample time is (40us * 2^lbSampleTm).
 *  @return            Error code
 */
QDS_API int WINCC QDS_getSampleTime( unsigned int   devNo,
                                     unsigned int * lbSampleTm );


/** Reset relative position
 *
 *  Sets the relative position value of the corresponding axes to 0.
 *  @param  devNo      Sequence number of the device
 *  @param  axisNo     Axis number (0 ... 2)
 *  @return            Error code
 */
QDS_API int WINCC QDS_resetRelPosition( unsigned int devNo,
                                        unsigned int axisNo );


/** Read positions
 *
 *  Reads the measured positions of all axes at the same time.
 *  In angular mode (see @ref QDS_enableAngularMode) the relative positions
 *  2 and 3 are replaced by the respective angles. If the position was already
 *  read using this function, the return code will be QDS_OldValue.
 *  @param  devNo         Sequence number of the device
 *  @param  positionsRel  Output: current relative positions [nm] of axes 1, 2, and 3,
 *                        or position of axis 1 and angles 1-2, 1-3 [uDegrees].
 *                        The caller must provide an array of 3 Elements.
 *  @param  positionsAbs  Output: current absolute positions [nm] of axes 1, 2, and 3.
 *                        The caller must provide an array of 3 Elements.
 *  @return               Error code
 */
QDS_API int WINCC QDS_getPositions( unsigned int devNo,
                                    double     * positionsRel,
                                    double     * positionsAbs );


/** Configure position buffer.
 *
 * QDS_getPositions only reads the current position values. To get a
 * continuous stream of position values, you either have to use callback
 * functions (see @ref QDS_setPositionCallback) or configure a position
 * buffer. The buffer is filled with position values with a sample time
 * (see @ref QDS_setSampleTime) interval. To obtain the data use 
 * @ref QDS_readBuffer.
 * @param devNo      Sequence number of the device.
 * @param size       Size of the buffer in number of samples. The buffer
 *                   will discard the oldest values once it is full.
 * @return           Error code. 
 */
QDS_API int WINCC QDS_configureBuffer( unsigned int devNo,
                                       unsigned int size );


/** Read the position buffer
 *
 * Reads the position buffer that has been configured with @ref QDS_configureBuffer.
 * The buffer is filled with position values that were measured at equidistant
 * time intervals (sample time, see @ref QDS_setSampleTime).
 * @param devNo      Sequence number of the device.
 * @param bufferRel  Output: 2D array of relative positions.
 * @param bufferAbs  Output: 2D array of absolute positions.
 * @param size       In-/Output: Size of the buffer in number of samples. The
 *                   function will return the number of elements that have been
 *                   written to the buffers.
 * @return           Error code. 
 */
QDS_API int WINCC QDS_readBuffer( unsigned int devNo,
                                  double     * bufferRel[QDS_AXES_CNT],
                                  double     * bufferAbs[QDS_AXES_CNT],
                                  unsigned int * size );


/** Read the lost data points
 *
 * Reads the number of positions lost since the last call to this function.
 * @param devNo      Sequence number of the device.
 * @param lostRel    Output: Number of lost relative positions.
 * @param lostAbs    Output: Number of lost absolute positions.
 */
QDS_API int WINCC QDS_getLostData( unsigned int devNo,
                                   unsigned int lostRel[QDS_AXES_CNT],
                                   unsigned int lostAbs[QDS_AXES_CNT] );


/** Estimate signal quality
 *
 *  Reads an estimated signal quality from the device.
 *  Currently, only the (normalized) interferogram contrast is available.
 *  Other quality related measures will be added.
 *
 *  @note  Currently the function is broken; it will always return with error
 *
 *  @param  devNo      Sequence number of the device
 *  @param  axisNo     Axis number (0 ... 2)
 *  @param  qData      Quality related Data Set
 *  @return            Error code
 */
QDS_API int WINCC QDS_estimateSignalQuality( unsigned int devNo,
                                             unsigned int axisNo,
                                             QDS_QMeasure * qData );


/** Set Interferogram preamp gain
 *
 *  Sets a relative gain factor for the preamplifiers of the optical inputs.
 *  Can be used if not enough reflected light returns from the cavity.
 *  The setting will be saved in the device.
 *  @param  devNo   Sequence number of the device
 *  @param  axis    Axis number (0 ... 2)
 *  @param  gain    Preamp gain in % of the factory setting (30 ... 800), default=100
 *  @return         Error code
 */
QDS_API int WINCC QDS_setPreampGain( unsigned int devNo,
                                     unsigned int axis,
                                     unsigned int gain );


/** Read AMU results
 *
 *  Reads the parameters (temperature, pressure and relative humidity) measured
 *  by the ambient measurement unit (AMU). In addition the index of refraction
 *  calculated
 *  @param  devNo           Sequence number of the device
 *  @param  temperature     Output: current temperature [°C]
 *  @param  pressure        Output: current pressure [hPa]
 *  @param  relHumidity     Output: current relative humidity [%]
 *  @param  refractiveIndex Output: current index of refraction calculated using
 *                                  Ciddor's equation
 *  @return                 Error code
 */
QDS_API int WINCC QDS_readAMU( unsigned int devNo,
                               double     * temperature,
                               double     * pressure,
                               double     * relHumidity,
                               double     * refractiveIndex);


/** Set AMU mode
 *
 *  Reads the parameters (temperature, pressure and relative humidity) measured
 *  by the ambient measurement unit (AMU). In addition the index of refraction
 *  calculated
 *  @param  devNo   Sequence number of the device
 *  @param  axis    Axis to change AMU mode
 *  @param  mode    Desired AMU mode.
 *  @return         Error code
 */
QDS_API int WINCC QDS_setAMUmode( unsigned int devNo,
                                  unsigned int axis,
                                  QDS_AMUmode  mode);


/** Register callback function
 *
 *  Registers a callback function for a device that will be called when new
 *  position data are available. A callback function registered previously
 *  is unregistered. The feature requires an USB connection to the device;
 *  on ethernet the call will have no effect.
 *  @param  devNo    Sequence number of the device
 *  @param  cbRel    Callback function for relative positions. Use NULL to
 *                   unregister a function.
 *  @param  cbAbs    Callback function for absolute positions. Use NULL to
 *                   unregister a function.
 *  @return          Error code
 */
QDS_API int WINCC QDS_setPositionCallback( unsigned int         devNo,
                                           QDS_PositionCallback cbRel,
                                           QDS_PositionCallback cbAbs   );



/** Enable marker function
 *
 *  The marker feature allows to correlate the measured positions with an external
 *  electrical trigger signal. The rising and falling edges are counted, the current
 *  counter value can be read out together with the corresponding positions.
 *  The marker counter is resetted automatically when the trigger level is
 *  low for the auto reset tim.
 *  @param  devNo         Sequence number of the device
 *  @param  mode          Marker mode; currently only on or off
 *  @param  timeout       Auto reset time [ms]; 0 for no auto reset
 *  @return               Error code
 */
QDS_API int WINCC QDS_enableMarkers( unsigned int devNo,
                                     QDS_MrkMode  mode,
                                     unsigned int timeout );


/** Reset marker counter
 *
 *  The marker counter is reset to 0 or 1, depending on the current level of
 *  the trigger signal. This guarantees that the LSB of the marker counter
 *  always reflects the trigger signal level.
 *  @param  devNo         Sequence number of the device
 *  @return               Error code
 */
QDS_API int WINCC QDS_resetMarkerCounter( unsigned int devNo );


/** Set HSSL parameters
 *
 *  Sets parameters of the device's HSSL interface for an axis.
 *  The parameters are stored in the device and will be reloaded at the next start.
 *  @param  devNo      Sequence number of the device
 *  @param  axisNo     Axis number (0 ... 2)
 *  @param  enable     Enables / disables the interface.
 *  @param  useLvds    Use LVDS output signals
 *  @param  clock      Signal clock [40ns]. Range: 1 ... 64 (40ns ... 2.5us).
 *  @param  gap        Pause between two values [number of clock ticks].
 *                     Range is 0 ... 63.
 *  @param  bits       Number of Bits a single value consists of. Range: 8 ... 48.
 *  @param  lbResol    Logarithmic physical distance represented by the least
 *                     significant bit of the HSSL output.
 *                     The resolution can be varied only in powers of two:
 *                     resolution = 1pm * (2 ^ lbResol). Range: 0 ... 40 (1pm ... 1m)
 *  @param  lbAvgTime  Logarithmic average time for transmited positions.
 *                     The average time can be varied only in powers of two:
 *                     time = 80ns * (2 ^ lbAvgTime). Range: 0 ... 15 (80ns ... 2.6ms)
 *  @return            Error code
 */
QDS_API int WINCC QDS_setHsslParams( unsigned int devNo,
                                     unsigned int axisNo,
                                     bln32        enable,
                                     bln32        useLvds,
                                     unsigned int clock,
                                     unsigned int gap,
                                     unsigned int bits,
                                     unsigned int lbResol,
                                     unsigned int lbAvgTime );


/** Set quadrature parameters
 *
 *  Sets parameters of the device's quadrature interface for an axis.
 *  The parameters are stored in the device and will be reloaded at the next start.
 *  @param  devNo      Sequence number of the device
 *  @param  axisNo     Axis number (0 ... 2)
 *  @param  enable     Enables / disables the interface.
 *  @param  zeroOnReset If the interface should transmit virtual displacements
 *                     caused by resetting the axis.
 *  @param  resolution The displacement represented by a pair of edges [pm].
 *                     Allowed range is 1pm ... 1mm .
 *  @param  clock      Signal clock [40ns]. The signal period is four times the
 *                     clock time. Allowed range is 1 ... 32,767 (40ns ... 1.3ms).
 *  @param  lbAvgTime  Logarithmic average time for transmited positions.
 *                     The average time can be varied only in powers of two:
 *                     time = 80ns * (2 ^ lbAvgTime). Range: 0 ... 15 (80ns ... 2.6ms).
 *  @return            Error code
 */
QDS_API int WINCC QDS_setQuadratureParams( unsigned int devNo,
                                           unsigned int axisNo,
                                           bln32        enable,
                                           bln32        zeroOnReset,
                                           unsigned int resolution,
                                           unsigned int clock,
                                           unsigned int lbAvgTime );

/** Enable Angular Measurement
 *
 *  For angular measurement two right angled triangles are considered.
 *  The length of one cathetus is constant and known  while the other cathetus 
 *  is given by the difference of axis 1 and axis 2 (or axis 3, respectively)
 *  relative distances. In the angular mode, the smaller angles of both triangles
 *  are calculated and replace the relative distances of axes 2 and 3 while axis 1
 *  stays unchanged.
 *  @param  devNo      Sequence number of the device
 *  @param  enable     Enables / disables angular mode
 *  @param  base1_2    Baseline length of the first triangle (axes 1 vs. 2) [um]
 *  @param  base1_3    Baseline length of the second triangle (axes 1 vs. 3) [um]
 *  @return            Error code
 */
QDS_API int WINCC QDS_enableAngularMode( unsigned int devNo,
                                         bln32        enable,
                                         unsigned int base1_2,
                                         unsigned int base1_3 );

/** Pilot Laser Control
 *
 *  En- / disables the pilot laser of the respective device.
 *  @param  devNo      Sequence number of the device
 *  @param  axisNo     Axis number (0 ... 2)
 *  @param  enable     Enables / disables angular mode
 *  @return            Error code
 */
QDS_API int WINCC QDS_setPilotLaser( unsigned int devNo,
                                     unsigned int axisNo,
                                     bln32        enable );

#endif

