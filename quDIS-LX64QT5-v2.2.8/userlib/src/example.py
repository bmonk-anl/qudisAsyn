# *****************************************************************************
#
#  Project:        quDIS User Library
#
#  Filename:       example.py
#
#  Purpose:        Minimal example for Python
#
#  Author:         N-Hands GmbH & Co KG
#
# *****************************************************************************

import ctypes
import platform


# Callback function
CALLBACK = ctypes.CFUNCTYPE(
    None,
    ctypes.c_uint,
    ctypes.c_uint,
    ctypes.c_uint,
    ctypes.POINTER(ctypes.POINTER(ctypes.c_double)),
    ctypes.POINTER(ctypes.c_int32),
)


@CALLBACK
def rel_position_callback(dev, count, index, pos, marker):
    # index + count = index of next call
    for i in range(count):
        print("relative: %f %f %f" % (pos[0][i], pos[1][i], pos[2][i]))


@CALLBACK
def abs_position_callback(dev, count, index, pos, marker):
    # index + count = index of next call
    for i in range(count):
        print("absolute: %f %f %f" % (pos[0][i], pos[1][i], pos[2][i]))


# API --------------------------------------------------------------


class QuDIS:
    def __init__(self):
        # load Lib -------------------------------------------
        if platform.system() == "Windows":
            try:
                self.qdslib = ctypes.windll.LoadLibrary("qudis.dll")
            except FileNotFoundError:
                from os.path import dirname, realpath, join

                full_name = join(dirname(realpath(__file__)), "qudis.dll")
                self.qdslib = ctypes.windll.LoadLibrary(full_name)
        if platform.system() == "Linux":
            self.qdslib = ctypes.cdll.LoadLibrary("./libqudis.so")

        # ------- tdcbase.h --------------------------------------------------------
        self.qdslib.QDS_discover.argtypes = [
            ctypes.c_int,
            ctypes.POINTER(ctypes.c_uint),
        ]
        self.qdslib.QDS_discover.restype = ctypes.c_int32

        self.qdslib.QDS_getDeviceInfo.argtypes = [
            ctypes.c_uint,
            ctypes.POINTER(ctypes.c_int),
            ctypes.POINTER(ctypes.c_char),
            ctypes.POINTER(ctypes.c_char),
        ]
        self.qdslib.QDS_getDeviceInfo.restype = ctypes.c_int32

        self.qdslib.QDS_connect.argtypes = [ctypes.c_uint]
        self.qdslib.QDS_connect.restype = ctypes.c_int32

        self.qdslib.QDS_disconnect.argtypes = [ctypes.c_uint]
        self.qdslib.QDS_disconnect.restype = ctypes.c_int32

        self.qdslib.QDS_setPositionCallback.argtypes = [
            ctypes.c_uint,
            CALLBACK,
            CALLBACK,
        ]
        self.qdslib.QDS_setPositionCallback.restype = ctypes.c_int

        self.qdslib.QDS_getPositions.argtypes = [
            ctypes.c_uint,
            ctypes.POINTER(ctypes.c_double),
            ctypes.POINTER(ctypes.c_double),
        ]
        self.qdslib.QDS_getPositions.restype = ctypes.c_int

    # Error Check --------------------------------------------------------------

    def __err_msg(self, code):
        if code == 0:
            return "Success"
        elif code == 1:
            return "Receive timed out"
        elif code == 2:
            return "No connection was established"
        elif code == 3:
            return "Error accessing the USB driver"
        elif code == 7:
            return "Can't connect device because already in use"
        elif code == 8:
            return "Unknown error"
        elif code == 9:
            return "Invalid device number used in call"
        elif code == 10:
            return "Parameter in function call is out of range"
        elif code == 11:
            return "Failed to open specified file"
        elif code == 12:
            return "Library has not been initialized"
        elif code == 13:
            return "Requested feature is not enabled"
        elif code == 14:
            return "Requested feature is not available"
        else:
            return "Unspecified error"

    def perror(self, environ, returnCode):
        if returnCode != 0:
            msg = self.qdslib.__err_msg(returnCode)
            print("Error in %s: %s" % (environ, msg))
            return True
        return False


# Example --------------------------------------------------------------


def find_all_deviced(qudis, dev_count):
    for device in range(dev_count):
        dev_id = ctypes.c_int()  # device id
        sno = (ctypes.c_char * 20)()  # serial number
        ipaddr = (ctypes.c_char * 20)()  # address
        qudis.qdslib.QDS_getDeviceInfo(device, ctypes.byref(dev_id), sno, ipaddr)

        str_sno = "".join(map(chr, sno.value))
        str_ip = "".join(map(chr, ipaddr.value))
        print(device, dev_id.value, str_sno, str_ip)


def poll_position(qudis, dev):
    from time import sleep

    for i in range(20):
        rel = (ctypes.c_double * 3)()
        abs = (ctypes.c_double * 3)()
        qudis.qdslib.QDS_getPositions(0, rel, abs)
        print("relative: %f %f %f" % (rel[0], rel[1], rel[2]))
        print("absolute: %f %f %f" % (abs[0], abs[1], abs[2]))
        sleep(0.05)


def stream_position(qudis, dev):
    from time import sleep

    qudis.qdslib.QDS_setPositionCallback(
        dev, rel_position_callback, abs_position_callback
    )
    sleep(2)


def main(argv):
    if_ethernet = 0x02
    if_usb3 = 0x10
    if_all = if_ethernet + if_usb3
    print("QuDIS Python Demo")
    qudis = QuDIS()
    dev_count = ctypes.c_uint()
    rc = qudis.qdslib.QDS_discover(if_all, ctypes.byref(dev_count))  # accept any device
    if qudis.perror("QDS_discover", rc):
        exit(1)
    print("Found %d devices" % (dev_count.value))
    find_all_deviced(qudis, dev_count.value)
    if dev_count.value > 0:
        qudis.qdslib.QDS_connect(0)
    poll_position(qudis, 0)
    print("Stopping to poll...")
    stream_position(qudis, 0)

    if dev_count.value > 0:
        qudis.qdslib.QDS_disconnect(0)


if __name__ == "__main__":
    from sys import argv

    main(argv[1:])
