---
title: "psqlODBC Configuration Options"
weight: 1
aliases:
  - /docs/config.html
---

## Advanced Options 1/3 Dialog Box

- **DEFAULTS:** Press to this button restore the normal defaults for the settings described below.
- **Recognize Unique Indexes:** Check this option.
- **Use Declare/Fetch:** If true, the driver automatically uses declare cursor/fetch to handle SELECT statements and keeps 100 rows in a cache. This is mostly a great advantage, especially if you are only interested in reading and not updating. It results in the driver not sucking down lots of memory to buffer the entire result set. If set to false, cursors will not be used and the driver will retrieve the entire result set. For very large tables, this is very inefficient and may use up all the Windows memory/resources. However, it may handle updates better since the tables are not kept open, as they are when using cursors. This was the style of the old podbc32 driver. However, the behavior of the memory allocation is much improved so even when not using cursors, performance should at least be better than the old podbc32.
- **CommLog (C:\psqlodbc_xxxx.log):** Log communications to/from the backend to that file. This is good for debugging problems.
- **Parse Statements:** Tell the driver how to gather the information about result columns of queries, if the application requests that information before executing the query. See also *ServerSide Prepare* options.\
  The driver checks this option first. If disabled then it checks the Server Side Prepare option.\
  \
  If this option is enabled, the driver will parse an SQL query statement to identify the columns and tables and gather statistics about them such as precision, nullability, aliases, etc. It then reports this information in SQLDescribeCol, SQLColAttributes, and SQLNumResultCols.\
  \
  When this option is disabled (the default), the query is sent to the server to be parsed and described. If the parser can not deal with a column (because it is a function or expression, etc.), it will fall back to describing the statement in the server. The parser is fairly sophisticated and can handle many things such as column and table aliases, quoted identifiers, literals, joins, cross-products, etc. It can correctly identify a function or expression column, regardless of the complexity, but it does not attempt to determine the data type or precision of these columns.
- **Ignore Timeout:** Ignore SQL_ATTR_QUERY_TIMEOUT set using SQLSetStmtAttr(). Some tools issue SQLSetStmtAttr(.., SQL_ATTR_QUERY_TIMEOUT, ...) internally and sometimes it's difficult for users to change the value.
- **MyLog (C:\mylog_xxxx.log):** Log debug messages to that file. This is good for debugging problems with the driver.
- **Unknown Sizes:** This controls what SQLDescribeCol and SQLColAttributes will return as to precision for character data types (varchar, text, and unknown) in a result set when the precision is unknown. **This was more of a workaround for pre-6.4 versions of PostgreSQL not being able to return the defined column width of the varchar data type**.\
  \
  - *Maximum*: Always return the maximum precision of the data type.
  - *Dont Know*: Return "Don't Know" value and let application decide.
  - *Longest*: Return the longest string length of the column of any row. Beware of this setting when using cursors because the cache size may not be a good representation of the longest column in the cache.
    \
    *MS Access*: Seems to handle *Maximum* setting ok, as well as all the others.\
    *Borland*: If sizes are large and lots of columns, Borland may crash badly (it doesn't seem to handle memory allocation well) if using *Maximum* size.

  \
- **Data Type Options:** affects how some data types are mapped:
  - *Text as LongVarChar*: PostgreSQL TEXT type is mapped to SQLLongVarchar, otherwise SQLVarchar.
  - *Unknowns as LongVarChar*: Unknown types (arrays, etc) are mapped to SQLLongVarChar, otherwise SQLVarchar
  - *Bools as Char*: Bools are mapped to SQL_CHAR, otherwise to SQL_BIT.

  \
- **Max Varchar** The maximum precision of the Varchar and BPChar(char\[x\]) types. The default is 254 which actually means 255 because of the null terminator. Note, if you set this value higher than 254, Access will not let you index on varchar columns!
- **Cache Size:**When using cursors, this is the row size of the tuple cache and the default is 100 rows. If not using cursors, this has no meaning.
- **Max LongVarChar:** The maximum precision of the LongVarChar type. The default is 4094 which actually means 4095 with the null terminator. You can even specify (-4) for this size, which is the odbc SQL_NO_TOTAL value.
- **SysTable Prefixes:** Additional prefixes of table names to regard as System Tables. Tables that begin with "pg\_" are always treated as system tables, even without this option. Separate each prefix with a semicolon (;)
- **Batch Size:**Chunk size when executing batches with arrays of parameters. Setting 1 to this option forces one by one execution (the behavior before).

## Advanced Options 2/3 Dialog Box

- **ReadOnly:** Whether the datasource will allow updates.
- **Show System Tables:** The driver will treat system tables as regular tables in SQLTables. This is good for Access so you can see system tables.
- **LF \<-\> CR/LF conversion:** Convert Unix style line endings to DOS style.
- **Updateable Cursors:** Enable updateable cursor emulation in the driver.
- **Bytea as LO:** Allow the use of bytea columns for Large Objects.
- **Row Versioning:** Allows applications to detect whether data has been modified by other users while you are attempting to update a row. It also speeds the update process since every single column does not need to be specified in the where clause to update a row. The driver uses the "xmin" system field of PostgreSQL to allow for row versioning. Microsoft products seem to use this option well. See the [faq](/faq/) for details on what you need to do to your database to allow for the row versioning feature to be used.
- **Display Optional Error Message:** Display optional(detail, hint, statement position etc) error messages.
- **True is -1:** Represent TRUE as -1 for compatibility with some applications.
- **Server side prepare:**If set, the driver uses server-side prepared statements. See also *Parse Statement* option. Note that if a query needs to be described before execution, e.g. because the application calls SQLDescribeCol() or SQLNumResultCols() before SQLExecute(), the driver will send a Parse request to the server even if this option is disabled. In that case, the query that is sent to the server for parsing will have the parameter markers replaced with the actual parameter values, or NULL literals if the values are not known yet.
- **Int8 As:** Define what datatype to report int8 columns as.
- **Numeric As:** Specify the map from numeric items without precision to SQL data types. numeric(default), varchar, double or memo(SQL_LONGVARCHAR) can be specified.
- **Extra Opts:** combination of the following bits.\
  \
    0x1: Force the output of short-length formatted connection string. Check this bit when you use MFC CDatabase class.\
    0x2: Fake MS SQL Server so that MS Access recognizes PostgreSQL's serial type as AutoNumber type.\
    0x4: Reply ANSI (not Unicode) char types for the inquiries from applications. Try to check this bit when your applications don't seem to be good at handling Unicode data.
- **Level of rollback on errors:** Specifies what to rollback should an error occur.
  - *Nop(0):* Don't rollback anything and let the application handle the error.
  - *Transaction(1):* Rollback the entire transaction.
  - *Statement(2):* Rollback the statement.
    \
    **Setup note: This specification is set up with the PROTOCOL option parameter.**\
    \
    PROTOCOL=7.4-(0\|1\|2)\
    default value is Statement (it is Transaction for servers before 8.0).\
    \
- **OID Options:**
  - *Show Column:* Includes the OID in SQLColumns. This is good for using as a unique identifier to update records if no good key exists OR if the key has many parts, which blows up the backend.
  - *Fake Index:* This option fakes a unique index on OID. This is useful when there is not a real unique index on OID and for apps which can't ask what the unique identifier should be (i.e, Access 2.0).
- **Connect Settings:** The driver sends these commands to the backend upon a successful connection.  It sends these settings AFTER it sends the driver "Connect Settings". Use a semi-colon (;) to separate commands. This can now handle any query, even if it returns results. The results will be thrown away however!
- **TCP KEEPALIVE setting(by sec):** Specifies TCP keepalives setting.
  - *disable:* Check when client-side TCP keepalives are not used.
  - *idle time:* The number of seconds of inactivity after which TCP should send a keepalive message to the server.
  - *interval:* The number of seconds after which a TCP keepalive message that is not acknowledged by the server should be retransmitted.

  

## Advanced Options 3/3 Dialog Box

- **Allow connections urecoverable by MSDTC?:** How to test distributed transactions.
  - *yes:* MSDTC is needless unless applications crash. So don't check the connectivity from MSDTC.
  - *rejects sslmode verify-\[ca\|full\]:* reject ssl connections with verify-ca or verify-full mode because in those cases msdtc could hardly establish the connection.
  - *no:* First confirm the connectivity from MSDTC.

  \
- **Libpq parameters:** Specify libpq connection parameters with conninfo style strings e.g. **sslrootcert=c:\\myfolder\\myroot sslcert=C:\\myfolder\\mycert sslkey=C:\\myfolder\\mykey**.\
  Though host, port, dbname, user, password, sslmode, keepalives_idle or keepalive_interval parameters can be set using this(pqopt) option, the use is not recommended because they are ordinarily set by other options. When some settings for those parameters conflict with other ordinary options, connections are rejected.

## Global settings Dialog Box

This dialog allows you to specify pre-connection/default logging options

- **CommLog (C:\psqlodbc_xxxx.log - Communications log):** Log communications to/from the backend to that file. This is good for debugging problems.
- **MyLog (C:\mylog_xxxx.log - Detailed debug output):** Log debug messages to that file. This is good for debugging problems with the driver.
- **MSDTCLog (C:\pgdtclog\mylog_xxxx.log - MSDTC debug output):** Log debug messages to that file. This is good for debugging problems with the MSDTC.
- **Specification of the holder for log outputs:** Adjustment of write permission.

## Manage DSN Dialog Box

This dialog allows you to select which PostgreSQL ODBC driver to use for this connection. Note that this may not work with third party drivers.

## How to specify as a connection option

There is a method of specifying a connection option in a keyword strings.

Example:VBA

- **myConn = "ODBC;DRIVER={PostgreSQL Unicode};" & serverConn & \_\
  "A0=0;A1=7.4;A2=0;A3=0;A4=0;A5=0;A6=;A7=100;A8=4096;A9=0;" & \_\
  "B0=254;B1=8190;B2=0;B3=0;B4=1;B5=1;B6=0;B7=1;B8=0;B9=1;BI=-5;" & \_\
  "C0=0;C2=dd\_;C4=1;C5=1;C6=1;C7=1;C8=1;C9=0;CA=verify-full;D1=30;D4=40;" & \_\
  "D5={sslrootcert=C:\\myfolder\\myroot sslcert=C:\\myfolder\\mycert sslkey=C:\\myfolder\\mykey}"**

Please refer to a [keyword list](/docs/connection-options/) for details.

## Special characters and escaping in connection strings

A value that contains characters with a special meaning (`;`, `=`, spaces or braces) must be escaped. It is important to understand that a connection string is processed by **two independent parsers**, each with its own escaping rules.

### 1. The psqlODBC connection string

The driver splits the keyword string on `;` (the attribute delimiter) and on the first `=` of each attribute. To include one of these characters, or a space, in a value, use one of the following:

- **Braces:** enclose the whole value in `{ }`. A braced value may contain `;`, `=` and spaces. For example **PWD={my;odd=pass word}**. To include a literal closing brace `}` inside a braced value, double it as `}}`.
- **Percent-encoding:** outside of braces the value is URL-decoded, so a space can be written as `+` or `%20`, and an arbitrary byte as `%`*`XX`* (two hex digits). For example **PWD=pass+word** or **PWD=pass%20word**.

A backslash (`\`) has **no special meaning** in the psqlODBC connection string itself; it is passed through literally.

### 2. The Libpq parameters (pqopt) value

The **Libpq parameters** option (see Advanced Options 3/3 above) is passed on to libpq and parsed by it as a [conninfo string](https://www.postgresql.org/docs/current/libpq-connect.html#LIBPQ-CONNSTRING), which has its own, different rules. Within this value libpq separates parameters by spaces, so to include a space (or other special character) in a libpq parameter value you use libpq's escaping:

- **Backslash:** `\` escapes the following character, e.g. **sslcert=C:\\ my\\ folder\\ cert**.
- **Single quotes:** enclose the value in `' '`, e.g. **sslcert='C:\my folder\cert'**. A literal single quote or backslash inside quotes is written as `\'` and `\\`.

This is why, in the VBA example above, the libpq paths inside the `D5={...}` braces use backslashes: the braces are consumed by the psqlODBC parser (layer 1), and the contents are then handed to libpq (layer 2). The two layers can therefore both apply to the same value.
