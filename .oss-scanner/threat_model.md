# Threat model

## What this project does and where untrusted input enters

psqlODBC is the ODBC driver for PostgreSQL. Applications load it as a shared library (`psqlodbcw.so`, `psqlodbca.so`, or the Windows DLLs) through an ODBC driver manager, and it talks to the server through libpq. A bug in the driver runs in the application's process, with the application's privileges.

Untrusted input enters from these sources, all in scope:

- **The server.** Result sets, column names and types, row and column counts, error and notice fields, and large-object replies. Treat every byte the server sends as hostile: the server may be malicious or compromised, and the default `sslmode` is `disable`, so anyone on the network path can rewrite the traffic unless the connection uses `sslmode=verify-full`.
- **Other database users.** Anyone who can create a table, view, column, function, or schema can choose names and values that the driver later reads back, for example a view with its own column named `ctid`, or a column name containing `"` or `%`.
- **The application's arguments.** SQL text, including ODBC escape sequences (`{fn ...}`, `{d ...}`, `{ts ...}`, `{escape ...}`), table and schema names passed to the catalog functions (`SQLTables`, `SQLColumns`, `SQLSpecialColumns`, `SQLStatistics`, and the rest), column numbers, row numbers, descriptor fields, and buffer lengths. Many applications pass end-user input straight through, so the driver must not turn a bad argument into memory corruption or SQL injection.
- **Connection strings and DSN settings.** Applications often build connection strings from user input. Escaping rules are in `docs/config.html`.
- **The local filesystem.** The driver writes log files (`Debug`, `CommLog`) and reads `odbc.ini` and `odbcinst.ini`. Other local users are untrusted.

## Components that matter most / least

Most important:

- Result handling: `qresult.c`, `results.c`, `connection.c` (`CC_send_query_append`, `CC_send_function`), `columninfo.c`.
- Data conversion: `convert.c` (application SQL rewriting, escape sequences, parameter binding, type conversion) and `bind.c`.
- Catalog functions: `info.c`.
- Updatable cursors and keysets: `parse.c`, `results.c` (`SQLSetPos`, `SQLBulkOperations`, positioned updates and deletes).
- Descriptors and statement attributes: `descriptor.c`, `pgapi30.c`, `options.c`.
- Unicode entry points: `odbcapi*w.c`, `win_unicode.c`.
- Logging: `mylog.c`.

Lower priority:

- Windows-only code that this image cannot build: the setup dialogs (`dlg_wingui.c`, `setup.c`), MSDTC support (`msdtc_enlist.cpp`, `pgxalib.cpp`), and the installer (`installer/`, `winbuild/`). Source review of these is still welcome.

Out of scope:

- `test/` is test code, and `docs/` is documentation.
- Bugs in libpq, unixODBC, or the PostgreSQL server belong to those projects.

## How to exercise it

The image builds both drivers into `/src/.libs/` and the regression tests into `/src/test/exe/`. The tests need a running server:

```sh
pg_ctlcluster "$(pg_lsclusters -h | awk 'NR==1{print $1}')" main start
cd /src/test
make installcheck              # full suite
./runsuite keyset-overflow     # one test; programs are in exe/, expected output in expected/
```

`make installcheck` creates `test/odbc.ini` and `test/odbcinst.ini`, which point at the drivers in `../.libs`. The test DSN is `psqlodbc_test_dsn` (Unicode driver) and `psqlodbc_test_dsn_ansi` (ANSI driver), connecting as `root` over the Unix socket to the `contrib_regression` database. `COMMON_CONNECTION_STRING_FOR_REGRESSION_TEST` adds settings to every test connection, for example `UseDeclareFetch=1;Fetch=1` or `UseServerSidePrepare=0`.

To act as a hostile server, use a small relay in front of the local server that rewrites its replies, or reach the code path from SQL. For example, `CREATE VIEW v AS SELECT 1 AS id, repeat('x', 200) AS ctid` makes the driver's appended `ctid` column return an oversized value.

A reproducer is most useful as a new program in `test/src/` with its expected output in `test/expected/`, added to `test/tests`, in the style of the existing tests.

## How you rate severity

We score with CVSS 3.1 and use these levels:

- **High:** memory corruption (out-of-bounds write, use after free, double free, format string) reachable from the server, from another database user, or from application arguments that applications commonly take from users. SQL injection through driver-built queries is also High. An overflow without a demonstrated path to code execution is capped at High.
- **Critical:** only with a demonstrated, reliable path to code execution from a position that needs no access.
- **Medium:** crashes and out-of-bounds reads that only cause denial of service, and disclosure of query text or data through log files.
- **Low:** defects that only an application can reach by calling a driver export in a way no driver manager does, or that need a misconfiguration.

Being reachable only from a hostile server, or from a man-in-the-middle when `sslmode` is not `verify-full`, does not lower a finding below these levels. It sets attack complexity to high.

## Anything to leave alone

- An application misusing the ODBC API in ways the specification leaves undefined, when the result only affects that application and no untrusted input is involved. A buffer length the application states wrongly is an example.
- Behaviour that needs control of the application's own process or of the driver's installation directory.
- Findings in third-party code that the driver links against (libpq, unixODBC, OpenSSL).
