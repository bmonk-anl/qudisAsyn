/*****************************************************************************
 *
 *  Project:        quDIS Interface
 *
 *  Filename:       qudis_protocol.h
 *
 *  Purpose:        Control Protocol Constants for quDIS
 *
 *  Author:         NHands GmbH & Co KG
 */
/*****************************************************************************/
/** @file qudis_protocol.h
 *  Control Protocol Constants for quDIS
 *
 *  This file defines constants to be used as parameters or parameter limits
 *  for the control protocol of ucprotocol.h .
 *  It is meant as a working C header as well as a documentation for users
 *  of other programming languages.
 *
 *  @note Constants beginning with ID_ are provided for the address field
 *        of the protocol. They are called addresses.
 *        All other constants are meant as values or limits for the data field.
 *
 *  @note Many addresses control a function of a specific axis. In this case
 *        the axis is identified by the index field of the protocol.
 */
/*****************************************************************************/
/* $Id: qudis_protocol.h,v 1.8 2021/08/13 09:24:41 trurl Exp $ */

#ifndef __QUDIS_PROTOCOL_H
#define __QUDIS_PROTOCOL_H


/** @name Global Limits and Resources
 *
 *  Resource counts and other addressing limitations.
 *  @{
 */

/** Number of Axes.
 *  Number of axes of the device. The axis number (used in the index field)
 *  has to be smaller than this number.
 */
#define QDS_NUM_AXES           3

/** Number of Data Channels.
 *  Number of @ref sec_channels "data channels" available for the transport
 *  of measurement results.
 */
#define QDS_NUM_CHANNELS       16

/** TCP Port Number
 *  The TCP port number the device is listening to.
 *  (The IP address has to be configured individually)
 */
#define QDS_TCP_PORT           2101

/** quDIS hardware Types
 *  Hardware identifier numbers of the existing quDIS variants.
 *  See @ref ID_GET_HW_TYPE .
 */
#define QDS_HWTYPE_0_1       0x8300     /**< Hardware version 0.1 (Serial numbers J 01 ...)      */
#define QDS_HWTYPE_1_0       0x8500     /**< Hardware version 1.0 (Serial numbers J 02 ...)      */
/** @} */


/** @name Device Features
 *
 *  Get requests installed device features. Index must be 0.
 *  @{
 */

/** Number of Axes available
 *  The parameter contains the number of axes statically enabled (1...3).
 */
#define ID_QDS_NUM_AXES_AV     0x0753

/** Absolute Distance Measurement
 *  The boolean parameter is true (1) if the absolute measurement feature is enabled.
 */
#define ID_QDS_CAP_ABSOLUTE_AV 0x0751

/** Ethernet Support
 *  The boolean parameter is true (1) if TCP/IP connectivity is enabled.
 */
#define ID_QDS_CAP_ETH_AV      0x0759

/** Pilot Laser available
 *  The boolean parameter is true (1) if a red pilot laser is built in the device.
 */
#define ID_QDS_CAP_PILOT_AV    0x0757
/** @} */


/** @name Device Identification
 *
 *  @details Every device provides a number ("Hardware ID") for identification
 *  in a multi device enviroment. This number can be individually programmed
 *  and read back. For programming, it is not sufficient to sent the new number
 *  to the device; it has to be saved to persistent memory explicitly.
 *  @{
 */

/** Retreive Hardware Type
 *  A GET on this address retrieves the hardware hardware type of the device.
 *  The hardware type is a fixed identifier that allows to identify the product
 *  and its hardware version. SET will be rejected.
 */
#define ID_GET_HW_TYPE        0x0167

/** Retreive Device ID
 *  A GET on this address retrieves the current hardware identification of the device.
 *  Factory default is -1 (0xFFFFFFFF). Index must be 0; SET will be rejected.
 */
#define ID_GET_HW_ID          0x0168

/** Set volatile Device ID
 *  A SET on this address sets the hardware identification number. Index must be 0.
 *  To save this value persistently in the device, use @ref ID_PROGRAM_ID.
 */
#define ID_SET_HW_ID          0x016A

/** Save Device ID persistently.
 *  A SET on this address with the field data set to 0x1234 saves the current
 *  identification number in persistent memory in the device. The value can be overwritten
 *  by @ref ID_SET_HW_ID and saved by @ref ID_PROGRAM_ID any time.
 */
#define ID_PROGRAM_ID         0x016F
/** @} */


/** @name Device Status
 *
 *  @details Output parameters that reflect the device status. The parameters
 *  can be inquired with GET, but are also autonomously sent by the device
 *  if the state changes.
 *  @{
 */

/** State of Wavelength Control
 *
 *  On startup, the quDIS initializes its wavelength control by adjusting the laser
 *  temperature. The parameter reflecs the state of that process.
 */
#define ID_QDS_MEAS_STATE     0x0773

/* Wavelength control states                                                                     */
#define QDS_MEAS_PREP              0    /**< preparing; yet no valid positions                   */
#define QDS_MEAS_OK                1    /**< ready, positions are valid                          */
#define QDS_MEAS_FAIL              2    /**< Wavelength control failed; no valid positions       */
/** @} */


/** @name Hardware Control
 *
 *  @details Settings for electrical and optical interfaces of the device.
 *  @{
 */

/** Pilot-Laser
 *  Switches the red pilot laser on (data=1) or off (data=0) for all axes.
 */
#define ID_QDS_PILOT_ON       0x07EA
/** @} */



/** @name Output Control
 *
 *  @details The following addresses control the output data stream.
 *  @{
 */

/** Enable asynchronous Events.
 *  State values (such as ID_QDS_TCTL_STATE) can be sent autonomously
 *  by the device if they change internally.
 *  The async enable telegram globally enables (data=1) or disables (data=0)
 *  all events.
 *  After successfully connecting to the device the events should be enabled.
 */
#define ID_ASYNC_EN           0x0145

/** Enable Streaming.
 *  Streaming is a method to transport measurement results,
 *  see @ref sec_channels "Data Channels". This telegram globally enables (data=1)
 *  or disables (data=0) streaming telegrams.
 *  After successfully connecting to the device streaming should be enabled.
 */
#define ID_DATA_EN            0x0146
/** @} */


/** @name Data Channels
 *
 *  The controller provides a number of data channels (@ref QDS_NUM_CHANNELS),
 *  see @ref sec_channels "Data Channels". The ID_CHAN_ addresses are used to
 *  configure the data that are sent on a specific channel:
 *  data source, triggering, and sampling. The ID_... constants are used for
 *  the "address" field, QDS_TRG_... and QDS_SRC_... are valid enumeration values
 *  for @ref ID_CHAN_TRIGGER and @ref ID_CHAN_SOURCE telegrams, respectively.
 *
 *  Data encoding depends on the data source.
 *  For the encoding of position data, see @ref sec_positions "here".
 *
 *  The index is valid and transports the channel number of the channel
 *  to be configured.
 *  @{
 */
#define ID_CHAN_TRIGGER       0x0030  /**< Data channel trigger (one of QDS_TRG_..)               */
#define ID_CHAN_SOURCE        0x0031  /**< Data channel source (one of QDS_SRC_..)                */
#define ID_QDS_DSIZE_EVEN     0x0696  /**< Force even packet size (bool), useful for positions    */

/** Data Channel Base Address
 *  When a data channel has been configured sucessfully, the device will autonomously
 *  send streaming telegrams. Their opcode is TELL, the address will be
 *  ID_CHAN_BASE + channel number. The Index is used as a data counter. Read only.
 */
#define ID_CHAN_BASE          0x0F00

/* Data Triggers (for ID_CHAN_CONNECT)                                                            */
#define QDS_TRG_DISABLED       0      /**< Data trigger: none (channel is disabled)               */
#define QDS_TRG_POS            2      /**< Data trigger: averaged positions                       */
#define QDS_TRG_SWEEP          5      /**< Data trigger: wavelength sweep (suitable e.g.
                                           for interferograms                                     */

/* Data Sources (for ID_CHAN_SOURCE)                                                              */
#define QDS_SRC_INTERFRG_1     9      /**< Data source: interferogram axis 1                      */
#define QDS_SRC_INTERFRG_2    10      /**< Data source: interferogram axis 2                      */
#define QDS_SRC_INTERFRG_3    11      /**< Data source: interferogram axis 3                      */
#define QDS_SRC_REL_1          1      /**< Data source: relative position axis 1                  */
#define QDS_SRC_REL_2          3      /**< Data source: relative position axis 2                  */
#define QDS_SRC_REL_3          5      /**< Data source: relative position axis 3                  */
#define QDS_SRC_ABS_1          2      /**< Data source: absolute position axis 1                  */
#define QDS_SRC_ABS_2          4      /**< Data source: absolute position axis 2                  */
#define QDS_SRC_ABS_3          6      /**< Data source: absolute position axis 3                  */
/** @} */


/** @name Position Control
 *
 *  @details The following parameters control details of the position output streams
 *  @{
 */

/** @brief Position Average Time
 *
 *  Controls the average time that is applied to all relative and absolute positions.
 *  The parameter is logarithmic; the actual average time is calculated as follows:
 *  time = 40us * (2 ^ parameterValue).  Range = 0 ... 16
 *  Affects all axes, the index field must be 0.
 */
#define ID_QDS_POS_AVG        0x0771


/** @brief Reset Relative Position
 *
 *  Sets the relative position of an axis to 0.
 *  The axis is selected by the index field of the telegram.
 *  Index = 3 resets the positions of all axes at the same time.
 *  The data field must be set to 1.
 */
#define ID_QDS_RESET          0x060D


/** @brief Reset Position Index
 *
 *  Requests resetting the index field of all position data streams to 0.
 *  Affects all axes, the index field must be 0.
 */
#define ID_QDS_INDEX_RESET    0x0770
/** @} */


/** @name Marker Feature
 *
 *  @details The marker feature allows to mark positions in data streams
 *  by using an external electrical trigger input. The edges of the trigger signal
 *  (rising and falling) are counted, and the count is included in data telegrams
 *  as part of the 64 bit samples.
 *  @{
 */

/** @brief Enable Marker Feature
 *
 *  The marker feature is enabled (data=1) or disabled (data=0).
 *  The index field must be 0.
 */
#define ID_QDS_TRG_ENABLE     0x07EB

/** @brief Reset Trigger Counter
 *
 *  The trigger counter is set to 0 (trigger signal low) or 1 (high)
 *  The index field must be 0, the data field should be 1.
 */
#define ID_QDS_TRG_RESET      0x07EC

/** @brief Timeout for Automatic Counter Reset
 *
 *  The trigger counter is automatically set to 0 if the trigger signal stays low
 *  for the timeout time. The parameter transports the timeout time in ms.
 *  A value of 0 disables the automatic reset.
 */
#define ID_QDS_TRG_AUTORESET  0x07ED
/** @} */


#endif
