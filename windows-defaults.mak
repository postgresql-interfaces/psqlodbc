#
# This file includes default configuration options used to build the driver
# on Windows using "nmake /f win64.mak". You can override these by creating a
# file in the same directory called "windows-local.mak". It will be processed
#after these defaults. (You can copy this file as a template, and modify)
#
!IF "$(TARGET_CPU)" == "x86"
PG_INC=C:\Program Files (x86)\PostgreSQL\9.6\include
PG_LIB=C:\Program Files (x86)\PostgreSQL\9.6\lib
!ELSE
PG_INC=$(PROGRAMFILES)\PostgreSQL\9.6\include
PG_LIB=$(PROGRAMFILES)\PostgreSQL\9.6\lib
!ENDIF

# Enable/disable features

MSDTC = yes
