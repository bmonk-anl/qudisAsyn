//*****************************************************************************
//
//  Project:        quDIS Control Library
//
//  Filename:       qudis-example.cs
//
//  Purpose:        example for DLL usage with C#
//
//  Author:         N-Hands GmbH & Co KG
//
//*****************************************************************************
// $Id: example.cs,v 1.1 2017/01/17 17:44:23 trurl Exp $

using System;
using System.Threading;
using System.Runtime.InteropServices;

namespace QudisExample
{
    class Example
    {
        // Import DLL functions from qudis.dll
        [DllImport("qudis.dll")]
        unsafe public static extern Int32 QDS_discover(Int32 ifaces, out UInt32 devCount);
        [DllImport("qudis.dll")]
        unsafe public static extern Int32 QDS_getDeviceInfo(UInt32 devNo, out Int32 id, char* serialNo, char* address);
        [DllImport("qudis.dll")]
        unsafe public static extern Int32 QDS_connect(UInt32 devNo);
        [DllImport("qudis.dll")]
        unsafe public static extern Int32 QDS_disconnect(UInt32 devNo);
        [DllImport("qudis.dll")]
        unsafe public static extern Int32 QDS_getLambdaControlState(UInt32 devNo, out Int32 state);
        [DllImport("qudis.dll")]
        unsafe public static extern Int32 QDS_setSampleTime(UInt32 devNo, UInt32 lbSampleTime);
        [DllImport("qudis.dll")]
        unsafe public static extern Int32 QDS_resetRelPosition(UInt32 devNo, UInt32 axisNo);
        [DllImport("qudis.dll")]
        unsafe public static extern Int32 QDS_getPositions(UInt32 devNo, double * positions, double * positionsAbs);

        // quDIS return codes (from qudis.h)
        const Int32  QDS_Ok           =  0;               // No error
        const Int32  QDS_Error        = -1;               // Unspecified error
        const Int32  QDS_Timeout      =  1;               // Communication timeout
        const Int32  QDS_NotConnected =  2;               // No active connection to device
        const Int32  QDS_DriverError  =  3;               // Error in comunication with driver
        const Int32  QDS_DeviceLocked =  7;               // Device is already in use by other program
        const Int32  QDS_Unknown      =  8;               // Unknown error
        const Int32  QDS_NoDevice     =  9;               // Invalid device number in function call
        const Int32  QDS_NoAxis       = 10;               // Invalid axis number in function call
        const Int32  QDS_ParamOutOfRg = 11;               // A parameter exceeds the allowed range

        // Interface Types (from qudis.h)
        const Int32 IfTcp             = 0x02;             // Ethernet interface
        const Int32 IfUsb3            = 0x10;             // USB3 interface
        const Int32 IfAll             = 0x12;             // All interfaces

        // Device states (from qudis.h)
        const Int32 LcPrep           = 0;                 // Device preparing
        const Int32 LcOk             = 1;                 // Device ready
        const Int32 LcFail           = 2;                 // Device preparation failed

        const double SAMPLE_TIME_BASE = 40e-6;            // Base unit of sample times (40us) in s


// ************************************* Convert error code to text

        static String getMessage(Int32 code)
        {
            switch(code)
            {
                case QDS_Ok:           return "";
                case QDS_Error:        return "Unspecified error";
                case QDS_Timeout:      return "Communication timeout";
                case QDS_NotConnected: return "No active connection to device";
                case QDS_DriverError:  return "Error in comunication with driver";
                case QDS_DeviceLocked: return "Device is already in use by other program";
                case QDS_Unknown:      return "Unknown error";
                case QDS_NoDevice:     return "Invalid device number in function call";
                case QDS_NoAxis:       return "Invalid axis number in function call";
                case QDS_ParamOutOfRg: return "A parameter exceeds the allowed range";
                default:               return "Unknown error code";
            }
        }


// ************************************* Check error code; abort on error

        static void checkError(String context, int code)
        {
            if (code != QDS_Ok)
            {
                throw new Exception("Error calling " + context + ": " + getMessage(code));
            }
        }


// *************************************  Discover devices and select one

        static unsafe UInt32 selectDevice()
        {
            UInt32 devCount = 0;
            Int32 rc = QDS_discover( IfAll, out devCount );
            checkError( "QDS_discover", rc );
            if ( devCount <= 0 ) {
                throw new Exception("No quDIS devices found");
            }

            for (UInt32 i = 0; i < devCount; ++i)
            {
                Int32 id = 0;
                IntPtr cSN   = Marshal.AllocHGlobal(20);
                char*  pSN   = (char*)cSN.ToPointer();
                IntPtr cAddr = Marshal.AllocHGlobal(20);
                char*  pAddr = (char*)cAddr.ToPointer();
                rc = QDS_getDeviceInfo(i, out id, pSN, pAddr);
                checkError("QDS_getDeviceInfo", rc);
                String serial  = Marshal.PtrToStringAnsi(cSN);
                Marshal.FreeHGlobal(cSN);
                String address = Marshal.PtrToStringAnsi(cAddr);
                Marshal.FreeHGlobal(cAddr);
                Console.WriteLine("Device {0}: Id={1} SerialNo={2}", i, id, serial);
            }

            Int32 devNo = 0;
            if ( devCount > 1 ) {
                Console.Write("Select device: ");
                String line = Console.ReadLine();
                Console.Write("\n");
                Int32.TryParse(line, out devNo);
                devNo = devNo >= devCount ? 0 : devNo;
            }

            return (UInt32) devNo;
        }


        // *************************************  Wait for device preparation to complete

        static unsafe void waitForPreparation(UInt32 devNo)
        {
            Int32 rc       = QDS_Ok;
            Int32 devState = LcPrep;
            while (devState != LcOk)
            {
                rc = QDS_getLambdaControlState(devNo, out devState);
                checkError("QDS_getLambdaControlState", rc);
                if (devState == LcFail)
                {
                    throw new Exception("Device preparation failed");
                }
                if (devState == LcPrep)
                {
                    Console.WriteLine("Waiting for device preparation...");
                    Thread.Sleep(1000);
                }
            }
        }


        // *************************************  Polling loop to retrieve positions

        static unsafe void readPositions(UInt32 devNo)
        {

            IntPtr   cRel = Marshal.AllocHGlobal(Marshal.SizeOf(typeof(double))*3);
            IntPtr   cAbs = Marshal.AllocHGlobal(Marshal.SizeOf(typeof(double))*3);
            double * pRel = (double *) cRel.ToPointer();
            double * pAbs = (double *) cAbs.ToPointer();
            Int32    rc   = QDS_getPositions(devNo, pRel, pAbs);
            checkError("QDS_getPositions", rc);
            Console.WriteLine("{0} nm {1} nm {2} nm - {3} nm {4} nm {5} nm",
                              pRel[0], pRel[1], pRel[2], pAbs[0], pAbs[1], pAbs[2]);
            Marshal.FreeHGlobal(cRel);
            Marshal.FreeHGlobal(cAbs);
        }


// *************************************  Main loop

        unsafe static void Main(string[] args)
        {
            UInt32 devNo = 9999;
            Int32  rc    = QDS_Ok;
            
            Console.WriteLine("quDIS example program");
            try
            {
                devNo = selectDevice();
                rc = QDS_connect(devNo);
                checkError("QDS_connect", rc);
                waitForPreparation(devNo);

                for (UInt32 i = 0; i < 3; ++i)
                {
                    rc = QDS_resetRelPosition(devNo,i);
                    checkError("QDS_resetRelPosition", rc);
                }
                for (UInt32 i = 0; i < 30; ++i)
                {
                    readPositions(devNo);
                    Thread.Sleep( 100 );
                }
            }
            catch(Exception e)
            {
                Console.WriteLine(e.Message);
            }

            QDS_disconnect(devNo);  // No problem if not connected
        }
    }
}
