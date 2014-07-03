The files:

  msdtc_pgxalib_tracing_enable.reg
  msdtc_pgxalib_tracing_disable.reg

are Windows Registry Editor files that can be opened to add registry entries
that turn psqlODBC's tracing for the Microsoft Distributed Transaction
Co-Ordinator (MSDTC) on and off.

You must also create C:\pgdtclog and ensure NETWORKSERVICE has write
permissions. (It will by default on most Windows installs). At time of writing
the trace path is hard coded to this location.

This tracing is separate to regular ODBC API tracing and to psqlODBC's "mylog"
tracing. It only traces access to the XA transaction manager used by MSDTC when
doing XA transaction co-ordination. You probably only need this if you're
debugging issues with "In doubt" or "only failed to notify" transactions in
MSDTC.


If you're tracing XA transactions in MSDTC you probably also want to trace
MSDTC its self. For that, see:

http://msdn.microsoft.com/en-us/library/ms684379(v=vs.85).aspx
http://msdn.microsoft.com/en-us/library/ms678917(v=vs.85).aspx

Tests suggest that only some SDKs' versions of the tracefmt.exe tool required
will work - I found that Windows SDK 7.1's worked, but Visual Studio Express
for Desktop 2012's didn't. Your results may vary.
