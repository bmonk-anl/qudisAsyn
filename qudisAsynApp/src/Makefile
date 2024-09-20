TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#USR_CXXFLAGS=-I/usr/include/mono-2.0
#USR_CXXFLAGS=-clr:safe

#==================================================
# build a support library

USR_INCLUDES += -I..
LIB_INSTALLS += ../libqudis.a
LIB_INSTALLS += ../libqudis.so
BIN_INSTALLS += ../libqudis.so

# LIBRARY_IOC += qudis-asyn
LIBRARY_IOC += qudisAsyn

# xxxRecord.h will be created from xxxRecord.dbd
#DBDINC += xxxRecord
# install qudis.dbd into <top>/dbd
DBD += qudisAsynSupport.dbd

# specify all source files to be compiled and added to the library
qudisAsyn_SRCS += qudisAsyn.cpp
# qudis_LIBS_Linux += Wrapper
# ifeq (win, $(findstring win, $(T_A)))
# qudis_LIBS += CommsWrapper
# endif

qudisAsyn_LIBS += asyn
qudisAsyn_LIBS += qudis
qudisAsyn_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

