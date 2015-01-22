#!/bin/sh
#
#	This isn't a test application.
#	Initial setting of odbc(inst).ini.
#
outini=odbc.ini
outinstini=odbcinst.ini

drvr=../.libs/psqlodbcw
driver=${drvr}.so
if test ! -e $driver ; then
	driver=${drvr}.dll
	if test ! -e $driver ; then
		echo Failure:driver ${drvr}.so\(.dll\) not found
		exit 2
	fi
fi

echo creating $outinstini
cat << _EOT_ > $outinstini
[psqlodbc test driver]
Description     = PostgreSQL ODBC driver (Unicode version), for regression tests
Driver          = $driver
Debug           = 0
CommLog         = 1
_EOT_

echo creating $outini: $@
cat << _EOT_ > $outini
[psqlodbc_test_dsn]
Description             = psqlodbc regression test DSN
Driver          = psqlodbc test driver
Trace           = No
TraceFile               =
Database                = contrib_regression
Servername              =
Username                =
Password                =
Port                    =
ReadOnly                = No
RowVersioning           = No
ShowSystemTables                = No
ShowOidColumn           = No
FakeOidIndex            = No
ConnSettings            =
_EOT_

# Add any extra options from the command line
for opt in "$@"
do
    echo "${opt}" >> $outini
done
