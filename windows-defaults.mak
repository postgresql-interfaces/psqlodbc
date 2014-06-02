#
# This file includes default configuration options used to build the driver
# on Windows using "nmake /f win64.mak". You can override these by creating a
# file in the same directory called "windows-local.mak". It will be processed
#after these defaults. (You can copy this file as a template, and modify)
#
!IF "$(TARGET_CPU)" == "x86"
PG_INC=C:\Program Files (x86)\PostgreSQL\9.3\include
PG_LIB=C:\Program Files (x86)\PostgreSQL\9.3\lib
!ELSE
PG_INC=$(PROGRAMFILES)\PostgreSQL\9.3\include
PG_LIB=$(PROGRAMFILES)\PostgreSQL\9.3\lib
!ENDIF

# these will only used if building with USE_LIBPQ (which is the default)
!IF "$(TARGET_CPU)" == "x86"
SSL_INC=C:\OpenSSL-Win32\include
SSL_LIB=C:\OpenSSL-Win32\lib
!ELSE
SSL_INC=C:\OpenSSL-Win64\include
SSL_LIB=C:\OpenSSL-Win64\lib
!ENDIF

# Enable/disable features

USE_LIBPQ = yes
USE_SSPI = no
USE_GSS = no
MSDTC = yes
