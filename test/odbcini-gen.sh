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

drvra=../.libs/psqlodbca
drivera=${drvra}.so
if test ! -e $drivera ; then
	drivera=${drvra}.dll
	if test ! -e $drivera ; then
		echo Failure:driver ${drvra}.so\(.dll\) not found
		exit 2
	fi
fi

echo creating $outinstini
cat << _EOT_ > $outinstini
[ODBC]
Trace = off
TraceFile =
[PostgreSQL Unicode]
Description     = PostgreSQL ODBC driver (Unicode version), for regression tests
Driver          = $driver
Debug           = 0
CommLog         = 0
[PostgreSQL ANSI]
Description     = PostgreSQL ODBC driver (ANSI version), for regression tests
Driver          = $drivera
Debug           = 0
CommLog         = 0
_EOT_

echo creating $outini: $@
# Unicode
cat << _EOT_ > $outini
[psqlodbc_test_dsn]
Description             = psqlodbc regression test DSN
Driver          = PostgreSQL Unicode
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
ConnSettings            = set lc_messages='C'
_EOT_

# Add any extra options from the command line
for opt in "$@"
do
    echo "${opt}" >> $outini
done

# ANSI
cat << _EOT_ >> $outini
[psqlodbc_test_dsn_ansi]
Description             = psqlodbc ansi regression test DSN
Driver          = PostgreSQL ANSI
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
ConnSettings            = set lc_messages='C'
_EOT_

# Add any extra options from the command line
for opt in "$@"
do
    echo "${opt}" >> $outini
done
