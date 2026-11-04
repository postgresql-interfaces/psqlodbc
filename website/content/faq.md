---
title: "psqlODBC FAQ"
type: docs
weight: 3
aliases:
  - /faq.html
---

Mailing lists may be useful when problems do not solve with these.\
[Discussion of PostgreSQL's ODBC interface.](https://www.postgresql.org/list/pgsql-odbc/)

The most recent version of this document can be viewed at <https://odbc.postgresql.org/faq.html>.

------------------------------------------------------------------------

## 1) General

[1.1](#q1-1)) How do I submit an idea for a new feature or even a patch?\
[1.2](#q1-2)) Do I even need to compile the driver to use it?\
[1.3](#q1-3)) How do I install the driver?\
[1.4](#q1-4)) What operating systems will the driver run on?\
[1.5](#q1-5)) I've seen other PostgreSQL ODBC drivers out there. Which one is the right one?\
[1.6](#q1-6)) How do I report a bug or other problems?

## 2) Basics

[2.1](#q2-1)) How do I setup a datasource?\
[2.2](#q2-2)) What's the difference between a File DSN, System DSN, and User DSN?\
[2.3](#q2-3)) How do I access more advanced driver and/or datasource options?\
[2.4](#q2-4)) Where can I discover more information about ODBC errors?\
[2.5](#q2-5)) There are 2 drivers installed - which should I use?

## 3) Connections

[3.1](#q3-1)) Why do I get a message like "Failed to authenticate client as Postgres user using unknown authentication type:be_recvauth: unrecognized message type: 65536" when I try to connect to a datasource?\
[3.2](#q3-2)) Why do I get a message like "User authentication failed"?\
[3.3](#q3-3)) What do I need to do to establish a connection to a database?\
[3.4](#q3-4)) Does psqlODBC support encrypted login for connections?

## 4) Advanced

[4.1](#q4-1)) Why do characters with umlauts or accents, or other non-ASCII characters show up in some applications as '?'\
[4.2](#q4-2)) What data types does the driver support?\
[4.3](#q4-3)) How do I use the row versioning -OR- why do I get a message about no operator for xid and int4?\
[4.4](#q4-4)) Can I use large objects or OLE?\
[4.5](#q4-5)) Why does the PostgreSQL backend complain about running out of memory when some I browse tables with primary keys?\
[4.6](#q4-6)) How do I get my application to recognize primary keys?

## 5) Borland Applications

[5.1](#q5-1)) What driver/datasource options work well with Borland products?\
[5.2](#q5-2)) \# Why do varchar/char datatypes not appear with the correct precision in Borland DBExplorer -OR- why do all varchar/char precisions appear as 128?

## 6) Microsoft Applications

[6.1](#q6-1)) Why don't int4 based aggregates seem to work?\
[6.2](#q6-2)) Why does Access force me specify the Data Source each time I run my SQL Pass-Thru query?\
[6.3](#q6-3)) Why does MS Access sometimes complain about a GROUP BY or ORDER BY not being in the target list?\
[6.4](#q6-4)) Why do I get 'Write Conflict - This record has been changed by another user since you started editing it' in Access 2000 and above?\
[6.5](#q6-5)) With MS Access, why can't I index on text fields -OR- why do I get "Invalid field definition 'field'" in definition of index or relationship?\
[6.6](#q6-6)) With MS Access, why can't I GROUP BY, ORDER BY, or even select WHERE, on columns which are of type "text"?\
[6.7](#q6-7)) With MS Access, why do I see \#Deleted# in rows after some operations?\
[6.8](#q6-8)) Installing psqlODBC on 64bit Windows.

## 1) General

### 1.1) How do I submit an idea for a new feature or even a patch? {#q1-1}

You can send mail to <pgsql-odbc@postgresql.org>.

### 1.2) Do I even need to compile the driver to use it? {#q1-2}

Probably not. If you are using Windows, all you really need to do is download and run the Full distribution.

### 1.3) How do I install the driver? {#q1-3}

The easiest way to install the driver is to get the Full distribution. Just download, unzip and run the setup program program. The installation is a standard Windows Installer package that will guide you through the process. For future upgrades, the MSI distribution can be used. This is considerably smaller as it does not include the Windows Installer redistributable which you only need install once. The second option is to compile the driver yourself. If you are using this driver on a unix platform, this is currently your only option.

### 1.4) What operating systems will the driver run on? {#q1-4}

The psqlODBC executable is written and compiled to run on 32 bit Wintel platform. This includes Windows 95, 98, ME, NT, 2000 and XP. 16 bit applications are capable of using the 32 bit driver but only on these platforms.

We do not distribute binaries for Unix. The source code, however has been ported to compile under Unix. There are two driver managers available for Unix, [UnixODBC](http://www.unixodbc.org/) and [iODBC](http://www.iodbc.org/).

### 1.5) I've seen other PostgreSQL ODBC drivers out there. Which one is the right one? {#q1-5}

This one! This is *the* official PostgreSQL ODBC driver.

### 1.6) How do I report a bug or other problems? {#q1-6}

You can send mail to pgsql-odbc@postgresql.org When you do, however, you should attach the following:

- your ODBC driver version
- your PostgreSQL database version
- any error messages you saw on the screen
- the debug log (mylog\_????.log) file (you will have to enable the mylog driver option)

Before posting however, please ensure you are running the latest version of psqlODBC.

## 2) Basics

### 2.1) How do I setup a datasource? {#q2-1}

For Windows, use the ODBC Administrator in Control Panel (in some releases it may be found in the Administrative Tools folder. Here you can add, modify, or delete data sources.

### 2.2) What's the difference between a File DSN, System DSN, and User DSN? {#q2-2}

System DSN's and User DSN's differ only in who can access them on the system. A File DSN, however, is not really a datasource. It is a file that contains all the connection parameters used to connect directly to an ODBC driver.

### 2.3) How do I access more advanced driver and/or datasource options? {#q2-3}

First, select your favorite datasource to configure. Then in the "PostgreSQL Driver Setup" dialog, select under "Options (Advanced)" either the Driver button or Datasource button. This will bring up another dialog box which contains options you can configure. Refer to the Configuration Help for information on all these options.

### 2.4) Where can I discover more information about ODBC errors? {#q2-4}

The ODBC driver has an option to log all direct communication (queries, updates, etc.) with the backend as well as error messages and notices in the commlog file. In addition, it now also logs any ODBC connection and statement errors in this file with detailed information. This is good for applications that give misleading, little, or no descriptive information when something goes wrong (VisData is a good example).

### 2.5) There are 2 drivers installed - which should I use? {#q2-5}

**PostgreSQL Unicode** is a Unicode enabled driver that will work well with modern versions of applications such as Microsoft Access, with character from a huge range of languages. You should use this driver with PostgreSQL databases encoded as 'UNICODE' (more precisely known as 'UTF-8' in PostgreSQL).

**PostgreSQL ANSI** is an ANSI driver which is also able to handle some multibyte character sets such as EUC_JP, BIG5 and Shift-JIS. This driver should also be used with databases encoded using any of the LATIN charactersets.

Note that some applications (notably Borland BDE) do not work properly with Unicode ODBC drivers. In this case, you must use the ANSI driver.

## 3) Connections

### 3.1) Why do I get a message like "Failed to authenticate client as Postgres user using unknown authentication type:be_recvauth: unrecognized message type: 65536" when I try to connect to a datasource? {#q3-1}

This message comes from the PostgreSQL backend, most likely when there is a protocol mismatch between the ODBC driver and the backend. For example, if you are using PostgreSQL 6.2 as the backend and try to use the ODBC driver without correctly setting the protocol, this error will occur. You must check the "6.2 protocol" advanced datasource option in the ODBC driver configuration dialog.

**Note:** If you're still running 6.x these days, you really, really, really should upgrade to 8.x as soon as possible!!

### 3.2) Why do I get a message like "User authentication failed"? {#q3-2}

Verify that the database you are trying to connect to exists and is accessible by you. Also, see the above question in regards to user name and password authentication.

### 3.3) What do I need to do to establish a connection to a database? {#q3-3}

ODBC Connection Checklist:

- ODBC Driver Data Source Checks
  - Valid and resolvable hostname.
  - Valid port number (default is 5432).
  - Valid and existing database name.
  - Valid user name.
  - Valid password for the specified user (required if pg_hba.config is set to password authenticate your client).

  
- PostgreSQL Server Checks
  - Postmaster must be running.
  - Postmaster must run with the -i option , or tcpip=true in postgresql.conf to allow remote connections.
  - The pg_hba.conf file in /data directory must be configured to allow your remote host to connect.

### 3.4) Does psqlODBC support encrypted login for connections? {#q3-4}

Yes. psqlODBC supports "md5" encrypted logins, but not "crypt" logins. Keep in mind that after login, ODBC sends all queries in plain text, so all you're protecting is your password. Also, support for md5 logins was added in late 2001, so if you have an older version of psqlODBC you may need to update it.

**Note:** As of version 08.01.002, psqlODBC now supports SSL encrypted connections.

## 4) Advanced

### 4.1) Why do characters with umlauts or accents, or other non-ASCII characters show up in some applications as '?' {#q4-1}

You are probably using the **PostgreSQL Unicode** driver with non-Unicode, 8 bit data - for example, from one of the LATIN encodings. You should either use the **PostgreSQL ANSI** driver, or move your data to a Unicode database.

### 4.2) What data types does the driver support? {#q4-2}

The driver provides full support for all PostgreSQL standard data types. These are: bool, int2, int4, int8, float4, float8, date, time, abstime, datetime, timestamp, char, varchar, and text.

There is partial support for all other data types. Examples of these: point, circle, box and arrays. String support only is provided for these non-standard types. In other words, they are returned as SQL_VARCHAR and can be displayed and updated like any other data type. The resulting behavior will probably vary some with each application and data type.

In the case of int4\[\] and MS Access 97 it does an effective job. The array can be displayed and updated cleanly. Arithmetic is a little tricky. MS Access complains about "t.a\[1\]" in the query builder. It does not like the syntax and never sends it to the backend. The work around is to choose the SQL Pass-thru option. This will allow you to build expressions like "t.a\[0\] \* t.a\[1\]". The hassle is that every time you run the query in Access 97 it prompts you with a database connection dialog.

MS Excel in combination w/ MS Query may provide a better solution. It passes every query through. I try to stay away from the more exotic types if I know I am going to expose them to the public.

### 4.3) How do I use the row versioning -OR- why do I get a message about no operator for xid and int4? {#q4-3}

Some of the operators are missing in some releases of PostgreSQL (anyone remember which?) so in order to use row versioning, you must overload the int4eq function for use with the xid type. Also, you need to create an operator to compare xid to int4. You must do this for each database you want to use this feature on. This will probably not be necessary in PostgreSQL 6.4 since it will be added. Here are the details:

        create function int4eq(xid,int4)
            returns bool
            as ''
            language 'internal';

        create operator = (
            leftarg=xid,
            rightarg=int4,
            procedure=int4eq,
            commutator='=',
            negator='<>',
            restrict=eqsel,
            join=eqjoinsel
        );
        

### 4.4) Can I use large objects or OLE? {#q4-4}

Large objects are mapped to LONGVARBINARY in the driver to allow storing things like OLE objects in Microsoft Access. Multiple SQLPutData and SQLGetData calls are usually used to send and retrieve these objects. The driver creates a new large object and simply inserts its 'identifier' into the respective table. However, since PostgreSQL uses an 'Oid' to identify a Large Object, it is necessary to create a new PostgreSQL type to be able to discriminate between an ordinary Oid and a Large Object Oid. Until this new type becomes an official part of PostgreSQL, it must be added into the desired database and looked up for each connection. The type used in the driver is simply called "lo" and here is the command used to create it:

        create type lo (
            internallength=4,
            externallength=10,
            input=int4in,
            output=int4out,
            default='',
            passedbyvalue
        );

        create table employee (
            id integer,
            name varchar(30),
            picture lo
        );
        

Once this is done, simply use the new 'lo' type to define columns in that database. When the driver sees an 'lo' type, it will handle it as SQL_LONGVARBINARY.

Another important note is that this new type is lacking in functionality. It will not cleanup after itself on updates and deletes, thus leaving orphans around and using up extra disk space. And currently, PostgreSQL does not support the vacuuming of large objects.

It would not be too difficult to write a interim stand-alone cleanup process to run at some interval on the server. It is only a matter of searching pg_attribute for lo data type columns and building a list of lo's by querying each table that contains lo's. Then compare this list with with the xinv.\* in pg_class. The xinv.\* with out a pointer are orphans and should be dropped.

Hopefully in the future, a real large object data type will be available as a base type. But for now, it sure is fun to stick a Word document, Visio document, or AVI of a dancing baby into a database column, even if you will fill up your server's hard disk after a while!

### 4.5) Why does the PostgreSQL backend complain about running out of memory when some I browse tables with primary keys? {#q4-5}

The Jet Database Engine (used by Access) and others can use "keysets" to access records. Depending on how many parts are in the key, performance can range from slow to crashing of the backend. Here is a keyset query using 10 rows (the typical keyset amount):

        -- This is a 3 part key

        select ... from foo where
            (v1 = "?" AND v2 = "?" AND v3 ="?") OR -- line 1
            (v1 = "?" AND v2 = "?" AND v3 ="?") OR -- line 2
        ...
            (v1 = "?" AND v2 = "?" AND v3 ="?") OR -- line 9
            (v1 = "?" AND v2 = "?" AND v3 ="?")    -- line 10
        

The question marks are replaced with the key values

Prior to PostgreSQL 6.4, this was a major problem. But there are at least 2 fixes in place for this as of 6.4. One of the fixes is called KSQO (Keyset Query Optimization). As of 6.4, the driver now turns this on by default, although this can be changed in the Advanced Driver options settings.

### 4.6) How do I get my application to recognize primary keys? {#q4-6}

SQLPrimaryKeys() is implemented in the driver. The driver queries the system tables in search of a unique index named with the using "{table}\_pkey". For Example:

        create table foo (
            id integer primary key,
            data varchar(20)
        );
        

## 5) Borland Applications

### 5.1) What driver/datasource options work well with Borland products? {#q5-1}

Consider setting the following advanced driver options if using Borland:

- Check Text as LongVarchar
- Uncheck Unknowns as LongVarchar
- Parse Statements option: Enable it, if using a protocol earlier than PostgreSQL 6.4
- Unknown Sizes Options: Set to "Longest"

### 5.1) Why do varchar/char datatypes not appear with the correct precision in Borland DBExplorer -OR- why do all varchar/char precisions appear as 128? {#q5-1}

When using the 6.4 protocol, this problem should not be an issue.

Prior to the PostgreSQL 6.4 protocol, the backend did not return the size of varchar/char datatypes in a query result and Borland relies heavily on this for both simple queries and the data dictionary import. Therefore, there are several driver options that were developed to help out with this.

- Parse Statements option -- driver parses the SQL statement and retrieves characteristics such as precision, nullability, aliases, etc. for the columns.
- Unknown Sizes option -- "longest" will return the precision based on the longest data of all the rows in the result set.

Currently, if the parse statements option is enabled, the parser will fallback on executing the statement if it cannot deal with a particular column. Therefore, it is a good idea to set the unknown sizes to "longest" as well.

## 6) Microsoft Applications

### 6.1) Why don't int4 based aggregates seem to work? {#q6-1}

Some Aggregates such as sum(int4) and avg(int4) return results using the numeric datatype. This is perfectly legal according to the SQL spec and is done to prevent overflows and other problems, but unfortunately Microsoft's ActiveX Data Objects don't seem to like it. There are two workarounds:

- Cast the result to an int4 e.g. SELECT avg(fieldname)::int4 FROM tablename
- Set the ADO CursorLocation to AdUseClient. Note: This will produce a read only recordset.

### 6.2) Why does Access force me specify the Data Source each time I run my SQL Pass-Thru query? {#q6-2}

There is a way to specify a Data Source in the query properties so it doesn't ask you each time. Under the view menu, select properties. For the "ODBC Connect Str" property right after "ODBC;" add "DSN=\<your_datasource_name_here\>". You can also add other properties if you like such as "ODBC;DSN=my_dsn;UID=me;PWD=test".

### 6.3) Why does MS Access sometimes complain about a GROUP BY or ORDER BY not being in the target list? {#q6-3}

This message comes from the PostgreSQL backend. PostgreSQL currently requires fields in the ORDER BY and GROUP BY clauses to be included in the target list. However, this restriction has been lifted in PostgreSQL 6.4+

Older versions of the Microsoft Jet Database Engine, that Access is built on, has some problems that can cause this to occur. Access will insist on throwing in an order by clause in a join query, even if you are not sorting on anything. Even with PostgreSQL 6.4+, the query may no longer error out, BUT it would be sorted in a way you may not want, and there would be no way to change it. To fix this problem, you need to update the Jet database engine to version 3.51. It is available at no charge from Microsoft. Click [here to download](http://www.microsoft.com/data) the latest Jet Engine from the Microsoft support site.

### 6.4) Why do I get 'Write Conflict - This record has been changed by another user since you started editing it' in Access 2000 and above? {#q6-4}

**Contributed by Michael Zedeler (michael.zedeler@tirush.dk)**

If you get the following message: "Write Conflict - This record has been changed by another user since you started editing it. \[...\]" from Access 2000, its likely to be a problem that arises from differing standards between Access and PostgreSQL.

From old times, according to some SQL standard, an empty space is by definition equal to NULL. This disposition has caused many grievances over the years, so PostgreSQL rectifies this by deviating from the standard. In PostgreSQL, NULL is NULL and the empty string is the empty string.

So when Access retrieves a row from PostgreSQL with fields containing the empty string, it automatically translates them into NULL values. When you try updating this row, the query sent from Access 2000 to PostgreSQL will fail to update the row. An example:

You have inserted the following row into table a:

            id  | name----+-------------------------  1 | <- contains the empty string. Not NULL.
        

And then you retrieve the row using Access 2000, subsequently inserting the value "Smartypants" in the "name" field.

You would expect Access 2000 to send the following query to PostgreSQL:

` UPDATE a SET name = 'Smartypants' WHERE id = 1 AND name = '' `

(The extra "AND name = ''" is included to avoid updates in case that some other user has already updated the given row.)

But what Access 2000 actually sends is:

` UPDATE a SET name = 'Smartypants' WHERE id = 1 AND name IS NULL `

That query fails because NULL is not being treated as the empty string by PostgreSQL.

**How to fix the problem**

I haven't found any way around it, but to stop using empty strings in character fields.

The problem only arises when other systems inserts data into PostgreSQL tables, so you'll have to make those systems start using NULL in stead of the empty string, where necessary. When Access 2000 inserts data, it will always translate the empty string into NULL, thus maintaining data that are consistent with the (old?) SQL standard.

PostgreSQL 7.2 and above can cause similar problems but for different reasons:

**Contributed by Sam Hokin (sam@ims.net)**

The new PostgreSQL timestamp data type defaults to microsecond precision. This means that timestamp values are stored like 2002-05-22 09:00:00.123456-05. However, Access does not support the extra precision, so the value that Access uses is 2002-05-22 09:00:00-05. When one tries to update a record, one gets the error message above because the value that Access uses in its UPDATE query does not match the value in the PostgreSQL table, similar to the NULL vs. empty string conflict that is already reported in this FAQ entry.

**How to fix the problem**

The simple fix is to use timestamp(0) rather than timestamp. Otherwise, one must make sure that all timestamp values entered into the PostgreSQL table have zero fractional second value.

Incidentally, this problem occurs in Access 97 as well as Access 2000.

**Contributed by Steven Citron-Pousty (Steven.Citron-Pousty@yale.edu)**

Another way around the issue noted here in access2k and 7.2 is to use a query behind your data view that excludes the timestamp columns. In our case the timestamps are autogenerated so we don't even need to see them. So if you don't need to edit the timestamp value hide the column by making a query without the data value.

### 6.5) With MS Access, why can't I index on text fields -OR- why do I get "Invalid field definition 'field'" in definition of index or relationship? {#q6-5}

Text fields are mapped to SQL_LONGVARCHAR by default. As a result MS Access treats these colomns as "Memo" types. The good news is that you can store up to the PostgreSQL block size limit in a text column. PostgreSQL has a tuple limit of just under 8k prior to version 7.1 which includes a new feature called TOAST which allows the storage of much larger strings.

You can change the mapping of Text fields to SQL_VARCHAR by unchecking the Advanced driver option "Text as LongVarchar" under Data Type Options. This should allow text fields to be used but you will be limited to the maximum size of a varchar.

### 6.6) With MS Access, why can't I GROUP BY, ORDER BY, or even select WHERE, on columns which are of type "text"? {#q6-6}

Text fields are mapped to SQL_LONGVARCHAR by default. As a result MS Access treats these colomns as "Memo" types. The good news is that you can store up to the PostgreSQL block size limit in a text column. PostgreSQL has a tuple limit of just under 8k prior to version 7.1 which includes a new feature called TOAST which allows the storage of much larger strings.

You can change the mapping of Text fields to SQL_VARCHAR by unchecking the Advanced driver option "Text as LongVarchar" under Data Type Options. This should allow text fields to be used.

### 6.7) With MS Access, why do I see \#Deleted# in rows after some operations? {#q6-7}

This commonly occurs if you have used numeric or int8 (bigint) columns as your primary key. In Access, an Int is a 16 bit value and a Long Int is a 32 bit value, unlike PostgreSQL in which an int is a 32 bit value and a bigint is 64 bit. Access sees the int8/numeric key as a floating point value, which it cannot use as a key. To resolve the problem, stick to int4 if you need numerical primary keys. For more intofmation, please see [Microsoft KB article \#128809](http://support.microsoft.com/default.aspx?scid=kb;en-us;128809).

### 6.8) Installing psqlODBC on 64bit Windows {#q6-8}

**Contributed by Mark Wood (mw@mcwood.co.uk)**

Though you can install 32 bit ODBC drivers on Win X64 as usual, you can't configure 32-bit DSNs via oridinary control panel or ODBC datasource administrator.

**How to configure 32 bit ODBC drivers on Win x64**

Configure ODBC DSN from **%SystemRoot%\syswow64\odbcad32.exe. ** Click

   Start-\>Run\
   Enter: **%SystemRoot%\syswow64\odbcad32.exe**\
   Hit return\
\
This looks the same as the driver manager you might run from the control panel, but is for managing 32bit drivers. If you run odbcad32.exe from the System32 directory, you are actually running the 64bit driver manager.\
\
**What is WOW64?**\
I would probably remove the rest of the info if possible, and just include a link to more information on WOW64 - maybe\
<http://msdn.microsoft.com/en-us/library/aa384274(v=VS.85).aspx> for example.\
\
**What's and why WOW64**\
<http://forums.techarena.in/operating-systems/1160164.htm>\
\
WoW64 stands for "Windows on 64-bit Windows", and it contains all the 32-bit binary files required for compatibility, which run on top of the\
64 bit Windows. So, yeah" it looks like a double copy of everything in System32 (which despite the directory name, are actually 64-bit binaries).\
\
If you are running 32 bit Windows then finding this SysWoW64 directory is a bit weird ...\
\
WOW64 knows that the Setup wizard is a 32-bit application running within an emulator. It also knows that 64- and 32-bit code cannot be mixed. As such, the WOW64 emulator aliases an alias to the \Windows\SysWOW64 folder. This means that any time a 32-bit application needs to read or write anything to or from the \Windows\System32 folder, the WOW64 emulator transparently redirects the request to the \Windows\SysWOW64 folder.\
\
Microsoft offers a set of 32 bit executables in the WoW(Windows on Windows) directory of the system, for a Windows Vista 64 bit Operating System.\
\
To setup DSN for 32-bit application you must use: **%WINDIR%\SysWOW64\odbcad32.exe**\
\
and for 64-bit application you must use: **%WINDIR%\System32\odbcad32.exe**\
\
So, to run a 32 bit application, we would need to modify the env PATH variable and put SysWOW64 before system32.\
\
Roughly the steps would be\
1. Start **%windir%\SysWoW64\cmd.exe**\
2. set **PATH=%systemroot%\SysWOW64;%PATH%**\
3. Run the application.\
4. After application has completed, reset PATH variable if required.\
\
Hope this was helpful.\
Or, if the application can't be run from a shell then you could modify the PATH variable directly from System Properties \> Advanced Tab \> Environmental Variable \> System variable.\
Edit the PATH variable accordingly and reboot the system for the changes to take effect.\
\
And also check the related threads :\
\
\* Adminpak SP2 , Windows Server x64 SP2\
\<<http://forums.techarena.in/windows-x64-edition/810091.htm>\>\
\* Need Help Registering DLLs\
\<<http://forums.techarena.in/windows-x64-edition/804399.htm>\>\
\* Certificate Services Web interface and Win2k3 x64/AMD64 edition\
\<<http://forums.techarena.in/server-security/598973.htm>\>\
\
Documentation on what you can expect you registry to show after installation using postgresql dbc msi\
\
<http://archives.postgresql.org/pgsql-interfaces/2001-01/msg00177.php>\
\
There are two versions of the ODBC driver at ftp.postgresql.org: one is the full package including installer, the other is just the driver itself.\
Once you have installed with installer once, you can upgrade by just copying new versions of the driver to windows\system.\
If for some reason you cannot get the installer to work, you can do the install by hand with regedit.\
\
Find the key **HKEY_LOCAL_MACHINE\SOFTWARE\ODBC\ODBCINST.INI** and add the following:\
\
REGEDIT4\
\
**\[HKEY_LOCAL_MACHINE\SOFTWARE\ODBC\ODBCINST.INI\PostgreSQL\]\**
"Setup"="C:\\WINDOWS\\SYSTEM\\PSQLODBC.DLL"\
"Driver"="C:\\WINDOWS\\SYSTEM\\PSQLODBC.DLL"\
"APILevel"="1"\
"ConnectFunctions"="YYN"\
"CommLog"="0"\
"Optimizer"="0"\
"DriverODBCVer"="02.00"\
"FileUsage"="0"\
"SQLLevel"="1"\
"UsageCount"=dword:00000001\
"Fetch"="100"\
"UniqueIndex"="1"\
"ReadOnly"="0"\
"UseCursors"="0"\
"UnknownSizes"="0"\
"TextAsLongVarchar"="0"\
"UnknownsAsLongVarchar"="0"\
"MaxVarcharSize"="254"\
"MaxLongVarcharSize"="4094"\
"ConnSettings"=""\
"UseDeclareFetch"="0"\
"BoolsAsChar"="0"\
"ExtraSysTablePrefixes"="dd\_;"\
"Parse"="1"\
"Ksqo"="1"\
"CancelAsFreeStmt"="0"**\**
\
(Note: check the above paths for your system)\
\
Find the key **HKEY_LOCAL_MACHINE\SOFTWARE\ODBC\ODBCINST.INI\ODBC** Drivers and add an entry "**PostgreSQL= Installed**".\
\
From the Desk of Mark Wood (mw@mcwood.co.uk ) 20100524
