# Changelog

Release notes for psqlODBC, converted from docs/release.html and docs/release-7.3.html.

## 18.00.0006

This release fixes the following vulnerabilities. All releases before 18.00.0006 are affected unless an entry gives a narrower range.

- Large-object read overflows the application buffer (CVE-2026-120057, [GHSA-qhmr-5339-q33r](https://github.com/postgresql-interfaces/psqlodbc/security/advisories/GHSA-qhmr-5339-q33r), CVSS 3.1 7.5 High)
- Row cache overflow when a later FETCH changes the column count (CVE-2026-120058, [GHSA-6rmx-9q62-4w6m](https://github.com/postgresql-interfaces/psqlodbc/security/advisories/GHSA-6rmx-9q62-4w6m), CVSS 3.1 7.5 High)
- Driver uses a server-supplied column name as a format string (CVE-2026-120059, [GHSA-44rq-72jh-x2h8](https://github.com/postgresql-interfaces/psqlodbc/security/advisories/GHSA-44rq-72jh-x2h8), CVSS 3.1 6.8 Medium)
- Driver does not validate server-supplied counts and lengths (CVE-2026-120060, [GHSA-4mfx-cfxx-c877](https://github.com/postgresql-interfaces/psqlodbc/security/advisories/GHSA-4mfx-cfxx-c877), CVSS 3.1 5.9 Medium)
- ODBC escape parser stack buffer overflow (CVE-2026-120061, [GHSA-hfm6-qfx8-v4xq](https://github.com/postgresql-interfaces/psqlodbc/security/advisories/GHSA-hfm6-qfx8-v4xq), CVSS 3.1 7.5 High)
- SQL injection through SQLSpecialColumns table and schema names. Affects 17.00.0006 through 18.00.0005 (CVE-2026-120062, [GHSA-gg8f-fx3w-rj33](https://github.com/postgresql-interfaces/psqlodbc/security/advisories/GHSA-gg8f-fx3w-rj33), CVSS 3.1 7.5 High)
- Stack buffer overflow when quoting long schema and table names (CVE-2026-120063, [GHSA-285r-84w3-xcxv](https://github.com/postgresql-interfaces/psqlodbc/security/advisories/GHSA-285r-84w3-xcxv), CVSS 3.1 5.9 Medium)
- Driver does not validate column numbers and row indexes in binding and descriptor functions (CVE-2026-120064, [GHSA-7pq2-q7mj-ph8c](https://github.com/postgresql-interfaces/psqlodbc/security/advisories/GHSA-7pq2-q7mj-ph8c), CVSS 3.1 7.5 High)
- Driver does not escape a server-supplied column name in its updatable-cursor queries (CVE-2026-120065, [GHSA-9cg8-r57h-h6jj](https://github.com/postgresql-interfaces/psqlodbc/security/advisories/GHSA-9cg8-r57h-h6jj), CVSS 3.1 7.1 High)
- Log files are created with default permissions at a predictable path and follow symbolic links (CVE-2026-120066, [GHSA-wj7r-6pj3-62g8](https://github.com/postgresql-interfaces/psqlodbc/security/advisories/GHSA-wj7r-6pj3-62g8), CVSS 3.1 5.3 Medium)
- Stack buffer overflow when reading the keyset (ctid/oid) columns of an updatable cursor (CVE-2026-120067, [GHSA-3r96-h2q4-6pxh](https://github.com/postgresql-interfaces/psqlodbc/security/advisories/GHSA-3r96-h2q4-6pxh), CVSS 3.1 5.9 Medium)

The release also contains these hardening changes. They have no demonstrated attack path and no CVE.

- SQLSetDescRecW stored a freed buffer and the address of a local variable as deferred bound data. ODBC defines no Unicode form of SQLSetDescRec, and driver managers call only SQLSetDescRec, so only an application that calls the driver's SQLSetDescRecW export directly reaches it. It now stores the application's pointers, as SQLSetDescRec does.
- SQLColumns returned uninitialized relkind and table OID values when the table query returned no rows.
- convert_linefeeds could dereference a NULL pointer in one build configuration.
- secure_sscanf limits an over-wide %Ns width, msgtowstr checks for a zero buffer length, and the MSDTC connect-string append is bounded.

## 18.00.0005

- Fix heap memory corruption when an application binds more than 32767 parameters on one statement: SQLBindParameter now rejects parameter numbers outside 1..32767 with SQLSTATE 07009 (GHSA-cmfx-rm3r-p2jr)
- Fix the placeholder cast being truncated from parameter 1000 on, which failed with type "timestam" does not exist (#221)
- Report the largest column size instead of SQL_NO_TOTAL (-4) as COLUMN_SIZE for bytea in SQLGetTypeInfo, which broke bytea parameters through MSDASQL (#215, #218)
- Build Windows OpenSSL 3.5.9 in CI (#219)

## 18.00.0004

- Fix the result-conversions regression test on big-endian systems (#213)

## 18.00.0003

- Report SQLSTATE 22003 for out-of-range integer conversions instead of silently returning the wrapped low bits (#207, #210, #211)
- Fix statement-level rollback (Protocol=7.4-2) discarding the whole transaction on a syntax error (#203, #204)
- Don't wrap internal queries in a savepoint at rollback-on-error level 0 (Protocol=7.4-0), which lost transactions on servers without SAVEPOINT support (#208, #209)
- Redact the database password (ODBC PWD and libpq pqopt password) in debug logs (#206)
- Fix hang on COPY ... FROM STDIN / TO STDOUT: report it as unsupported and keep the connection usable (#202)
- Fix 'infinity'/'-infinity' dates silently returned as today's date (#199)
- Document connection string escaping rules (#140, #197)
- Resolve Visual Studio 2026 (v18) build issues and circular dependencies (#201)
- Add Windows on Arm (ARM64) CI build and regression testing (#196)
- Build Windows OpenSSL 3.5.8 in CI (#205)

## 18.00.0002

- Use signed type for error status (#193)
- Redact sensitive connection parameters in driver logs
- Fix double-free / heap corruption during SQLDisconnect cleanup (#189, #190)
- Clear PGresults from per-query rollback (#188)
- Validate UTF-16 surrogate pairs before combining (#187)
- Pass SQLSetDescRec field values correctly (#186)
- Avoid reading before leading string literals (#185)
- Detach freed app descriptors from statements (#183)
- Reject invalid SQLPutData lengths (#182)
- Reject overlong cursor names (#178)
- Fix version reporting (#181)
- Handle ARD bookmark allocation failure (#179)
- Validate percent escapes before decoding (#175)
- Avoid reading past conn_settings end (#176)
- Fix stack buffer overflow in getPrecisionPart() (#173, #174)
- Fix SQLPrimaryKeys to exclude INCLUDE columns from results (#171)
- Use openssl 3.5.5 (#169)

## 18.00.0001

- No code changes from 18.00.0000

## 17.00.0010

- No code changes from 17.00.0009

## 18.00.0000

- No changes from 17.00.0009

## 17.00.0009

- Enhancement: Fix tests for PostgreSQL version 19 (standard_conforming_strings always on) [PR #162](https://github.com/postgresql/psqlodbc/pull/162) Author: [Dave Cramer](https://github.com/davecramer)
- CI: Upgrade GitHub Actions workflow - use prebuilt PostgreSQL binaries, parallel matrix builds, separate release job [PR #157](https://github.com/postgresql/psqlodbc/pull/157) Author: [F.D.Castel](https://github.com/fdcastel)

## 17.00.0008

- Security: Update OpenSSL to version 3.3.6 (CVE-2025-15467) [issue #158](https://github.com/postgresql/psqlodbc/issues/158) Author: [Dave Cramer](https://github.com/davecramer)
- Fix: Fixes for issue #153 [PR #155](https://github.com/postgresql/psqlodbc/pull/155) Author: [F.D.Castel](https://github.com/fdcastel)
- Fix: Bug causing out-of-bounds memory access in AddUpdated when updating cached row status [PR #151](https://github.com/postgresql/psqlodbc/pull/151) Author: [Yashwanth Chandolu](https://github.com/yashwanthchandolu)
- Fix: Check for errors in convert_to_pgbinary instead of ignoring return value of -1 from pg_bin2hex [PR #150](https://github.com/postgresql/psqlodbc/pull/150) Author: [Dave Cramer](https://github.com/davecramer)

## 17.00.0007

- Fix: GetDescRec on big-endian architectures [PR #130](https://github.com/postgresql/psqlodbc/pull/130) Author: [Christoph Berg](https://github.com/df7cb)
- Refactor: use operator(pg_catalog.operator) to fully qualify the operator [PR #139](https://github.com/postgresql/psqlodbc/pull/139) Author: [Dave Cramer](https://github.com/davecramer)
- Fix: Make sure the null terminator len_for_wcs_term is included in the decision to realloc [PR #145](https://github.com/postgresql/psqlodbc/pull/144) Author: [Mark Maker](https://github.com/markmaker)
- Refactor: check the datestyle before setting it, if it is different than ISO change it, otherwise leave it as is [PR #144](https://github.com/postgresql/psqlodbc/pull/144) Author: [Dave Cramer](https://github.com/davecramer)

## 17.00.0006

- Refactor: Comprehensive variable naming improvements across the codebase [PR #125](https://github.com/postgresql/psqlodbc/pull/125) Author: [Dave Cramer](https://github.com/davecramer)
- Enhancement: Renamed ConnectionClass structure variables for better readability (e.g., pqconn → postgresConnection, status → connectionStatus) Author: [Dave Cramer](https://github.com/davecramer)
- Enhancement: Improved StatementClass structure variable names for clarity (e.g., hdbc → connectionHandle, curres → currentResult) Author: [Dave Cramer](https://github.com/davecramer)
- Enhancement: Updated QResultClass structure variables with more descriptive names (e.g., rstatus → resultStatus, num_fields → columnCount) Author: [Dave Cramer](https://github.com/davecramer)
- Documentation: Updated inline comments to reflect new variable naming conventions Author: [Dave Cramer](https://github.com/davecramer)
- Maintenance: Removed unnecessary prefixes from variable names (e.g., __error_message → errorMessage) Author: [Dave Cramer](https://github.com/davecramer)
- Enhancement: Replaced abbreviations with full words for better code readability Author: [Dave Cramer](https://github.com/davecramer)
- Maintenance: Updated all references to renamed variables throughout the codebase Author: [Dave Cramer](https://github.com/davecramer)
- Documentation: Added comprehensive variable mapping documentation for future reference Author: [Dave Cramer](https://github.com/davecramer)

## 17.00.0005

- Security: fix possible int overflow [PR #121](https://github.com/postgresql/psqlodbc/pull/121) fixes [issue #119](https://github.com/postgresql/psqlodbc/issues/119) Author: [Dave Cramer](https://github.com/davecramer)
- Fix: Use memset if memset_s is not available [PR #107](https://github.com/postgresql/psqlodbc/pull/107) Author: [Favian (Ian) Samatha](https://github.com/faviansamatha)
- Feature: Safe sscanf implementation [PR #108](https://github.com/postgresql/psqlodbc/pull/108) Author: [Colin](https://github.com/colin)
- Fix: Don't assume output connection string has size MAX_CONNECT_STRING [PR #106](https://github.com/postgresql/psqlodbc/pull/106) Author: [Micah Baker](https://github.com/micahbaker)
- Security: Change memset to memset_s/SecureZeroMemory [PR #105](https://github.com/postgresql/psqlodbc/pull/105) Author: [Favian (Ian) Samatha](https://github.com/faviansamatha)
- Security: Replace unsafe string-to-number conversions [PR #103](https://github.com/postgresql/psqlodbc/pull/103) Author: [Favian (Ian) Samatha](https://github.com/faviansamatha)
- Build: Update main.yml to use wix version 6.0.0 [PR #104](https://github.com/postgresql/psqlodbc/pull/104) Author: [Dave Cramer](https://github.com/davecramer)
- Fix: Fix possible use of uninitialized memory (Issue #99) [PR #100](https://github.com/postgresql/psqlodbc/pull/100) Author: [Dave Cramer](https://github.com/davecramer)
- Fix: Change SpecialColumns to return correct information for unique indexes [PR #97](https://github.com/postgresql/psqlodbc/pull/97) Author: [Dave Cramer](https://github.com/davecramer)
- UI: Update the font and layouts of all dialogs to improve their look and feel [PR #91](https://github.com/postgresql/psqlodbc/pull/91) Author: [Keith Wedinger](https://github.com/keithw)
- Security: Update OPENSSL to 3.3.3 Author: [Dave Cramer](https://github.com/davecramer)
- Test: Test with just unique keys [PR #87](https://github.com/postgresql/psqlodbc/pull/87) Author: [Dave Cramer](https://github.com/davecramer)
- Fix: Fix duplicate component codes [PR #75](https://github.com/postgresql/psqlodbc/pull/75) Author: [Christian Ullrich](https://github.com/christianu)
- Documentation: Add new test table for SQLSpecialColumns [PR #85](https://github.com/postgresql/psqlodbc/pull/85) Author: [Dave Cramer](https://github.com/davecramer)
- Fix: Fix spelling of 'statement_deferred' [PR #86](https://github.com/postgresql/psqlodbc/pull/86) Author: [Dave Cramer](https://github.com/davecramer)
- Documentation: Fix release notes to properly reflect changes [PR #81](https://github.com/postgresql/psqlodbc/pull/81) Author: [Dave Cramer](https://github.com/davecramer)

/

## 17.00.0004

- Fix: Install the drivers with short file names [PR #76](https://github.com/postgresql-interfaces/psqlodbc/pull/76) Author: [Christian Ullrich](https://github.com/)

## 17.00.0003

- Chore: Remove the NMAKE build system for installers [PR #65](https://github.com/postgresql-interfaces/psqlodbc/pull/65) Author: [Christian Ullrich](https://github.com/)
- Use WiX 5 by @chrullrich [PR #66](https://github.com/postgresql-interfaces/psqlodbc/pull/66) Author: [Christian Ullrich](https://github.com/)
- Fix:fix small memory leak in 'CC_initial_log' function by @iobnc [PR #67](https://github.com/postgresql-interfaces/psqlodbc/pull/67) Author: [iobnc](https://github.com/iobnc)
- Fix typo failing the Rebuild target by @chrullrich [PR #68](https://github.com/postgresql-interfaces/psqlodbc/pull/68) Author: [Christian Ullrich](https://github.com/)
- Correct wrong string method calls by @chrullrich [PR #69](https://github.com/postgresql-interfaces/psqlodbc/pull/69) Author: [Christian Ullrich](https://github.com/)
- Fix PG_ErrorInfo and ER_ReturnError when error message is larger than 65535 bytes [PR #70](https://github.com/postgresql-interfaces/psqlodbc/pull/70) Author: [r33s3n6](https://github.com/r33s3n6)
- Release: Update version numbers [PR #77](https://github.com/postgresql-interfaces/psqlodbc/pull/77) Author: [Dave Cramer](https://github.com/davecramer)

Full Changelog: REL-17_00_0002...REL-17_00_0003 (https://github.com/postgresql-interfaces/psqlodbc/compare/REL-17_00_0002...REL-17_00_0003)

## 17.00.0002

- Fix: Fix MSI error messages by in [PR #58](https://github.com/postgresql-interfaces/psqlodbc/pull/58) Author: [Christian Ullrich](https://github.com/)
- Release: Update version numbers [PR #52](https://github.com/postgresql-interfaces/psqlodbc/pull/52) Author: [Dave Cramer](https://github.com/davecramer)

Full Changelog: REL-17_00_0001...REL-17_00_0002 (https://github.com/postgresql-interfaces/psqlodbc/compare/REL-17_00_0001...REL-17_00_0002)

## 17.00.0001

- Release: Use REL_17_STABLE by @davecramer in [PR #53](https://github.com/postgresql-interfaces/psqlodbc/pull/53) Author: [Dave Cramer](https://github.com/davecramer)
- fix:Another place where refcnt in the COL_INFO object is unfortunately erased. [PR #55](https://github.com/postgresql-interfaces/psqlodbc/pull/55) Author: [Alexandr Kuznetsov](https://github.com/progmachine)

Full Changelog: REL-17_00_0000...REL-17_00_0001 (https://github.com/postgresql-interfaces/psqlodbc/compare/REL-17_00_0000...REL-17_00_0001)

## 17.00.0000

- Release: Update version numbers [PR #52](https://github.com/postgresql-interfaces/psqlodbc/pull/52) Author: [Dave Cramer](https://github.com/davecramer)

Full Changelog: REL-17_00_0000...REL-16_00_0006 (https://github.com/postgresql-interfaces/psqlodbc/compare/REL-17_00_0000...REL-16_00_0006)

## 16.00.0006

- Change: Build against PostgreSQL 17 RC1 [PR #44](https://github.com/postgresql-interfaces/psqlodbc/pull/44) Author: [Dave Cramer](https://github.com/davecramer)
- Fix: Double free of COL_INFO object [PR #48](https://github.com/postgresql-interfaces/psqlodbc/pull/48) Fixes [Issue #47](https://github.com/postgresql-interfaces/psqlodbc/issues/47) Author: [Alexandr Kuznetsov](https://github.com/progmachine)

Full Changelog: REL-16_00_0006...REL-16_00_0005 (https://github.com/postgresql-interfaces/psqlodbc/compare/REL-16_00_0006...REL-16_00_0005)

## 16.00.0005

- Fix: if the connection is broken when calling SQLFreeHandle, the client can't be notified [PR #1](https://github.com/postgresql-interfaces/psqlodbc/pull/1) Author: [Adrian Grucza](https://github.com/apgrucza)
- Perf: Use mimalloc to improve performance and reduce memory allocation lock contention [PR#6](https://github.com/postgresql-interfaces/psqlodbc/pull/6)
- Add: Build and release using github actions [PR#7](https://github.com/postgresql-interfaces/psqlodbc/pull/7)
- Add: Support building against VC17 on AMD64 [PR#9](https://github.com/postgresql-interfaces/psqlodbc/pull/9) Author: [Adrian Grucza](https://github.com/apgrucza)
- Fix: Fixed typos in all source code and documentations [PR#11](https://github.com/postgresql-interfaces/psqlodbc/pull/11) Author [Hunaid Sohail](https://github.com/Hunaid2000)
- Fix: during call SQLFreeHandle, if the connection is broken the client is not notified PR #12 Author: [Adrian Grucza](https://github.com/apgrucza)
- Fix: Key caches based on software versions PR #13\< Author: [Adrian Grucza](https://github.com/apgrucza)
- Add: Add mimalloc release PR #14 Author: [Adrian Grucza](https://github.com/apgrucza)
- Fix: Use strdup on non-Windows systems PR #17 Author: [Adrian Grucza](https://github.com/apgrucza)
- Add: Implementation of SQLSetDescRec & SQLGetDescRec functions [PR#23](https://github.com/postgresql-interfaces/psqlodbc/pull/23) Author [Hunaid Sohail](https://github.com/Hunaid2000)
- Fix: Fix Memory leaks [Issue #8](https://github.com/postgresql-interfaces/psqlodbc/issues/8) [PR#25](https://github.com/postgresql-interfaces/psqlodbc/pull/25) Author: [Alexandr Kuznetsov](https://github.com/progmachine)
- Fix: SQLGetData function can return an incorrect SQLSTATE [Issue #33](https://github.com/postgresql-interfaces/psqlodbc/issues/33) [PR #35](https://github.com/postgresql-interfaces/psqlodbc/pull/35) Author: Carlos García
- Change: Build against PostgreSQL 17 Beta3 [PR #37](https://github.com/postgresql-interfaces/psqlodbc/pull/37) Author: [Dave Cramer](https://github.com/davecramer)

Full Changelog: REL-16_00_0005...REL-16_00_0000 (https://github.com/postgresql-interfaces/psqlodbc/compare/REL-16_00_0005...REL-16_00_0000)

## 16.00.0000

- Use autoconf to check for stdbool.h.
- Make it possible to use standard bool on Windows.

## 15.00.0000

- Fix a bug that SQLGetDiagField(SQL_DIAG_NUMBER) with SQL_HANDLE_DBC returns SQL_NO_DATA even if an error record exists.
- Fix a typo "RECISION" which should be "PRECISION".
- Enable SQLColumnPrivileges functionality.
- Currently SQLColumnPrivileges() call fails. Fix a typo which is the cause.
- Fix a bug that SQLColAttribute returns wrong column name.

## 13.02.0000

- Fix a bug of CC_send_query_append() when the ignore_roundtrip_time flag is onRemove a mylog() call which is a duplicate of the preceding MYLOG() macro call.

Also direct use of mylog() should be avoided because mylog() uses lots of CPU.

- Noticed that the password field on the PostgreSQL Connection

dialog is always focused when empty, even when other fields that appear earlier on the dialog (and in the tab order) are empty. This is only a minor annoyance, but it does seem like it would be contrary to the expectation of most users. I would propose separating the conditional to inform the user that the password is required, but still focus the first empty field.

- Remove REFCUR_SUPPORT, add FetchRefcursors setting
- Support fetching results from multiple refcursors

Multiple result sets are returned if multiple refcursors are found Works with both ODBC CALL and PostgreSQL CALL syntaxes Query must be executed in a transaction to avoid cursors being closed Now works when output parameters are present.

- Named parameter binding support
- Use ODBC 3 column names for the result set of catalog functions.

## 13.01.0000

- Fix a bug of CC_send_query_append() when the ignore_roundtrip_time flag is on.
- Add a call for SQLDescribeCol() before SQLExecute() to prepare-test.

The diff of the result was reported by Mangold Fabian and will be fixed in the later commit.

- Add a \*update returning\* test case to insertreturning regression test.

The diff of the result was reported by Patrick Cheung and will be fixed in the later commit.

- Let SQLDescribeCol() use parsed result when the current executed result is NULL.

This change fixes the diff of prepare-test regression test reported by Mangold Fabian.

- Let SQLExecute() destroy the old result first.

This change fixes the diff of insertreturning-test regression test reported by Patrick Cheung.

- Forget to apply disable_convert_func flag to VARCHAR and LONGVARCHAR.
- Prioritize DISABLE_KEEPALIVE checkbox over the disable_keepalive bit of ExtraOptions.
- Format check for ExtraOptions of setup dialog.

## 13.00.0000

- Add support for CONVERT scalar function.
- Cope with the case that openssl libraries link msvc runtimes other than libraries which psqlodbc or libpq links.
- Call AC_CHECK_SIZEOF() or AC_CHECK_TYPES() macros at earlier stage where LIBS variable isn't set yet.

These macros compile and run some programs. On some platforms(with some linker options like --no-as-needed in Lnux Gnu linker) the programs try to link libraries specified by LIBS variable at run time. There are some cases that the programs fail due to missing library which is actually not needed and AC_CHECK_SIZEOF() returns 0.

- Fix a compilation error with GCC 10 due to conflicting variable names.

Patch by Paul Wise.

- Remove curr_param_result property of StatementClass and separate parsed result from the exec result.
- Add support for development with VC16(Visual Studio 2019).

MSToolsVersion parameter of BuildAll.ps1 and of regress.ps1 are deprecated.

- Hold the first and last result for parametrized SQL statements with array of parameters.

This would improve the performance of bulk inserts/updates etc.

- Revise the handling of QResultClass list.

Introduce macros QR_concat(), QR_detach() and QR_next().

- Correct the handling of SQL_ROW_ERROR and SQL_ROW_SUCCESS_WITH_INFO.
- Remove the single table restriction in SC_set_SS_columnkey.
- Improve error reporting about SC_pos_reload_needed().

## 12.02.0000

- Add a new \*Display Optional Error Message\* option.

This option allows to display error messages other than primary one. Also add documentation about the option and \*Numeric as\* option.

- Handle notice messages in libpq_bind_and_exec().

Sets and resets a notify receiver around PQexecParams() or PQexecPrepared().

- Ignore PQtransactionStatus PQTRANS_ACTIVE in LIBPQ_update_transaction_status(). PQTRANS_ACTIVE isn't a transaction status.
- Improve execution of parameterized SQL statements with arrays of parameters by sending chunks of SQL statements.

If SQL_ATTR_CURSOR_TYPE of an statement is SQL_CURSOR_FORWARD_ONLY, SQL_ATTR_CONCURRENCY is SQL_CONCUR_READ_ONLY and extended protocol isn't used, the batch execution of the statement is possible. A new option Batch Size was introduced for such cases.
Batch Size: Split an array (of parameters) into chunks of Batch Size to execute statements. The last chunk may contain less than Batch Size elements. Setting 1 to this option forces the current one by one execution.
Also turn off use_server_side_prepare option temporarily when batch execution is possible.

- Change SC_execute() so that it returns a return code which is not affetced by the preceding results. It's necessary for batch execution with arrays of parameters.
- Add a new option IgnoreTimeout.

Some tools issue issue SQLSetStmtAttr(.., SQL_ATTR_QUERY_TIMEOUT,,) internally and sometimes it's difficult for users to change the timeout value. You can disable the timeout by turning on this option.

- An improvement for psqlodbc developers.

Make it possible to call some shell scripts from other directories.

## 12.01.0000

- Fix result-conversions regression test.
- Remove tests for abstime type because the type was removed in PG12.
- Update results for PG12's new floating point output format.
- Fix the bug that causes Error : A parameter cannot be found that matches parameter name�ene'.
- Check BuildResult(the result of drivers build) before building installers.
- Enclose the command part \* Find_VSDir $vc_ver \* with parentheses so that the subsequent \* -ne "" \* isn't considered to be a parameter.
- Cope with the removal of pg_class.relhasoids in PG12 correctly when retrieving updatable cursors.

## 12.00.0000

- Fix the bug that SQLGetDescField() for Field SQL_DESC_COUNT returns SQLINTEGER value which should be of type SQLSMALLINT.
- SQLGetTypeInfo() filters SQL_TYPE_DATE, SQL_TYPE_TIME and SQL_TYPE_TIMESTAMP for ODBC 2.x applications.
- Added support for scalar functions TIMESTAMPADD(), TIMESTAMPDIFF() and EXTRACT() functions.
- The macro IS_NOT_SPACE() is used for not pointers but integers.
- Fix a crash bug when SQLProcedureColumns() handles satisfies_hash_partition().

The proargmodes column of satisfies_hash_partition()'s pg_proc entry is not null but the proallargtypes column is null.
Per report from James.wang.

## 11.01.0000

- Correct the rgbInfoValue returned by SQLGetInfo(SQL_TIMEDATE_FUNCTIONS, ..).

Pointed out by Song X. Gao.

- Because the field 'relhasoids' was dropped in PG12, psqlodbc drivers would have some troubles with PG12 servers.

Report and patch by Vladimir Kokovic.

- Register drivers {PostgreSQL ANSI} and {PostgreSQL Unicode} during installation on 64bit Windows so that users could use the same connection strings in both x86 and x64 environments.

Per report from Grant Shirreffs.

- Correct the rgbInfoValue returned by SQLGetInfo(SQL_LIKE_ESCAPE_CLAUSE, ..).

Pointed out by Grant Shirreffs.

- Fix a typo in SQLForeignKeys-ResultSet-Column. 'deferrablity' should be 'DEFERRABILITY'.

Report from Alexander Roskamp.

- Correct the rgbInfoValue returned by SQLGetInfo(.., SQL_NUMERIC_FUNCTIONS(SQL_SYSTEM_FUNCTIONS or SQL_STRING_FUNCTIONS, ..).

Pointed out by Grant Shirreffs.

- Bug fix: don't forget to set parameter numbers while handling escaped ODBC functions.

Report and patch by Grant Shirreiffs.

- ix test_connection() in setup.c so that settings of conn_settings and pqopt option are reflected properly.

## 11.00.0000

- Remove obsolete maps pointed out.

POWER -\> pow, CONCAT -\> textcat, LEFT -\> ltrunc, RIGHT -\> rtrunc
Patch by Daniel Cory.

- Remove connSettings option and/or pqopt option from the OutConnectionString parameter of SQLDriverConnect() when each option doesn't exist in InConnectionString parameter.
- The parameters should be cast because parameters of concat() function are variadic "any".
- Unbuffered-IO in Windows is incredibly slow. Instead call fflush() after fprintf().
- Add an alias DX of \*Database\* keyword for connection strings to avoid the use of "database" keyword which has a special meaning in some apps or middlewares.
- numeric items without precision are unlimited and there's no natural map between SQL Data types.

Add an option \*Numeric(without precision) as\*.

- Fix a bug that SQLSpecialColumns() returns oid/xmin incorrectly when a table does not exist.

Patch by Quan Zongliang.

## 10.03.0000

- Put back the handling of lock_CC_for_rb variable. The variable lock_CC_for_rb should be held per connection.

Per report from Ayman Samamry.

- Fix SQLGetTypeInfo() so that it filters SQL_TYPE_DATE, SQL_TYPE_TIME or SQL_TYPE_TIMESTAMP for ODBC 2.x applications.

Per report from Oleg Tonkikh.

- Revise ConfigDSN() so that it handles the 4th parameter(lpszAttributes) correctly.

Per report from Haribabu Kommi.

- Fix a crash bug when handling error messages. Also modified some error messages.
- Let SQLTables() or SQLTablePrivileges() show partition tables.
- Fix build on Solaris defined(__SUNPRO_C) using Solaris Studio
- Reduce DB access to pg_class or pg_index by caching relhasoids, relhassubclass etc. It would improve the performance of SQLSetPos() or SQLBulkOperations() very much in some cases.

Per report from Takayuki Tsunakawa.

## 10.02.0000

- It's safer to call setlocale(LC_CTYPE, "") than calling setlocale(LC_ALL, "").

Report and patch by Mario De Frutos.

- Avoid replacing effective notice messages.

Report from Wolfgang Apolinarski.
Patch by Clemens Ladisch.

- Handle MALLOC/REALLOC errors while fetching tuples more effectively.

Per report from Haruka Takatsuka.

- Make SQLSetPos(SQL_DELETE/SQL_REFRESH) more effective. Because queries calling currtid(2) like select .. from .. where ctid=currtid2(.., ..) cause Seq Scan, their execution may be very slow. It is better to execute queries using subqueries like select .. from .. where ctid=(select currtid2(.., ..)) because they cause Tid Scan. Report and changes by Tsunakawa Takayuki.
- Fix a crash bug in AddDeleted().

Report and patch by Takayuki Tsunakawa.

## 10.01.0000

- Call Get-VSSetupInstance to detect Visual Studio installations whose version \>= 15 when VSSetup module is available.
- Review the handling of @@IDENTITY when a column's default is nextval(a_sequence) but the column isn't a serial. Unfortunately the function pg_get_serial_sequence() doesn't work for sequences which aren't serial type.
- Add add an option -NoPDB to buildInstallers.ps1.

The switch option can be specified so as to exclude PDB files from the installers.

- Fix a bug in pgdebug_realloc().
- Free any data at exec params before the statement is executed again or the next set of parameters is processed. If not, then there will be a memory leak when the next SQLParamData/SQLPutData is called.

Per report from Mihai Giurgeanu

- Review the level of MYLOG, QLOG.

Divide \*level 1(detail)\* log into \*level 1(tuple level)\* log and \*level 2(detail)\* log.

## 10.00.0000

- Fix multibyte handling of ansi driver.
- Handle PG 10 identity columns.
- For PG10 servers, we will never see "unknown" output columns.
- Improve @@identity handling.

Use multibyte-aware eatTableIdentifiers() instead of next_name_token().
Use identifierEscape() to escape single quotes or double quotes.

- PG_PRINTF_ATTRIBUTE instead of printf in __attribute__((format(printf, .., ..))) function attribute.
- Review the use of encoded_nextchar().

Remove variables which work in conjunction with encstr(encoded_str) so as to avoid misleadingness.

- Use pg_get_serial_sequence() to get serial sequence of identity columns or serial columns.

pg_get_serial_sequence() for identity columns became available in PG 10.

- Preparation for development with VC15(VS 2017).
- Fix the bug that 'create table T_$001 (\\81c)' fails when useServerSidePrepare is turned on.

Per report from Gregory Pruss.

## 09.06.0500

- Fix a segfault on ResolveOneParam() function while executing a query after calling SQLSetStmtAttr(.., SQL_ATTR_APP_PARAM_DESC, an_ARD_for_another_statement, ..).
- Fix a linkage error when doing installcheck for test/win.mak.

Report and patch by Kazuki Uehara.

- Fix a bug when fetching bytea field to SQL_C_BINALY data. It's bad to double count of bytes.
- Handle '$'s in identifiers correctly. Also take multibyte characters into account in identifiers.
- Reduce the use of snprintfcat(or SPRINTFCAT_FIXED macro) as much as possible. Instead use PQExpBuffer library without having to worry about buffer length.
- Use macro MYLOG instead of function mylog in preparation for later changes and unify mylog() and inolog() using level parameter. Enable the compiler to check the format string against the parameter of mylog() by adding __attribute__((format(printf,.,.))) to the declaration of mylog(GCC only). Using this mechanism, check and fix many discordances of parameters.
- Implement myprintf() which prints logs according to printf format and make logs a little easier to read using the macro MYPRINTF() which calls myprintf().
- Introduce token control while parsing. Detect \*select .. into\* or \*select ... for update\* and suppress \*declare .. cursor for\* clause in declare/fetch mode using the mechanism.
- Add __FILE__, __FUNCTION__, __LINE__ to MYLOG.
- Fix an parameter unmatch error which would cause a crash.
- Removed some qlogs, changed some qlogs to MYLOG and added some qlogs.
- Move the implementation of GetExeName() from psqlodbc.c(psqlsetup.c) to mylog.c.
- Remove the GssAuthUseGssapi option completely.
- Fix bug that calls strdup() for NULL pointers.

Per report from Ding, Haiqiang.

- Detect fast shutdown while connecting.
- Put back the behavior when conversion errors from the current locale to SQLWCHAR occur. Simply returns zero length string instead of erroring out.

Per report from Andrus.

- Add level parameter to Commlog/Mylog option of setup program.
- Add __attribute__((format(printf,3,4))) to the declaration of snprintfcat().
- Allow PROCCALL in ReadOnly mode.

Per report from Matej Mihelic.

## 09.06.0410

- Fix buffer truncations. The truncations occurred when concatenating 'RELEASE xxxx...;' and 'SAVEPOINT xxxx....'.

Per reported by Pavel Raiskup.

- Correct the behavior of snprintfcat() on Windows using posix_vsnprintf() instead of _vsnprintf().
- Handle StrLen_or_IndPtr argument of SQLBindParameter() properly when StrLen_or_IndPtr doesn't point to SQL_NTS and conversions between SQLWCHAR and SQLCHAR occur.

This change fixes MSACCESS errors reported by Christian Ullrich and Robert Ball.

- Make quote_table() and ti_quote() thread safe.This change fixes "relation does not exist".

Per report and patch by Scott Beil.

## 09.06.0400

- Reorganize the language conversion between the current locale and unicode.
- Make a function setup_getdataclass() separating from long confusing if-block in convert_text_field_to_sql_c() and clarify the parameters.
- Implement SC_get_localtime() and reduce time()/localtime() calls.
- Visual C++ 14 provides snprintf().
- Fix a crash bug in SQLBulkOperations() under use_declare/fetch off mode.
- Clean up the use of 'internal' member of StatementClass.
- Fix an infinite loop of CC_on_commit. The loop occurs when checking whether cursors are alive using 'MOVE 0' in CC_clear_cursors().
- Fix the bug, skip malloc(0) in build_libpq_bind_params(). malloc(0) seems to return NULL on AIX, whereas it returns non-NULL on Windows or linux.

Per reported by Keith Handlon.

- Add support for foreign tables and matviews in SQLTables.
- Documentation fix for the 'PROTOCOL=' option. It's out of sync with the current driver behavior (since 09.05.0100).

Patch by Jan Wieck.

- Fix more bugs with returning NULL for malloc(0).

Patch by TAKATSUKA Haruka.

## 09.06.0310

- **Fix a crash bug when connecting from MSACCESS(Microsoft Jet)**

Per reported by Robert Ball.
This occurs when MSACCESS(Jet) calls SQLConnectAttr(.., 30002, ..) =\> ucs2_to_utf8() and about to call mylog() with a NULL dereferenced pointer parameter.

## 09.06.0300

- **Implement SQLSet/GetConnectAttr() for some driver specific options.**
- **Modify the code about UNKNOWNS_AS_LONGEST of the Unknown Sizes option so that it works properly.**
- **Revise the handling of show oid column and row versioning option.**
- **Revise the use of pgtype_xxxx() in catalog functions.**
- **Replace pgtype_xxxx() whose 1st parameter is StatementClass with pgtype_attr_xxxx(). **

Also use macros for pgtype_xxxx() calls which have PG_ATP_UNSET, PG_ADT_UNSET or PG_UNKNOWNS_UNSET parameters.

- **Remove the use of PG_STATIC and UNKNOWNS_AS_DEFAULT.**
- **Remove UNKNOWNS_AS_CATALOG of Unknown Sizes option.**
- **Use symbols defined in catfunc.h instead of literal column numbers in SQLGetTypeInfo(), SQLSpecialColumns() and SQLTablePrivileges().**
- **Fixes about SQLDescribeCol() or SQLColAttribute().**
- Report the size of constant strings using Unknown Sizes as Longest.
- Correct the inconsistency between the sqltype of PG_TYPE_TEXT or PG_TYPE_UNKNOWN and the size in case of Unknown Sizes as Longeset.
- **Fix a bug about Unicode driver for ansi apps.**
- **Fix a dangling reference in CC_lookup_characterset() introduced.**
- **Simple implementation of get_environment_setting() on non-Windows platforms. If the codeset part of the current locale is UTF8, set the client_encoding to 'UTF8'. Take \* set client_coding='...' \* into account in check_client_coding().**
- **Revise the handling of client encoding so as not to forget the update of related items(ccsc or mb_maxbyte_per_char).**
- **Change the max byte per char of UTF8 from 6 to 4.**
- **For Unicode drivers, when SQLConnect() or SQLDriverConnect() instead of SQLConnectW or SQLDriverConnectW is called, use the same client encoding as ANSI version of drivers use.Stop using percent-encoding to store the conn_settings option in odbc.ini.**
- **Stop using percent-encoding to store the conn_settings option in odbc.ini.**
- **Add #ifdef UNICODE_SUPPORT where there should be.**
- **Introduce locale_encoding which corresponds to the locale. For non-Windows platforms determine the locale encodings from the current locale.**
- **Make a function convert_text_field_to_sql_c() simply extracting from long confusing code and clarify the parameters for subsequent modifications.**
- **Handle locales in non-Windows platforms and overhaule the language system.**
- **convert bad to use lf_conv here.**
- **Add __STDC_ISO_10646__ check to confirm that wchar_t means unicode.**
- **Error out when SQLSetPos() skips updates.**
- **Don't reset the status of Keyset in case of updates.**

This fixes the bug about repeated updates using SQLSetPos().

- **Correct the inappropriate use of set_nullfield_int2().**
- **Review the handling of OUTPUT parameters in SQLProcedureColumns().**
- If there are OUT, INOUT or TABLE arguments, ignore RETURN values.
- OUT parameters of functions with returning SETOF should be treated as SQL_RESULT_COL rather than SQL_PARAM_OUTPUT.
- Also treat table output arguments as SQL_RESULT_COL because functions returning table are equivalent to using one or more OUT parameters plus marking the function as returning SETOF record (or SETOF a single output parameter's type).
- **Use the same CPPFLAGS as well as CFLAGS/LDFLAGS to build regression tests as used to build driver.Also uchar.h header checking for later use.**

Fix a crash bug when the locale isn't set connection.

## 09.06.0200

- **Start transactons first before calling SQLEndTran().**
- **The failure of libpq_bind_exec() which returns NULL doesn't mean a FATAL error. Also make a check for disconnected connections.**
- **Handle output parameters properly in build_libpq_bind_params.Also handle return values properly.**
- **Make check of connection lost.**
- **Revise recent lost connection check.**

Check the lost connection

- at the beginning of ODBC APIs.
- at the beginning of FE-BE communications using extended protocol.
- **Stop specifying parameter datatype oids in parse message. Unfortunately it is inflexible and rather harmful. Though I don't know if it has much meaning, use cast instead just in order to keep regression test results. It's not necessary to cast any more the 2nd parameter of {call a_b_c_d_e(?, ?, ?, ?, ?)} in odbc-escapes-test. **

## 09.06.0100

- **Optimization of a build system.**

some adjustment and support Visual Studio 2015 for Windows.

- **Use SIZEOF_LONG_INT to check whether SQLROWSETSIZE exists.**

SIZEOF_LONG_INT is defined in UnixODBC headers, while SIZEOF_LONG is defined by psqlodbc's ./configure. In odbcapi.c, we use it to determine what format modifier, %ld or %lld, to use for pointers. (Why don't we use %p?). We should rely on our autoconf check for that. In odbcapi.c, however, the check should match that in the ODBC headers, to determine whether SQLROWSIZE exists, so we should use SIZEOF_LONG_INT.

- **Fix the bug messages handle.**

handle_error_message() and handle_notice_message() were unified into handle_pgres_error() in the wrong way. Unfortunately handle_error_message() never handled notice messages and set positive errornumbers which mean error_not_a_notice in psqlodbc. I renamed the constant CONNECTION_SERVER_REPORTED_ERROR to CONNECTION_SERVER_REPORTED_SEVERITY_FATAL and the constant CONNECTION_SERVER_REPORTED_WARNING to CONNECTION_SERVER_REPORTED_SEVERITY_ERROR so as to avoid confusion.
Reported by Shinji Mecha.

- **Allow setting isolation level before the establishment of connection.**
- **Fixed return proper sqlstate error 08S01 in case of communication link failures.**

Patch by Tsunakawa,Takayuki.

- **Check the existence of directory for logging.**
- **Handle PG_DIAG_SEVERITY_NONLOCALIZED for servers whose version \>= 9.6 so as to detect FATAL message properly.**
- **Now default isolation level is server's default instead of fixed SQL_TXN_READ_COMMITTED.**

Patch by Tsunakawa,Takayuki.

- **Make sure the isolation level is SQLUINTEGER and add the declaration of CC_get_isolation().**
- **Change the default for ExtraSysTablePrefixes to an empty string.**
- **Use lo_lseek64(lo_tell64) instead of lo_lseek(lo_tell) if the server's version \>= 9.3. We are now able to handle \> 2GB BLOBs.**
- **Implement SQL_FECTH_BY_BOOKMARK operation of SQLBulkOperations properly.**
- **Enable SQLBulkOperations in declare/fetch mode using long (\>=12bytes) bookmarks.**

Formerly the driver offered very simple bookmark support -- it is just the current row number. Now the driver offers more verbose bookmarks which contain KeySet information (CTID (+ OID)). Though they consume 12bytes (row number + CTID) or 16bytes (row number + CTID + OID), they are useful in declare/fetch mode.

- **a hack to handle boolean items in VBA with MS Access.**

VBA seems to transform the where condition a_boolean_item=True into ("a_boolean_item" = 1) which causes an ERROR:Operator does not exist boolean = integer . Here transforms it into ("a_boolean_item"='1') which seems safe in anyway.

- **Add a new option 'pqopt' which specifies libpq connection parameters as a conninfo style string.**

The string must be enclosed with braces {} e.g. pqopt={sslcert=... sslkey=... sslrootcert=...} in the case of connection strings. Though host, port, dbname, user, password, sslmode, keepalives_idle or keepalive_interval parameters can be set using this option, they are ordinarily set by other options. When some settings for those parameters conflict with other ordinary options, connections are rejected.

- **Fix a corner case when parsing connection strings enclosed with braces.**
- **As tuple_updated item in AddUpdated(), the parameter should be used. Deriving from other items caused bad results.**
- **Change the default of ByteaAsLongVarBinary from 0 to 1.**

## 09.05.0400

- **Enable COMMLOG and MYLOG button.**
- **Default of \*level of rollback on errors\* is default(statement) not transaction.**
- **Fix the bug SQLGetDescFieldW or SQLColAttributeW mistakenly returns SQL_ERROR.**
- **Move src/\*.exe to exe/\*.exe on Windows.**

Also use fc command instead of diff command on Windows. Fix a compilation error in large-object-data-at-exec-test.c regarding a type declaration in the middle of a code block.
Patch by Michael Paquier.

- **Ignore the difference between CR LF, LF and CR line break when comparing expected and result output.**
- **Prior to Visual Studio 2015 Update 3, the hook functions of type PfnDliHook __pfnDliNotifyHook2 and __pfnDliFailureHook2 were non-const.**

They were made const to improve security (global, writable function pointers are bad). If for backwards compatibility you require the hooks to be writable, define the macro DELAYIMP_INSECURE_WRITABLE_HOOKS prior to including this header and provide your own non-const definition of the hooks.
The following information is from Dave Cramer(davecramer@gmail.com).

- **Fix a regression test some failures.**

## 09.05.0300

- **Largely reduce overhead of localtime() in SQLFetch**

localtime() is a very costly system call causing serious grief on performance of the driver for OLTP-type applications using a lot SQLFetch and this one is being called for all data types when performing an in-line conversion, while it is being used only in the case where some of the date and/or timestamp fields are not initialized. While testing performance of an internal application, this patch has proved to improve the overall performance of an application by 20%.

- **Fix the use and the implementation of getNthValid().**
- **Fix the handling of boundary_should_be_adjusted case when a rowset is cut in the middle of cached rows. Also fix the row info of SQL_ADDed tuples.**
- **Estimate the rowsetSize including deleted tuples correctly so that rowset can be effectively handled.**
- **Set once_reached_eof status when useDeclareFetch=0 so that QR_num_total_read() macro works well.**

Don't forget to call CC_on_abort_partial() when rollback back to a savepoint. Now the driver passes cursor-block-delete test.

- **Refine SQLFetchScroll() for the orientations other than SQL_FETCH_NEXT/PRIOR.**

Discard move_from_the_last stuff and call move_cursor_to_last() instead.

- **Refine cursors with KEYSET using oid or oid alternative as far as possible.**
- **Preparation for handling inheritance.**
- **Convert tableoid to relation name in case of inheritance.**
- **fix buffer overrun in logging, and code cleanup.**

Patch by Tsunakawa, Takayuki.

- **Implement SQL_QUERY_TIMEOUT.**

Patch by Tsunakawa, Takayuki.

- **'for' loop initial declarations are only allowed in C99 mode.**

## 09.05.0210

- **Makefile.am in order to add new test case and expected output to distribution.**
- **Use LoadLibraryEx for gxalib.dll load search path.**

## 09.05.0200

- **Clear the parsed result before executing simple query so that SQLRowCount() returns.**

This fix the bug reported by Walter Willmertinger.

- **Fix NULL dereferences which causes application crash.**

Patch by Tsunakawa, Takayuki.

- **Don't initialize next_in_cache or rowstart_in_cache info when scrolling (block) cursors in declare/fetch mode.**

This fixes the bug reported by John Kew.

- **Correct the detection of eof in useDeclare/fetch mode.**

This change fixes the bug about fetching prior after scrolling next until eof reported by BenS.

- **Fix the bug about MONEY type.**
- **Consider decimal_digits of SQL_TIME (and time field of PostgreSQL).**
- **Don't forget to update num_total_read field of QResultClass record in case 'fetch 0 rows' after 'move'.**

This fixes the bug 'When fetching a row "behind" the rowset by "fetch absolute" only the first ones of the result set can be fetched.' reported by Bernhard Goebel.

- **Make sure that EOF occurs when 'fetched \< fetch requested'.**
- **Correctly handle SQLGetData() when the target type is SQL_C_WCHAR and the buffer is small to get the whole data for some data types such as bool, date, time ... etc.**
- **Refine SQLGetData().**
- Unify midtemp and moneytemp.
- Variable wconverted is no longer needed.
- Pre_convert for PG_TYPE_BOOL is not needed.
- It's better to take the same road even in case of pre_convert.
- **Fixed possible once to build test suite in a different place other than the source code is, it's not now.**
- **CHAR causes a compilation error in some environment. Use char instead of CHAR.**
- **Correctly judge the SQLSTATE 01S06(Attempt to fetch before the result set returned the first rowset) for SQL_FETCH_PRIOR operations of SQFetchScroll.**
- **Checking a flag bit mistakenly resets other flag bits.**

Patch by Tsunakawa, Takayuki.

- **setup.c:test_connection() calls EN_Constructor() twice, leaking small memory.**

Patch by tsunakawa, Takayuki.

- **The existing memory is not freed upon realloc() failure.**

PGAPI_BindCol() doesn't check the failure of extend_getdata_info().
Patch by Tsunakawa, Takayuki.

- **The statement linked list is not properly chained, which throws away some statements.**

Patch by Tsunakawa, Takayuki.

- **LINKMT is not used in the build process.**

ADDL_INC should be ADD_INC. Just a typo. The CLEAN target doesn't delete .pdb files.
Patch by Tsunakawa, Takayuki.

- **Fix a parameter unmatch of snprintf.**
- **In Windows the use of external variables timezone, tzname and daylight are deprecated. Use _timezone, _tzname and _daylight instead.**
- **SQLS(G)etDescRecW instead of SQLS(G)etDescRec for Unicode driver.**
- **The third stricmp() in writeDriverCommoninfo() is useless, because the second one causes the function to return.**
- SC_get_errormsg() definition is repeated twice in succession.
- In info.c:SQLGetInfo(), "len = 0" is unnecessary because len is assigned at the end of the function.
- In execute.c:PGAPI_ExecDirect(), flag doesn't need to be set as its value doesn't change.
- PODBC_PER_STATEMENT_ROLLBACK is not used.

Patch by Tsunakawa, Takayuki.

## 09.05.0100

- **Use libpq for all communication with the server**

Previously, libpq was only used for authentication. Using it for all communication lets us remove a lot of duplicated code. libpq is now required for building or using libpq.

- **Remove support for protocol versions older than 3.**

Protocol version 3 was introduced in PostgreSQL 7.4, so this means that we no longer support server versions 7.3 or older. This simplified the code, making maintenance easier.

- **Remove support for ODBC versions older than 3.51.**

The official binaries have been compiled in 3.51 mode for a very long time, so this has no effect on the official binaries.

- **Avoid one round-trip to the server when establishing connection.**

When connecting to a server, the driver used to issue a query to determine the client_encoding setting being used, but that was unnecessary because all supported server versions send that information as part of the connection handshake when a connection is established. Remove the extra query, which should make connecting over a high-latency link faster.

- **Report max length of 'name' fields correctly.**

PostgreSQL columns of type 'name', used e.g. in catalog tables to hold names of relations and other objects, were reported as having type VARCHAR(64). However, the real max length of a name field is only 63 bytes.

- **Remove 'Optimizer' configuration option**

You can use the generic ConnSettings option with value "set geqo=off", to disable GEQO.

- **Fix title in 32-bit Unicode drivers setup dialog**

The title on the 32-bit Unicode drivers's setup dialog on Windows incorrectly said "ANSI".

- **Don't export private symbols that are not part of the ODBC interface **

This was causing problems when the driver was loaded together with another dynamically loaded library with a function of the same name. This was reported to happen when loading the odbc_fdw foreign data wrapper into a PostgreSQL server, because both the PostgreSQL server and the ODBC driver contain a function called check_client_encoding().

- **Report a sensible SQL_DESC_OCTET_LENGTH value for result set columns of type 'unknown'**

If a query returns a column of type 'unknown', as happens e.g. in a query like "select 'foobar' from table" where the datatype of the constant 'foobar' is not specified, the SQL_DESC_OCTET_LENGTH property of the column was returned as invalid. Report it the same as 'varchar' field with no explicit length, instead.

- **Fix quoting bugs in sending integer query parameters to server**

The drivers used to assume that if a parameter's SQL type is SQL_INTEGER or SQL_SMALLINT, the value does not require quoting when its send to the server. For example, "SELECT ?", with parameter 123 was translated to "SELECT 123", when UseServerSidePrepare was not enabled. However, there was no check that the query parameter in fact contained a valid integer, when replacing the parameter markers with their values. Also, in a query like "SELECT 0-?", a negative value needs to have parens around it, as in "SELECT 0-(-123)".

- **Don't reset autocommit when a connection is established**

If autocommit is disabled on a connection, by calling SQLSetConnectAttr(SQL_ATTR_AUTOCOMMIT, SQL_AUTOCOMMIT_OFF, 0), before connecting with SQLDriverConnect(), autocommit was incorrectly reset back to on when the connection was established.

- **Send datatype information for query parameters, when known**

If a query parameter is bound with a specific SQL type, pass on that information to the server. This makes the behaviour of queries like "SELECT '555' \> ?" more sensible, where the result depends on whether the query parameter is interpreted as an integer or a string.

- **Use libpq defaults for Server, Username and Database settings**

Server, Username and Database settings can now be left empty, in which case the corresponding libpq defaults will be used. The libpq default is to connect to a local server over Unix domain sockets (on Unix systems) or to localhost over TCP/IP (on Windows), if Server is left empty. The default username is the current operating system user, and the database to connect to is a database with the same name as the user. These built-in defaults can be changed by environment variables, however. This is particularly useful for connecting to a local installation via Unix domain sockets, as that is the default when Server is left empty, and was difficult to configure otherwise.

- **Fix bug in parsing E'' literals**

The driver needs to parse a query string to determine whether a '?' character is a parameter marker, or if it's inside a quoted string or a SQL comment. This parsing did not correctly handle E''-style strings.

- **Remove 256 byte limit on the length of NOTICE messages**

There was a built-in limit of 256 bytes on the length of NOTICE messages received from the server. Strings larger than 256 bytes were not returned to the application. That limit has been removed.

- **Fix parsing of strings containing escaped quotes with Parse=1**

This fixes issues in extracting metadata like result set column names, with the Parse=1 option.

- **Fix connecting with a user or database name that contains spaces or quotes **

User and database names were not quoted correctly when building a connection string. A different method is now used for sending those parameters, which doesn't require quoting and escaping.

- **Remove support for so-called "premature execution"**

Premature execution was a method for getting information about a result set of a query, before SQLExecute() was called. That was always dangerous, as any side-effects of the query would happen, even if the application did not call SQLExecute() after all. With protocol version 3, there is a safer way to do this. This also removes the DisallowPremature configuration option, as the driver never does premature execution anymore.

- **Fix buffer overrun when the server reports an error message longer than 4096 bytes**

If the server returned an error longer than 4096 bytes, we would overrun the buffer by two bytes. If you're unlucky, that could lead to a crash. That consistently happened on Windows.

- **Improve parsing of INSERT INTO statements, for @@identity support**

The code that parses INSERT INTO statements, to extract the name of the target table, got confused by values containing dots, and other things. Improve the parsing support to handle a wider range of INSERT statements.

- **Fix crash in SQLTables() function, if search_path was set to a non-existent schema**

The server-side current_schema() function returns a NULL if search_path is set to a non-existent schema, and the driver code was not prepared for a NULL result.

- **Fix buffer overrun when connecting, and KeepaliveTime and KeepaliveInterval were both set**

If the server returned an error longer than 4096 bytes, we would overrun the buffer by two bytes. If you're unlucky, that could lead to a crash, and it consistently did on Windows. Per report from Andrus Moor.

- **Set an error message when password is required to connect.**

In previous versions, if a connection fails because the server requires a password and no password was supplied. Per report from "PSequel Support".

- **Improve parsing of INSERT INTO statements to support @@identity**

Patch by Christian Ullrich.

- **Fix SQL_DATA_AT_EXEC processing for large objects.**

Reported by Vadym Krevs.

- **The server decide if SQL_LONGVARBINARY means a bytea or large object.**

This reverts the behaviour of SQL_LONGVARBINARY parameters.

- **Fix handling of return value in {? = CALL ... } syntax.**

When sending the query to the server, need to use the number of parameters calculated by build_libpq_bind_params(), which excludes the OUT parameter, rather than the original number of parameters. Reported by Haribabu Kommi.

## 09.03.0400

- **Rip out broken retry/timeout logic in SOCK_wait_for_ready.**

At a quick glance, the logic looked like retry with progressive timeout, but it was horribly broken. First of all, the timeout was only used if retry_count was passed as 0. And in that case, the timeout was 0. So retry_count == 0 actually meant "check if data can be read/written without blocking", while retry_count \> 0 meant "sleep until data is available". There was also some code to handle retry_count \< 0, but that was dead code because none of the callers passed a negative value. The hardcoded max. retry count of 30 retries was quite bogus too. Filling the output buffer more 30 times is not necessary an error, and it's not clear that the ssl library could not return SSL_ERROR_WANT_READ or SSL_ERROR_WANT_WRITE more than 30 times. Unlikely, yes, but possible. So, just rip out the retry_count and progressive timeout logic. Replace the retry_count argument with a simple "should block?" argument. I didn't dare to remove the condition that we always wait if SSL is enabled, although I don't understand that at all. But perhaps there's some well-hidden reason for that. Per report from Prakash Itnal, although this just removes the broken code, and won't enable the kind of a timeout that he wanted.

- **Clean up Windows makefiles.**

Get rid of the CPU and CPUTYPE variables in win64.mak. Instead, assume that the TARGET_CPU variable has been set correctly in the environment. TARGET_CPU is set by "setenv" script that comes with the Microsoft Visual Studio C compiler package. It never worked to pass a "CPU=x86" and using a x64 compiler, for example, so seems better to determine the correct target from the environment, than fail with a cryptic error message if the CPU argument and the environment don't match. To build 32-bit binaries with win64.mak, simply do "setenv /x86" before running "nmake -f win64.mak". Similarly, to build 64-bit binaries, use "setenv /x64". It should now be possible to build both 32-bit and 64-bit binaries with win64.mak. Ideally, we could now remove win32.mak altogether, but there are small differences between win64.mak and win32.mak in the build options that they use. I don't understand the differences enough to dare to remove win32.mak yet. The CPU=AMD64 alias for x64 is no longer supported (as the whole parameter is now gone). Also do whitespace & other cosmetic fixes to win32.mak and win64, to make them more similar. This helps when comparing the two with "diff". Remove remains of the long-gone "MULTIBYTE" setting (it was misspelled in win64.mak, anyway)

- **Fix crash if connection is closed while sending a query to the server.**

Some functions reset conn-\>sock to NULL on a socket error, so we must not keep a copy of it in a local variable. Also, SOCK_get_id should check for NULL argument like all the rest of the SOCK_get/put_\* functions.

- **Rewrite the conversion functions between strings and SQL_NUMERIC_STRUCT.**

Aside from making the functions more readable and faster, this fixes a number of bugs:

- When converting a large number from decimal to binary, the least significant byte might be wrong. reported by Walter Couto.
- In binary-\>decimal conversion, a number with small precision but large scale caused a "Floating point exception".
- In binary-\>decimal conversion, building the final string was wrong, picking up "digits" from outside the reserved stack space, if scale was larger than 40
- The param_string buffer allocated for the result of binary-\>decimal conversion was too small.

Also add a test cases for all of these bugs.

- **Fix ODBC function escape for SPACE function.**
- **Make sure that the psqlodbc ANSI driver is ODBC3.5.**
- **Register 64bit XA DLL on installation**

Changes by a patch from Craig Ringer

- **Fix potentially uninitialized local pointer access which causes.**

fatal compilation errors on vs2012/2013 reported by Craig Ringer.

- **Fixed ctype_length() is missing the length for SQL_C_BIGINT and SQL_C_NUMERIC.**

patch by John Smith.

- **Fix access to unallocated memory in IAsyncPG.**

IAsyncPG was deleting its self, then returning an instance variable by value. This is probably going to be safe pretty much all the time, but it's still a memory error, and DrMemory (a Windows valgrind-alike)

- **SQL_NUMERIC_STRUCT was not available until ODBC 3.0.**

Also add a prototype for ResolveNumericParam for readability.

- **Increase digits used to convert from REAL/DOUBLEs to strings so that the reverse conversions can recover original values.**

Patch originally by John Smith.

- **Clear errors for prepared statement for subsequent executions.**
- **Fix the problems using SQLFetch on prepared INSERT with RETURNING clause.**

reported by John Smith.

- **Fix use of server-queried version before connection.**

psqlODBC attempted to use the version that's queried from the server before it was obtained from the server during the connection setup. Because the server version is initialized to 0.0, tests for "server version \>= 7.4" or "server version \>= 6.4" using the PG_VERSION_LT macro were always returning false. Instead, use PROTOCOL_64 and PROTOCOL_74 tests against conninfo. These don't offer any "greater than" form, so just test both. There's little point adding anything prettier when it can all be removed when v1/v2 protocol support is removed soon anyway. The main symptom of this issue was that psqlODBC was sending an invalid protocol message for the v3 protocol, "Q \\0", causing the server to wait for the second half of the length word forever. At connection close, the server would emit:
unexpected EOF within message length word
This issue was observed while tracking down problems with XA transactions in MSDTC failing to commit after a successful prepare because an isolated tx created via getLockedXAConn() got stuck here, but is not confined to that problem.

- **Don't use PG_VERSION_xx() macros in original_CC_connect().**

it's not obtained from the server yet. This may fix another cause of the problem Craig Ringer reported.

- **Fix bug in SQLCancel().**

If one thread was busy executing a savepoint statement, while another thread calls SQLCancel() on the statement, SQLCancel() would try to release the locks held by the other thread. That's extremely racy; if the other thread tries to increment/decrement the lock_CC_for_rb counter at the same time, we could end up releasing the lock to many times or too few times. Also, releasing a pthread mutex from a different thread than the one who acquired it is undefined behavior. AFAICS the DiscardStatementSvp() call in SQLCancel() can be safely removed. It's outright wrong and dangerous if the statement is busy executing in a different thread, and the other codepaths in PGAPI_Cancel() already call DiscardStatementSvp(). Bug report by Jade Koskela.

- **Fixes a NULL pointer dereference occurring when connecting.**

using SSPI authentication (without kerberos, a local Windows user trying to connect to postgres instance on same host). Patch originally by Nikhil R Deshpande.

- **Fix bug with UseDeclareFetch=1 when a transaction is committed before fetch.**

If a server cursor is closed before the application has fetched any rows from the result set, the "base" of the result set's cached rowset was off-by-one. Also add a regression test for the same. This fixes the bug reported by Jan-Peter Seifert.

- **Fix driver name mismatch between 32-bit ODBC app and 64-bit MSDTC host.**

If the 32-bit driver runs on a 64-bit host within a 32-bit app running under SysWow64 then XA transactions will fail to recover. That's because recovery is done by pgxalib.dll which runs with in MSDTC.exe - a 64-bit application. It connects to a DSN supplied by psqlODBC in EnlistInDtc_1pipe during DTC enlistment by the XA resource manager. This DSN uses the driver name of the client's ODBC driver - which is fine so long as the client and MSDTC architectures match. If they don't then MSDTC will fail to connect to Pg, so it'll never resolve transactions. Traces of MSDTC will show RM_COMMIT_DELIVERY_FAILED_DUE_TO_CONNECTION_DOWN events. Traces of pgxalib.dll will show xa_recover(..) being entered, followed by "SQLDriverConnect return=-1".

- **Remove PgDtc_set_property(.., prepareRequestded, 0) which causes a crash in case PREPARE TRANSACTION error.**

The property prepareRequested isn't set anywhere.

- **Add KeepaliveTime and KeepaliveInterval options to specify time or interval for keepalive.**
- **Fix a memory leak check qresult.**

Per report by Pluto Cobain.

- **Get rid of copy and paste code in installer/ and unify them.**

The powershell scripts had lots of duplication. Make the unified code instead. Original patch was provided by Craig Ringer.

- **Clear unused stuff from PgDtc_isolate() function.**

Also add CC_initialize_pg_version() call after CC_copy_conninfo() so as to avoid the trouble caused by early PG_VERSION_xx() calls.

- **Fix race condition in getting the current decimal separator.**

The code to get the current decimal separator was not thread-safe. If two threads concurrently saw that the locale had changed, they might both try to free() the same string. This isn't a perfect fix, there's still a race condition if a thread changes the locale while another thread is running the psqlodbc code. But at least this eliminates the race condition when the locale was changed earlier, not concurrently, and even if it is changed concurrently, this is less likely to crash, depending on the way localeconv() is implemented in libc.

- **Fix bug in building an abbreviated connection string.**

Introduced by the keepalive-patch, which accidentally removed a line from sprintf format string. Caught by a compiler warning.

- **Fix buffer overflow in SQLGetData.**

If a PostgreSQL timestamp column is fetched using SQLGetData, into a SQL_C_CHAR output variable, with size of 20-22 bytes, the driver might overrun the buffer by a few bytes with unusual timestamp values, with year \< 0 or \> 10000. While at it, fix the truncation behavior for date/time/timestamp values so that if the output buffer is too small, we still write what fits and truncate, instead of writing nothing. This is something that one might do on purpose, e.g. if you only want to fetch the year part of a date, you might call SQLGetData() on a date column, with only 4 bytes long output buffer. (although we haven't heard any complaints from the field). Also, if the output buffer is of type SQL_C_WCHAR, but the buffer length is odd, and we have to truncate, we didn't NULL-terminate the string. Using an odd-sized buffer with SQL_C_WCHAR is a pretty unlikely thing to do, but nevertheless I think this is more correct behavior.

- **The function SC_setInsertedTable() prepares an info for subsequent 'select @@IDENTITY' command.**

Take table name like catalog.schema.table into account in the function. Fix the bug reported by Phillippe Champignon.

- **Improve pgxalib.dll(MSDTC support).**
- Add an option which lets xa_open() try to connect to the database immediately. This would cause an error at enlistment in dtc when the authentication method depends not only on the db user but also on the OS user, which means the automatic transaction recovery is unavailable.
- Log the error message in case of SQLDriverConnect() error.
- Improve the logging.
- **Fix the regkey search bug when XARMcreate() causes an error at enlistmentvin DTC.**
- **Additional improvement on MSDTC enlistment.**

Unfortunately the current pgxalib.dll can't recover transactions which use sslmode verify-\[ca|full\] or whose authentication is SSPI, certificate or ident. When MSDTC.exe tries to connect to PostgreSQL it does so under Windows user NETWORKSERVICE, which won't match the username the original user connected to PostgreSQL as. So PostgreSQL will reject the connection.

- **Fix SQLTables column names to be ODBC version 3 compliant.**

In ODBC3, TABLE_QUALIFIER was renamed to TABLE_CAT, and TABLE_OWNER to TABLE_SCHEM. Per report David Hedberg.

- **Add a new dialog page to the setup program and allow the setting of PREFERLIBPQ and XAOPT option.**
- **Add a powershell script buildBootStrapper.ps1 which builds bootStrapper program with some optional parameters.**

Quote the version number of bootstrapper from the configuration xml file unless specified. Also change the name of 32bit MSI in the bootstrapper according to changes of 32bit installer build system. Change buildInstallers.ps1 build the bootstrapper together. Change ALLUSERS from "2" to "1" of the 64bit installer.

## 09.03.0300

- **Fix implicit casts between SQLCHAR and char. Don't rely on -Wno-pointer-sign.**
- **Pass the argument to isalpha/isspace etc. routines as unsigned char.**

Per the C standard, the routine should be passed an int, with a value that's representable as an unsigned char or EOF. Passing a signed char is wrong, because a negative value is not representable as an unsigned char. Unfortunately no compiler warns about that.

- **Use "function(void)" instead of "function()" to declare 0-arg functions.**

"function(void)" is the correct syntax in a function declaration, although in practice compilers accept the latter too. It's OK in a function definition, but change those too for consistency. This fixed by Michael Paquier.

- **Add missing function declarations.**

These are not actually used outside of pgtypes.c, but let's be consistent and declare them in pgtypes.h This fixed by Michael Paquier.

- **Use the PG_CONFIG setting from ./configure cmd line in regression tests.**

If compiling without libpq (configure --without-libpq), then you still need to have pg_config in path to build the regression tests, or specify PG_CONFIG at the make command line.

- **Add more test cases.**

Add a new test case to test ODBC functions deprecated in ODBC 3.0, ODBC catalog functions, SQLGetConnectOption and Avoid deprecated SQLAllocStmt/SQLFreeStmt in regression tests. and more. Per Michael Paquier

- **Don't clear error number when SQLGetDiagRec is called.**

Calls to SQLGetDiagRec are supposed to be nondestructive, per ODBC spec. Also add a test case for that.

- **Fix two UseDeclareFetch bugs.**
- NOTICE messages were not delivered to the application, if they arrived as response to the DECLARE CURSOR statement.
- Array-bound parameters on SELECT-queries caused a "cursor already open" error.

These bugs were found by running the regression suite with UseDeclareFetch=1. It's now clean.

- **Fix crash if connection is closed during CC_send_query_append function.**

This fixed Malcolm MacLeod

## 09.03.0210

- **SSL verify\[-(ca|full)\] is available since 8.4.**

There seems no need to check it. Also there's no need to call lt_dlopen currently.

- **Update EXTRA_DIST in Makefile.am**

Files for the new lfconversion test case were missing.

- **Fix locking in SC_set_prepared**

added ENTER/LEAVE_CRIT_CS calls in SC_set_prepared.

- **Add test case for CTE queries (WITH ...)**

None of the existing test cases covered that. This test case gives a different e rror message than the original one, but it's the same underlying issue.

- **Silence compiler warning.**

SQLGUID format and conn_settings cannot be NULL.
This fixed by Michael Paquier.

- **Fixed POSTGRES_RESOURCE_VERSION variable.**

Pass the content of POSTGRES_RESOURCE_VERSION variable to the resource compiler correctly so that FileVersion and ProductVersion are properly set.

- **Change regression test positioned-update**

Use "exit(1);" instead of "return;" to exit from main().
This fixed by Michael Paquier, at least some compiler on OS X didn't like it.

## 09.03.0200

- **Set TCP keepalive by default.**
- **Fix cursors test case on big-endian systems.**

The SQL_CURSOR_COMMIT_BEHAVIOR property is a SQLUSMALLINT, not SQLUINTEGER. On a little-endian system, you wouldn't notice, provided that the target variable was initialized to 0 before the SQLGetInfo call.
Per report from Christoph Berg that the cursors test was failing on mipsel and other big-endian architectures.

- **Add regression test for SQLBindCol.**

We were already doing SQLBindCol as part of the positioned-update test, but seems good to have one explicitly for it.

- **The driver takes SQL_C_LONG to mean SQLINTEGER rather than "long".**

The regression test was failing on the s390x architecture because of that.
It's big-endian, with sizeof(long) == 8.

- **Revert "When LF-\>CR+LF conversion causes an buffer truncation, suppress the conversion (in case of unicode)."**
- **Refactor utf8_to_ucs_lf.**

A macro is difficult to debug, so turn it into a regular function.
Also, add a new test case for LF-\>CR+LF conversion, to test the bug that Nils Go"sche reported (which was already fixed).

- **Add locking to SQLFreeStmt and SQLFreeHandle.**

This fixes a race condition, where SQLFreeStmt is called while the connection is busy executing another statement.
SC_set_prepared would see that the connection is busy (CONN_EXECUTING), and not issue a DEALLOCATE statement to free the prepared statement in the backend, leaking it.

- **The 2dn argument of SC_set_prepared() is int(enum) not BOOL.**
- **Change to supply non-NULL parameters for SC_set_error().**

## 09.03.0100

- ** Fix uninitialized use of 'allocbuf' variable, in case of out-of-memory.**

Compiler warned about this. If ENLARGE_NEWSTATEMENT macro ran out of memory, it would jump to cleanup routine.
The cleanup would check if allocbuf isNULL, and try to free() it if not.
allocbuf needs to be initialized to NULL before the first ENLARGE_NEWSTATEMENT macro invocation.

- ** SQL_C_SLONG stands for SQLINTEGER not long.**
- ** Change SQL_ATTR_PARAMS_PROCESSED_PTR attribute which is set by SQLSetStmtAttr() from (SQLUINTEGER \*) to (SQLULEN \*).**

This fixes the bug reported by Christopf Berg.
Also verify similar attributes which were changed from SQL(U)INTEGER (\*) to SQL(U)LEN (\*) when 64bit ODBC was introduced.

- ** Reduce the memory usage of ConnectionClass objects by changing their large fixed length text fields to variable ones.**

Because changes are applied to percent-encoded fields this time, password field is also a target of this.

- ** Revise MSDTC support.**

Remove pointlessly complicated AsyncThreads stuff. Instead use _beginthread() to clean up threads.
Make pgenlist.dll from the structure change of ConnectionClass.
The driver dlls exports the functions described in connexp.h which are used by pgenlist.dll.
Isolate the current communication path if necessary.
While an IAsyncPG object is alive, a ConnectionClass object (hereinafter referred to as conn-obj) is assigned to it.
The assignment has to be changed in the following cases.

- SQLDisconnect() is called for the current connection handle which is assigned to an IAsyncPG object.

Allocate another conn-obj and move the current communication path (\*sock\* member of the current conn-obj) to the new conn-obj. The communicaation path is lost from the current conn-obj and the new conn-obj is assigned to the IAsyncPG object.

- Another (global) transaction is about to begin but the current global transaction is not PREPARED yet.

Same as case a) but will open a new communication path for the current conn-obj for the subsequent ODBC API calls.

- Another (global) transaction is about to begin and the current global transaction is already PREPARED.

Allocate another conn-obj and open a new communication path for the conn-obj. The new conn-obj is assigned to the IAsyncPG object only to issue COMMIT/ROLLBACK PREPARED command. communication pass (\*sock\* member of the current ConnectionClass object) to the new object and change the state of the current object NOT CONNECTED. The IAsyncPG object uses new object instead of the current object. In case b) the current object will open a new communication path.

- ** Ignore automatically-generated files in source code with .gitignore.**

This is useful to prevent accidental commit of files that are not wanted in the remote repository.
The original patch was provided by MichaelPaquier. I also added files genearated by VC build tools or WIX tools to .gitignore.

- ** The first cut of psqlodbc setup project. It builds a setup program which can't be done by a single MSI. **

It would install VC++ redistributable, 32 bit psqlodbc driver and 64 bit psqlodbc driver (on 64 bit windows).

- **Change the configure.ac **
- Change checking SIZEOF_LONG not SIZEOF_LONG_INT.
- with_xxxxx (xxxxx is the package name) variables directly instead of the withval variable.
- Improve the help message of configure script.
- Stop linking lib(i)odbc library because the library is unnecessary and rather harmful.

Per report from Pavel Raiskup(postgresql.org/message-id/1769926.65KUyECjFr@nb.usersys.redhat.com).

- Move -Wall -Wno-pointer-sign CFLAGS option specified in Makefile.am to configure.ac.
- ODBC_CONFIG is set when neither unixODBC nor iODBC is explicitly specified.
- ** Silence misc compiler warnings. Also a few comment typo and whitespace fixes.**
- ** CC_Copy is needed if _HANDLE_ENLIST_IN_DTC_ is used, regardless of CLEANUP_CONN_BEFORE_ISOLATION.**

That caused compilation on Windows to fail.

- ** Remove some dead code.(SOCK_clear_error, SOCK_skip_n_bytes)**
- ** Handle turning standard_conforming_strings to off in mid-session.**

We already watched for a ParameterStatus response indicating that standard_conforming_strings was turned on, and acted accordingly, but if it was turned off, we did nothing.

- ** Remove common.o on "make clean"**
- ** Change the default for UseServerSidePrepare to 1.**

The docs have recommended UseServerSidePrepare=1 for server versions 7.4 onwards, so it seems prudent to change the default so that people don't needto remember to specify it manually.
With UseServerSidePrepare=1, the "insertreturning" regression test case behaves better, ie.
SQLNumResultCols() correctly returns the number of columns for an INSERT RETURNING statement, even when called before SQLExecute().

- ** Remove useless 'sync' parameter from prepareParameters() function. It was always passed as TRUE.**
- ** Don't issue a BEGIN when running VACUUM in auto-commit mode.**

Normally in auto-commit mode the driver begins a new transaction implicitly at the first statement, by sending a BEGIN statement. However, some commands, like VACUUM, cannot be run in a transaction block, and you will get an error like "VACUUM cannot run inside a transaction block" from the server. In UseServerSidePrepare=0 mode, the code looks at the first word of the query to determine if the statement is one of the special ones, and if so, didn't begin a new transaction even when auto-commit mode is disabled. However, in UseServerSidePrepare=1 mode, when using SQLPrepare/ SQLExecute to run the VACUUM, that check was not made. Fix that. There was one more related inconsistency between UseServerSidePrepare modes. Without server-side-prepares, if you issued an explicit BEGIN in auto-commit mode, the implicit BEGIN was ont sent. But without server-side prepares, it was. It seems best to send the implicit BEGIN in both cases, because then you get a warning from the backend about the second BEGIN. That's a good thing, because a sane ODBC application should be using the ODBC function SQLEndTran() for transaction control, not explicit BEGIN/COMMIT.

- ** When LF-\>CR+LF conversion causes an buffer truncation, suppress the conversion (in case of unicode).**
- ** Handle SSL client certificate authentication in Windows Schannel security support provider.**

You have to place the certificate file postgresql.pfx (PFX or PKCS12 format) in %APPDATA%\\postgresql folder instead of postgresql.crt and postgresql.key (I wasn't able to find the way to handle PEM format using Cryptography API unfortunately).
You can create the file using the following command.
openssl pkcs12 -export -in postgresql.crt -inkey postgresql.key -out postgresql.pfx (with empty password).

- ** Change Windows installer build system for the next release.**

Change to specify ProductCode for each version so as not to forget to change ProductCode in case of major version up. Remove VC runtime merge modules from the installer(runtime dlls will be installed via psqlodbc setup program).
\[64bit version only\] Use libpq by default Remove GSSAPI support by default

- **Concentrate the settings of Windows build system to an xml file and build binaries or installers with reference to it.**

(configuration.xml) xml file to specify the setting of the build environment. It is automatically generated in winbuild folder as a copy of winbuild/configuration_template.xml when you invoke editConfiguration script firstly.
.winbuild/configuration.ps1 modules to handle the configuration file
.winbuild/editConfiguration.ps1 edit configuration file (GUI)
.winbuild/editConfiguration.bat same as above with a minimized console window
.buildx86.ps1 build 32bit binary
.buildx64.ps1 build 64bit binary
.installer/buildx86-installer.ps1 build 32bit installer
.installer/buildx64-installer.ps1 build 64bit installer

- ** handle private keys of PEM form using CryptoAPI.**

Certificates of PFX form are no longer needed for SSL client certificate authntication.

- **Fix a bug in CC_copy_conninfo() that free()s password item of input source unexpectedly.**

Change CC_copy_conninfo() and copy_globals() so that they copy each item one by one.

- **Fix the newly-introduced CORR_STRCPY and CORR_VALCPY macros.**

They were broken, did not compile. This fixed by Michael Paquier.

- **Add support for verify-ca/full sslmode using Windows Schannel Security Service Provider.**

Root CAs must be installed into Windows Root certificate store beforehand.

- ** VOID is not defined on all systems. Use void instead.**

This fixed by Christoph Berg.

- ** winres.h instead of afxres.h.**

There are some cases (VS 2012 express etc) when afxres.h doesn't exist.

- ** Avoid double-free() bug.**

Spotted by Fortify static analysis tool.

- ** Escape double-quotes in table name correctly.**

When constructing the select/update/delete for current row in a rowset, and the schema or table name contained double-quotes, they were not correctly escaped in the constructed SQL statement. That lead to errors when doing positioned updates. Also, use snprintf and snprintf_add instead of sprintf and strcat in more places. Makes these things look less like buffer overflows to static analysis tools, and make for more readable code anyway.

- ** Construct ctid string correctly for block no \> 2^31.**

Block number is unsigned. Other places where we construct ctid strings we got this right, but not this one. I'm not sure how to trigger this codepath, but I'm sure something funny would happen if you tried to use these functions on large enough tables (\> 16 TB).

- ** Fix buffer overflow in interval parsing.**

Flagged by Fortify static analysis tool.

- ** Check return value of stdup() for out-of-memory.**

There are a lot of little bugs like this throughout the code, but it's start...

- ** Replace calls to my_strcat and my_strcat with snprintf_add.**

my_strcat didn't check for buffer overflow, which Fortify static analysis tool flagged as an issue.

- ** Fix buffer overflow in handling of SQLTables params.**
- ** Use snprintf() instead of sprintf() for safety.**

I believe these instances were in fact safe, because a cursor name has a maximum length.

- ** bufferoverflowu.lib seems no longer needed in recent versions of VC environment.**

removed it from win64.mak. Speocify CUSTOMLINKLIBS=bufferoverflowu.lib from the command line when it is necessary in old VC environment.

- ** Fix memset() call, meant to clear the whole struct.**

Per compiler warning.(psqlodbc.c)

- ** socket: speedup also ipv6 connection**

Call getaddrinfo() with AI_NUMERICHOST if it is ipv4/ipv6 address. For that reason the inet_pton() is better than inet_addr().
This fixed by Pavel Raiskup

- **Call getaddrinfo() with AI_NUMERICSERV because the service is a port number.**
- **Fixed lost mylog.h in Makefile.am.**

This fixed by Pavel Raiskup

## 09.02.0100

1.) Protect shared connection list when making cleanup tasks in EN_Destructor using the patch provided by Michael Kocherov.
2.) Fix a lot of compiler warnings. Most of them were harmless, but some pointed to real, but rare, bugs.
3.) Check SIZEOF_LONG_INT, rather than SIZEOF_VOID_P, when deciding whether SQLROWSETSIZE exists. That's consistent with unixodbc's sqltypes.h.
4.) Check BUILD_LEGACY_64_BIT_MODE instead of BUILD_REAL_64_BIT_MODE. This makes us choose correctly whether we're building a 32-bit or 64-bit version, when building with unixODBC version 2.2.13 or higher.
5.) Fix handling of some out-of-memory situations.
6.) Eliminate recursion when freeing result sets of queries with array-bound parameters. This avoids running out of stack space on SQLFreeHandle, and speeds it up considerably.
7.) Fix memory leak, e.g when SQLNumResultCols is called on a non-SELECT query. This was caused by unintentional multiple evaluation of macro arguments.
8.) Move psqlodbc website's main page, FAQ, and howto pages to a separate git repository. They are no longer included in psqldbc release tarballs.
9.) Fix bug with binding a 5-bytes long string as VARCHAR parameter, with UseServerSidePrepare=0 and BoolsAsChar=1. That combination produced an extra empty result set on execution.
10.) Make the MSI build process of 32bit drivers available in 64bit.
11.) Fix bug with DeUseDeclareFetch=1 and UseServerSidePrepare=1, issue COMMIT commands properly in case of AUTOCOMMIT on mode. per reported by Jack Wilson(ljwilson@digitalav.com).
12.) Don't #include errno.h on Windows. VC10 or later intentionally changed the values of error numbers like EINTR, EWOULDBLOCK.
13.) Allow special characters in the password value of a connection string by enclosing the value by braces({}).
14.) Fixed conflict between winsock2.h and winsock.h in socket.c.
15.)Apply blank date \<-\> null conversion functionality to timestamp type as well.
16.) Fix the \*cursor XXXXXXXX already exists\* error when handling \*with cte\* statements reported by Joe Conway.
17.) Fixed the problem \*Segmentation Fault in Postgres server when using psqlODBC\* reported by Joshua Berry.
18.) Fixed the problem \*UseDeclareFetch=1, Fetch=100, UseServerSidePrepare=1 causes Windows client to intermittently hang\* reported by Jack Wilson.

## 09.01.0200

1.) Use int instead of size_t when condition \>=0 is used.
2.) Add a necessary break in a switch statement etc.
3.) Fix missing constant(SQL_ATTR_PGOPT_FETCH).
4.) Fix a bug about reference count handling for columns info (Bug report by B.Goebel).
5.) Improve the handling of ARRAY type.
6.) Fix a compilation error etc when MULTITHREAD support is disabled.
7.) Don't discard the result of unnamed statements for later SQLDescribeCol or SQLColAttribute calls.(Bug report by Alexandre).
8.) Set rowstart_in_cache properly when closing eof cursors.(Bug report by Alexandre).
9.) Remove columns info of dropped tables ASAP.
10.) Fix the bug which causes a segfault in SQLSpecialColumns when table name is null string (bug report by Terrence Enger).
11.) Fix the bug that when the show OID column option is enabled, SQLColumns() returns "oid" column info even when the column name parameter which is different from "oid" is specified (bug report from Seifert, Jan-Peter).
12.) Take care of an environment variable PGKRBSRVNAME.

## 09.01.0100

1.) SQLStatistics() sets 'D' for the collation column when an reverse index is used.
2.) Fix the bug that PostgreSQL's function calls in queries cause a crash on SQL Server linked servers.
3.) Fix the bug that the first fetch operation doesn't work properly when the operation is SQL_FETCH_ABSOLUTE or SQL_FETCH_LAST.
4.) Revise the handling of decimal point.
5.) Adjust the operations after closing eof-detected cursors.
6.) Before dropping the statement, sync and discard the response from the server for the pending extended query.
7.) Clear col_info(columns info) cache when DROP/ALTER TABLE is called so that later SQLDescribeCol() etc work properly.
8.) Improve the handling of BYTEA type.
9.) Don't clear the columns cache info when they are referenced.
10.) Correct the column size of interval types.

## 09.00.0310

1.) Let SC_forget_unnamed() clear the result in case the statement is not executed yet. It would fix the bug reported by Silvio Brandani.
2.) Fix the bug that strings are copied to a null pointer.
3.) Don't clear the parsed plan in SC_recycle_statement().
4.) Now SQLDescribeCol() can detect the changes of column type or size.
5.) Handle \*with or without oids\* correctly.
6.) Take the environment variable into account.
7.) Use SOCK_get_next_n_bytes() instead of SOCK_get_next_byte().
8.) Add some driver specific options for SQLSetConnectAttr().
9.) Fix compilation errors in case ODBC 2.5.
10.) Fix compilation error on different ODBCVER.
11.) Correct the behavior of SQLSetConnectAttr() for the driver specific options.

## 09.00.0300

1.) Don't propgate the connection level statement options to the internal statements. This fixes an infinite loop reported by Nelson Andre.
2.) Improved a mylog output.
3.) Fix the bug introduced by the previous change reprted by Adrien de Croy.
4.) Divide SC_returns_rows() macro into several categories and make SQLResultNumCols(), SQLDescribeCol() or SQLColAttribute() available
for insert/update/delete .. returning statements.
5.) Initialize flags member in QResultClass before calling QR_set_rowstart_in_cache() per report from Arno Moore.
6.) Applied a patch by Taizo Ito and correct the length of BIT type in Postgres.
7.) Change to read and skip the rest of result data when out of memory occurs while reading tuples so that we can continue to use the connection.
8.) Fix the bug that Access autonumber fields are not detected in 9.0.0200 reported by Arnaud Lesauvage.
9.) Convert large integer strings properly.
10.) Fix the trouble introduced by the change \*Return 0 for the column size when the size of numeric items are unknown\* per report from Marco Gaiarin.
11.) Remove a meaningless if clause. This had a problem with ltdl function.

## 09.00.0200

1.) Fix a bug about sizeof() at idx_fake_oid of info.
2.) Return 0 for the column size when the size of numeric items are unknown.
Also improve the calculation of scale of numeric items when it is not explicitly specified.
3.) Change to use 'if' instead of an inappropriate 'else if' at cursor of convert.
4.) Remember to set permanent flag to holdable cursors on commit with no precedent rollback (to savepoint) operations.
This fixes a crash bug in case of cursor operations across transaction.
5.) Fix a bug about the handling of the SQL_RETRIEVE_DATA option.
Per report by Andreas.
6.) Fix cursor bug introduced by the previous change.
Per report by Andreas.
7.) Move the setting of folder name of libpq or gssapi related dlls from .wxs file to .bat file.
8.) Fix a bug which returns incorrect values for SQL_DATETIME_SUB.

## 09.00.0101

1.) Fix build source of Windows 64bit environment.

## 09.00.0100

1.) Allow password which contains special characters like {,},=,;.
2.) Add a new data source option which makes it possible to use Kerberos for Windows library to reply to GSSAPI authentication request.
3.) Native support for SSPI Kerberos or Negaotiate service. It may be useful for the 64-bit drivers.
4.) Fix an oversight of Memory overflow handling.
5.) Removed "#define SQL_WCHART_CONVERT" which causes a trouble on some platforms.
6.) Removed the use of misused strcat_s together with snprintf_s (bug report from Jap-Peter Seifert) and use strlcat instead of strncat.
7.) Fix a bug about pre-execute behavior in case of protocol v2 or earlier.
8.) Use poll() instead of select() when it's available.
9.) Take comments or line comments in a query into account.
10.) Fix a crash bug on authentication failures.
11.) Take --without-iodbc(unixODBC) configure option into account.
12.) Apply the patch by Peter Crabtree which fixes a crash bug.
13.) Improve the handling of bools_as_char case.
14.) Fix a bug when creating a connection string.
15.) Use pg_get_expr(adbin, ..) instead of unreliable adsrc in pg_attrdef so as to know the sequence name associated with serial items.
16.) Added 64-bit version of installer files.
17.) Introduce pgtype_attr_xxxx functions which take a typmod parameter as well as a type oid parameter as an extension of pgtype_xxxx functions so that SQLColumns and SQLDescribeCol(SQLColAttrinute) could use common functions.
18.) Call PQconnectdbParams instead of PQconnectdb when it's available.
19.) Make cursor open check at transaction end a little more effective.
20.) Added code for SQL_INTERVAL support and refcursor support though they are disabled.
21.) Correct the handling of dynamic cursors so that they are substituted by keyset-driven ones.
22.) Remove a compilation error and some compiler warnings under unixdODBC 2.3 environment.
23.) Make sure the support of the backward-compatibility version of getaddrinfo() family even when _WIN32_WINNT is supplied.
24.) Update win64.mak so that gssapi support is available without libpq.

## 08.04.0200

1.) Fix a column uodatability problem reported by Tom Goodman.
2.) Display SSL mode list properly on setup dialog.
3.) Don't truncate the result of msgtowstr()/wstrtomsg().
4.) Implement ConfigDriver() function.
5.) check of the SQLLEN definition by the unixODBC version by SQLColAttribute.
6.) Take WITH cte staments into account.
7.) Fix a bug about UTF8 handling.
8.) Wait a ReadyForQuery Message after errors correctly.
9.) Fix a problem with {call procedure reported by Wolfgang Pasche.
10.) Cleanups about the handing of unnamed parsed statements and the handling of ODBC escape { .
11.)Link ws2_32.lib in case the compilation environment #defines _WIN32_WINNT and the value \>= 0x0501.
12) Added --with(out)-libpq\[=DIR} option to configure.
13) Revise autoconf/automake so that libpq/ssl header/libs are resolved at configure phase.
14) Use md5.c directly instead of win_md5.c.
15) Suppress some compiler warnings.
16) Fix memory leaks on connection failure (Shouji morimoto).
17) Suppress some compilation errors and warnings.
18) Fix SSL connection timeout.

## 08.04.0100

1.) Avoid a crash on exit when using SSL connections by resetting CRTPTO_xx_callbacks before unloding libpq.
2.) Correct the function name DiscardRollbackState pointed out by Zoltan Boszormenyi.
3.) Correct the value of INDEX_QUALIFIER column which returned by SLQSTATISTICS();
4.) Take domain types into account in SQLColumns() (Thanks to Luiz K. Matsumura).
5.) Take RESTRICT actions into account in SQLForeignKeys() (report from Farid Zidan).
6.) Fix a bug that small negative decimal values are mistaken for non-negative (bug report from Dominic Smith).
7.) Use MSG_NOSIGNAL/MSG_NOSIGPIPE option on send()/recv() to avoid crash on SIGPIPE (bug report from Brian Feldman).
8.) Remove a spurious "." with no trailing digits in timestamp representation (bug report from Brian Feldman).
9.) Rename trim() function in order to avoid conflict of function name(report from Dominic Smith).
10.) Put back the change to add \*read only\* clause for read only cursors.
11.) Improve pgtype_transfer_octet_length().
12.) Fix a bug reported by Milen Manev that SQLExec \*select for a table\* -\> SQLDescribeCol() -\> add a column to the table -\> SQLExec \*select for the table\* -\> SQLDescribeCol() for the added column causes a bad result.
13.) Use strncpy_null() instead of strncpy().
14.) Close (holdable) cursors on commit if possible.
15.) Recycle columns cache info if the size becomes pretty large.
16.) Add a 'verify-ca' and 'verify-full' to the sslmode option via libpq of version 8.4.
17.) Add a functionality to change the directory for logging.
18.) Correct the error code for communication errors.
19.) Correct the conversion between UTF-16 and UTF-8 for non UCS2 characters.
20.) Try to convert (especially connection error) messages using local conversions when they are not valid unicode characters.
21.) Add a textbox to setup dialog to change the directory for logging.
22.) Allow multiple Connsettings statements in connection string by enclosing them by braces({}).
23.) Improve the transactional control under useDeclareFetch mode.
24.) Take the platforms where char is unsigned into account per report from Alex Goncharov.
25.) Improve the handling of UUID type especially to support IMPORT or LINK in MS Access.

## 08.03.0400

1.) GUID forgot to set the value to the buffer.
2.) -Wall was taken as gcc limitation.

## 08.03.0300

1.) SQLGUID type support thanks to Jan-Willem Goossens.
2.) Fix a bug about silently adding a \*for read only\* clause.
3.) Fix a 64bit mode bug about handling of arrays of parameters.
4.) Change the implemetatin of SQLForeignKeys() for 8.3+ servers.
5.) Not commit the transaction too early in useDeclareFetch mode.
6.) Add a cursor open check for SQLPrepare().
7.) Reset the column binding information after SQLMoreResults().
8.) Save the rowset size properly for the FETCH_NEXT operation in case of \>= 3.0 drivers.
9.) Support FE/BE communications on Big Endian platform.
10.) Check strerror_r function's return type.
11.) Suppress some compiler warnings.

## 08.03.0200

1.) Fix a bug in socket which uses a socket variable.
2.) Support column alias without "as" so that links from the SQLServer work.
3.) Take ';' into account when the driver adds "for read only" clause.
4.) Use the E'.. ' notation not only in '=' expressions but also in LIKE expressions.
5.) Change to return milliseconds parts for timestamp fields.
6.) Change to return a specific sqlstate in case of multiple parameters.
7.) Fix bug of the selection not using SSL.

## 08.03.0100

1.) Correct the flow of trial of multi protocols and fix related bugs about handling of connection errors.
2.) Use SSPI service for SSL support when libpq is unavailable.
3.) Fix a bug in copy_and_convert_field() when fetching bookmark columns. This bug could occur in case no suitable? lo type is found.
4.) Improve the parse statement operation so that it detect srf in (from clause).
5.) Correct the COLUMN_LENGTH return value of SQLColumns() for varchar/bpchar type columns (Unicode driver).
6.) Change to not return database name if case of MS Query.
7.) The first cut to use Windows SSPI. The trial to use Schannel service for SSL support.
8.) Be more careful about \<for locking clause\> in UseDeclareFetch mode. "for read only" clause for read only queries for 8.3 or later servers for safety.

## 08.02.0500

1.) Correct the format of Bind message under 64bit environment.
2.) Fixed build of without OpenSSL.

## 08.02.0403

1.) Support SQLColAttribute for MS specific SQL_CA_SS_COLUMN_KEY. Some MS applications use this.
2.) Improve the check of updatability of queries by checking if they have multiple tables.
3.) Reduce the round trip overhead in FE/BE communications(especially in useDeclareFetch mode).
4.) Fix a bug in ResolveOneParam() pointed out by Rainer Bauer.
5.) Add a flag which lets SQLTables() show only accessible tables.
6.) The unicode driver now can handle utf-16 surrogate pairs.

## 08.02.0402

1.) Fix some bugs in case without MSDTC support.
2.) Refine the realloc handling.
3.) Put back the @@IDENTITY implementation so as not to use lastval().
4.) Change SQLColumns() to return correct column length in the Unicode driver.
5.) Remove the connection count limitation.
6.) Fix Protocol=7.4--1 notation (should be Protocol=7.4).
7.) Fix a typo in socket.c (bug report from Rainer bauer).
8.) Add CC_set_autocommit to psqlodbc(a).def files.
9.) Handle Standard_conforming_strings.
10.) Handle standard_conforming_strings also in case via libpq.
11.) Fix a bug which doesn't free connection list properly.
12.) Fix DelayLoadDLL was made explicit of psqlodbc.proj file.

## 08.02.0401

1.) Fix a bug which checking join is confused by CR+LF.
2.) Handle AUTOCOMMIT mode more carefully in a distributed transaction so as not to issue COMMIT unexpectedly.
3.) Allow the UNIX domain configuration (the same as libpq).
4.) Fixed the automake/autoconf scripts.(by Peter Eisentraut)

## 08.02.0400

1.) Fixed control binding of SetField problem.
2.) Fixed pgenlist.h losts to the release package.

## 08.02.0300

1.) Allow non-admin or Vista users to create log files in the home directory.
2.) Fix an index over bug which causes a crash or an unexpected result.

## 08.02.0205

1.) Append DETAIL messages to GetDiag...() messages.
2.) Use SQL_SUCCEEDED macros so as to simplify the code.
3.) Use lastval() function to replace IDENTITY on 8.1 or later servers.
4.) Remove WSAStartup() and WSACleanup() from DllMain.
5.) Load libpq from the driver's folder.
6.) Use QR_get_value_backend_int/_text() funcs instead of QR_get_value_backend_row().
7.) Improve the implementation of SQLSetPos(.., SQL_ADD/SQL_UPDATE) using the 8.2 new feature INSERT/UPDATE .. returning.
8.) Seaparate DTC code as a Delayload DLL.

## 08.02.0204

1.) Configure the combination of time.h and sys/time.h.
2.) Treat the tables in information_schema as system tables.
3.) Correct the precision of SQL_NUMERIC_STRUCT.
4.) Change the default max varchar size from 254 to 255.
5.) Reset the fields information properly in case of SQLMoreResults.
6.) Implement SQLDescribeParam() also in case of multi-command queries.
7.) Handle dollar-quotes more properly.
8.) Provide a make option to link dynamic multithread library.
9.) Set the default nullablity to TRUE.
10.) Parse command delimiters(;) more correctly.
11.) Use QR_get_value_backend_text() or QR_get_value_backend_int()
12.) instead of QR_get_value_backend_row().
13.) Apply Parse statement or disallow premature properly.
14.) Reset current_schema cache in case 'set search_path ..' command is issued.

## 08.02.0203

1) Fix a bug which forgets unnamed plans too early especially when handling large objects.
2) Don't treat characters whose value \>= 128 as alphanumeric in case of conversion of binary data to bytea.
3) Change ConfigDSN() so that it takes the options in Setup Dialog page 1 into account.
4) Simplify the memory management of statements'columns info so as to prevent memory leaks or a crash in parse_statement etc.
5) SQLTables("", SQL_ALL_SCHEMAS. "", ..) now returns a list of valid schemas.
6) SQLTables("", "", "", SQL_ALL_TABLE_TYPES) now returns a list of valid table types.
7) SQLGetInfo SQL_DATABASE_NAME now returns the database name.
8) Improve the automatic setting mechanism of the client_encoding for Windows code page 125x (ANSI drivers only).

## 08.02.0202

1) Change to use NULL indicator instead of the length buffer in SQLFetch.
2) Fix long -\> serial conversion error in MS Access.

## 08.02.0201

1) Ditinguish the indicaitor and the octet_length field of APD clcearly.
2) Handle @@IDENTITY more generally.
3) Take outer join into account so as to evaluate nullability.
4) Fix a bug about Keyset-driven cursors.

## 08.02.0200

1) Fix some bugs or warnings on 64 bit OS.
2) Avoid backend crash caused by Execute Requests for committed holdable cursors.

## 08.02.0105

1) SQLStatistics now returns indices based on expression also.
2) Add a TEST button on the setup dialog.

## 08.02.0104

1) Take 64bit mode into account for the format parameter of (s(n))printf, sscanf etc.
2) getpid() instead of _getpid()
3) SQLGetInfo(SQL_NULL_COLLATION) now returns SQL_NC_HIGH instead of SQL_NC_END
4) Remove the function SQLParamOptions from ODBC3.0.

## 08.02.0103

1) Improve the handling of connection error messages.
2) Add an option to convert empty strings to NULL.
3) Handle domain type as the basetype.
4) Fake MSS in case of not only SQLExecute but also SQLParamData.
5) Prevent SQLColumns from displaying system columns.
6) Unload delayLoaded libraries on dll detach (not on disconnect time).
7) Fix a SQLGetDiagField crash bug.
8) Fix a insertion count bug.
9) Take win98 cases into account a little.

## 08.02.0102

1) Compensate a loss of report of being Jet when using MS Access.
2) Add NULL \<-\> "" date conversion function for FOXPRO and
revise parse/describe/execute mechanism.
3) Miscellaneous type changes for 64 bit drivers.

## 08.02.0101

1) Delayload import XOLEHLP.dll so that the driver can be loaded
without the existence of the dll.
2) Fix a simple password authentication bug.
3) Fix a buffer overrun bug which prevented the connection phase
from retrying another version appropriately.
4) Handle multi-addresses of a specified host properly.
5) Refine the socket handling(connect, recv, send).
6) Display more appropriate error messages in the connection phase.

## 08.02.0100

1) Return to Unicode/ANSI driver distribution as some users still find one works better than the other for them.
2) Improve connection error message in case of invalid protocol characters.
3) Initial value has set MSDTC=no of VC6 (Support of MSDTC is only VC7 and VC8)
4) Addition which is a driver name in a resource (Unicode or ANSI)

## 08.02.0007

1) Fix some SQLPrimaryKey, SQLForeignKey bugs.
Fill PK_NAME or FK_NAME columns.
Return right update_rule values.
Remove duplicates.
2) Handle SQL_ALL_SCHEMAS parameter in SQLTables.
3) Handle unaligned indicator/length buffers more properly.
4) Fix a bug in ProcedureColumns in case of set returning functions.
5) Fix a buffer overrun in handle_notice_message (Bart Samuel)
6) Fix a output parameters problem (Bart Samuel)
7) Hold the connection lock while a rollbackable statement running.
8) Add Extra Opts option to the DSN option setupdialog.

## 08.02.0006

1) Add an option to fake Microsoft SQL Server which would improve SERIAL type handling.
2) Add support for LOGIN_TIMEOUT.
3) Improve Statement error handling about the reference of Connection error.
4) Improve the handling BIGINT type in the OSs without having strtoll().
5) Support ODBC CONVERT scalar functions in some cases.
6) Close qlog, mylog files on detach dll.
7) Improve communication performance in case of the driver's original socket.
8) Fix send a close
9) Support of IPV6.

8.2.0003-8.2.0005 was skipped by the situation.

## 08.02.0002

1) SQL injections via ODBC catalog function calls.
2) Fix SQLNumParams problem related to literal or dollar quote.
3) Call libpq in case of Kerberos authentication.
4) Add E' before literal for after 8.1 severs.
5) Return the correct error message in case of no password.
6) Take E'...' literal expression into account.
7) Fix an SQL_C_WCHAR -\> numeric type conversion problem.
8) Refine the handling of io or output parameters.
9) Refine the dollar quote handling.
10) Fix the client_encoding mismatch problem.
11) Fix for SQLStatistics returns UNKNOWN instead of real column names.

## 08.02.0001

1) updated CVS tip to the REL-07_03_ENHANCED branch
2) updated the installer for the single driver distribution
3) standardised various filenames that were previously based on build types
4) standardised the build on ODBC version 3.51

Release notes for older versions are here (release-7.3.html)

## 07.03 enhanced branch (7.3.0251 to 7.3.0261)

Releases from this point onwards are based on the REL-07_03_ENHANCED code branch of psqlODBC - please see http://archives.postgresql.org/pgsql-odbc/2006-02/msg00131.php for further details.

The following changes are based on the 07.03 codebase

```text
[7.3.0261]

1.Handle the conversion from text-like field to SQL_C_BINARY more properly.
2.Make buffer extension in SQLPutData more effective.
3.Reply SQL_CONVERT_xxxx correctly for SQLGetInfo request.
4.Reply DBMS_NAME, DBMS_VERSION correctly for SQLGetInfo request.
5.Not call show max_identifier_length in PG 7.4.
6.Change adjustLikePattern() to take literal quote(') into account.
7.Fix a bug in memory debug routine.
8.Change the multibyte encoding list to match that of the current
  official driver.
9.Change SC_clear_error() to call CC_clear_error() also.
10.Change the length values returned when they are unknown for
   SQLDescribeCol or SQLColAttribute requests.

[7.3.0261]

1.Fix the handling of *too small buffer* in the Unicode version.
2.Fix the behavior of DiscardStatementSvp which resets the status too early.
3.Fix the bug in PGAPI_ParamData which fails to count the number
  of parameters.
4.Allow blanks in password.
5.Add an option to force abbreviation of connection string.
6.Fix the handling of "PROTOCOL=x.x-x" in the connection string.
7.Fix the loadlib module of *nix.

[7.3.0260]

1. Change delay load import library mechanism(libpq,openssl)
2. Compile Optimization correspondence.
3. SQLForeignKeys ResultSet accuracy correspondence.

[7.3.0258]

1. bug fix: SQLBulkOperations rollback option.
   There was a case where a rollback was not published in SQLBulkOperations
   as option specification at the time of error generating.

[7.3.0257]

1. Fault correction of the division SQLGetData to SQL_C_WCHAR.

[7.3.0256]

1. SQLSetPos does not position to current-row to renewal of unblock cursor.
   (Data is unacquirable by SQLGetData etc.)

[7.3.0255]

1. bug fix: leak of a return value clearance
2. Correspondence Windows English Dialog.
3. Changes into the libpq link method.

[7.3.0254]

1. Setting menu of SSLMODE is added.
2. Correspondence to domain type lo (an approvement is still required).
3. bug fix: LIBPQ version cancellation request transmitting correspondence.
4. Correction of the exclusive control between threads of SQLCancel

[7.3.0253]

1. Compilation of escape usage.
   (preliminaries corresponding to a literal-escape abolition schedule)
2. Correspondence of libpq and openssl.
3. SQLProcedureColumns()RESULT_COLS row is outputted.
4. bug fix: server-side prepare the number check of parameters.
5. bug fix: parameter bind without a parameter marker is disregarded.
6. bug fix: Retrying without carrying out a select operation to EWOULDBLOCK of socket/SSL-read.
7. Abolition of abolition in manual_result mode, TupleList, and TupleNode employment

[7.3.0252]

1. Bug fix: Multi-byte employment by a table name etc.
2. The text (varchar/char) type key assignation was enabled by MS-Access.

[7.3.0251]

1. Suppression of SQLColumnPrivilege support
   It rejects using information_schema.column_privileges.
2. Binary mode is used for a bytea type parameter value setup.
   (Performs by the sequence of SQLPrepare->SQLExecute by server_side_prepare employment)
3. Coexistence of LO type and bytea type.
   When the parameter of LongVarBinary exists not by SQLPrepare->SQLExecute but by
   SQLExecDirect at the time of direct dispatching, the Parse/Describe function of
   a protocol 3 is used, the model in saver of LongVarBinary is acquired beforehand,
   and a bytea type and LO type are recognized.

[Feature of Driver7.3.025x]

1. Correspondence protocol 3
   Options, such as client encoding, are set up by the start-up packet.
   This makes the most of the mechanism of Parse/Bind/Execute.
   .The outfit of SQLDescribeParam
   .PREPARE/EXECUTE is changed from the command base to the protocol base.
   .FETCH command is changed to an Execute request as it has come out.
   .Moreover, bytea and lo type coexistence is possible.
   .Others...

2. Enhancement in declare/fetch mode (7.4 or later)
   Even if a certain renewal occurs on the way by holdable cursor exploitation,
   a commitment is possible in comfort.
   Moreover, the static update cursor was supported.
   (Only for SQLSetPos correspondence, SQLBulkOperation are un-supporting)
   Bug fix of the cursor which can be updated.

3. Rollback support of a syntactical unit at the time of an error.(8.0 or later)
   Can choose from three Transaction module,Statement module,Nothing.
   These can be specified by data source setup.

4. Output parameter support (8.1 or later)
   Corresponded to the output parameter of the function introduced by 8.1.
   (Only the return value of a function was supported before.)
```

## 08.01.0200

psqlODBC 08.01.0200 contains only documentation changes over the 08.01.0108 development build.

- Notes: psqlODBC 08.01.0108

1) fix most warnings during build in x86_64 environment
2) fixed point of failure on x86_64 boxes
3) fixed autocommit = off behaviour with starting transactions when needed
4) quick hack againist access violation with Disallow premature - this rarely used feature is still broken, but should no longer crash
5) report correct SQLSTATE in SQLGetDiagField and SQLGetDiagRec

- Notes: psqlODBC 08.01.0107

1) fix next problem with calling implicit rollback in bad time
2) detecting transaction commands into if branch which mean backend successfully done the command. add support for "start transaction"
3) fix bug when mylog output can't be created
4) fix problem with SQLSetPos
5) fix problem with column length in CC_mapping
6) fix - typelen could be less then zero not only -1

- Notes: psqlODBC 08.01.0106

1) fix problem with getting long text in unicode driver
2) fix sqlstate in case of error in statement
3) doesn't call rollback in autocommit off or in manual transaction
4) remove obsolete parameter protocol

- Notes: psqlODBC 08.01.0105

1) fix problem with rollback aborted queries
2) SQLColumns or SQLColAttribute may return bad answer when Show system tables is turned off
3) adds support for ; delimited options in ConfigDSN
4) fixes problem with binding parameters and SQLCancel without SQLFreeStmt

- Notes: psqlODBC 08.01.0104

1) add commit after set basic options in SQLDriverConnect when autocommit is turned off
2) driver doesn't fail when there isn't bind enough parameters
3) collattributes doesn't work since switch to libpq.
There is still problem with select '' as xx from table
4) SQLTables after SQLExecDirect has problem with GetData for all rows
5) grow MAXKEYLEN for SQLConfigDataSource
6) repair behaviour of more queries in one statement

- Notes: psqlODBC 08.01.0103

1) RowCount problem in SELECT
2) Server side prepare - but only first run in allocated statement
(http://pgfoundry.org/tracker/index.php?func=detail&aid=1000460&group_id=1000125&atid=538)
3) Table names repeated in BDE with usedeclarefetch=1
4) Binding parameters are now freed in SQLCancel
5) Skip fetching data for commands

- Notes: psqlODBC 08.01.0102

1) Server side prepare now works correctly.
2) A bug in SQLTables that shows up in Borland SQL Explorer in
3) Declare/Fetch mode has been fixed.
4) A number of inconsistent internal APIs have been cleaned up. These
issues only showed up on some OS's.
5) Correct value returned by SQLGetInfo for
SQL_CATALOG_LOCATION/SQL_QUALIFIER_LOCATION. This prevented use of the
driver in Visual Studio 2005.

The last four numbers will represent the build. If the last 2 digits are
both zero, a stable version is indicated. If the last two digits are
non-zero, it is a development snapshot.
So, starting with the next stable release, we will have:
08.01.0200 - Stable
08.01.0201 - Development
...
...
08.01.0299 - Development
08.01.0300 - Stable
Of course, we won't necessarily have 99 snaphosts between stable releases!

- Notes: psqlODBC 08.01.0101

This release fixes a serious problem in non-declare/fetch mode where
recordsets over the configured declare/fetch cache size were returned
incorrectly. It is advised that all users of v08.01.0100 upgrade as soon
as possible.

## 08.01.0100

This release represents a major change to the driver over previous versions;
in particular, the network code has been replaced with the PostgreSQL libpq
library. This allows us to more easily take advantage of updates and new
features that might be introduced to the PostgreSQL wire protocol, and also
allowed us to upgrade straight away to the version 3 protocol which was
previously unsupported. We were also able to add SSL support, as well as
Kerberos authentication support.

\>From this release, two versions of the driver are provided for Windows,
'PostgreSQL ANSI' which supports single and multibyte applications through
the ANSI ODBC API, and 'PostgreSQL Unicode' which provides Unicode support
through the Unicode ODBC API. On Unix systems, the driver type may be
selected via a configure option.

psqlODBC may be downloaded from http://www.postgresql.org/ftp/odbc/versions/
in source, Windows Installer, merge module, and basic zip file formats.

Please post any bug reports to the pgsql-odbc@postgresql.org mailing list.

I'd like to take this opportunity to thank all those involved with the
development, testing and bug fixing of the updated driver.
PostgreSQL Core Team

- Notes: psqlODBC 08.00.0102

This is a bug fix release
of the pre-libpq version of the driver, specifically to fix a bug
affecting Unix users
(http://gborg.postgresql.org/project/psqlodbc/bugs/bugupdate.php?1176)

## 08.00.0100

The driver is available in source form, as well as a dll, merge module
and msi installer for Windows.

Regards, Dave.

- Notes: psqlODBC 08.00.0005

This version
contains a some recent fixes, and is packaged in a new installer built
entirely using open source tools (specifically, the Wix toolkit).

- Notes: psqlODBC 08.00.0002

This version of the driver is a minor update
with a fix to the string sizing code in SQLGetInfo, and removal of the
need for tables to be in the search path or public schema for them to be
returned by SQLTables.

- Notes: psqlODBC 08.00.0001

The purpose of this version is to try to get something stable, with
proper schema/without OID support ready for the 8.0 release
(particularly the win32 release). To that end, this release is mainly a
quick cleanup of the following:

- GUI cleanup. Dialogues should behave better now and not have unlabeled
buttons or move around the screen in strange ways.

- Updateable Cursor support. This is largely experimental and is (I
believe) the cause of a least a couple of stability issues in the recent
snapshots. In this release it is marked 'experimental' on the setup
dialogue (only in English, if anyone can do the Japanese version, please
send a patch) and turned off by default.

I'd appreciate it if ppl could test it a little with Updateable Cursors
\*turned off\* and report any bugs.

## 07.05.0001

This is version 07.05.0001. For those that don't know or don't recall
how the versioning works, the major and minor number track the latest
release of PostgreSQL that the driver is expected to work with. The
revision number is a bit more complex. The first two digits are the
version number, where even numbers are development versions, and odd are
release. The last two digits are the build number.

In other words, this driver is build 01 of development version 00, and
is expected to work with PostgreSQL 7.5dev. :-) The leading zeros are an
ODBC thing...

## 07.03.0100

I'm pleased to announce the long awaited new version of psqlODBC. This
release represents a major change in the packaging and distribution of
psqlODBC. In particular the following points should be noted:

1) The build number will now be used to indicate both the release number
(first 2 digits), and snapshot build number (last 2 digits). this
release is 0100, or release 1, snapshot 0. The first development
snapshot will become 0101, the second 0102 and so on. The next official
release should be 07.03.0200.

2) With this release, the ODBC 3 driver is now the default. The
filenames, driver names and their capabilities are as follows:

psqlodbc.dll PostgreSQL ODBC3.0 Level
driver
psqlodbc25.dll PostgreSQL Legacy ODBC2.5 Level
driver
psqlodbc30w.dll PostgreSQL Unicode (Beta) Beta quality ODBC3.0
level driver with Unicode support

3) The 3 drivers are no longer self contained. psqlodbc.dll is used as
the setup dll for all three drivers, hence no matter how you
redistribute or install the drivers, you should always include this
file.

In addition, this driver fixes an annoying problem with recent builds of
PostgreSQL on Redhat systems that have a particularly long compiler
version string that caused a buffer overflow resulting in a GPF on
Windows upon connection to the database.
The driver is released under the LGPL, and can be downloaded from the
Downloads page on the website at
http://gborg.postgresql.org/project/psqlodbc/projdisplay.php

## gborg.postgresql.org(This is the past history.)

1.33 by hinoue Annotate
Thu Feb 5 09:11:44 2004 (2 weeks, 3 days ago)
Changed since 1.32: +3 -3 \[Select for Diff\]
Diffs to version 1.32 (long / unified)
Diffs ignoring whitespace to version 1.32 (long / unified)
\[7.3.0208\]
1) Change catalog functions to search 'public' schema if a user
tries to search the user named schema and finds no record to
return.
2) Set the statement status to STMT_FINISHED(not STMT_PREPARED)
if the pre execution causes an error.
3) Adjust null terminator for SQL_C_WCHAR type.
4) Change getCharColumnSize to return max size when the
statement has no result.
5) Cache the number of parameters for statements.

1.32 by hinoue Annotate
Tue Jan 6 09:58:22 2004 (46 hours, 12 minutes ago)
Changed since 1.31: +3 -3 \[Select for Diff\]
Diffs to version 1.31 (long / unified)
Diffs ignoring whitespace to version 1.31 (long / unified)
1) Correct the handling of SQL_C_ULONG (for \*nix).
2) Prevent plan deallocation errors when the transaction
is in abort status.
3) Avoid a connection failure when notice message arrives.
4) Improve the handling of mutex(critical section).
5) Correct the cursor open check.
6) Change the Unicode driver to set the NULL terminator
of SQL_C_WCHAR type data properly.
7) Add some m(re)alloc error check.
8) Add proper cleanup steps for some functions.
9) Return proper min & max scale for timestamp data type.
10) Return proper scale for timestamp data fields.
11) Fix the bug that .009 numeric generates .9 output. (George A.J)

1.31 by hinoue Annotate
Tue Dec 9 05:01:38 2003 (44 hours, 22 minutes ago)
Changed since 1.30: +3 -3 \[Select for Diff\]
Diffs to version 1.30 (long / unified)
Diffs ignoring whitespace to version 1.30 (long / unified)
\[7.3.0205\]
(for \*nix)
1. Detect -lsocket for some platforms.
2. Handle unix domain sockets.
3. EINTR safety for send/recv.
4. Set TCP_NDEALY option for inet connections.
\<1-4: thanks to Hartmut Raschick\>
5. Use recursive mutex if possible.
6. Add descriptor.c to Makefile.
(for Windows)
7. Automatic setting of client encoding for latin1(9).
(common)
8. Avoid those notorious core dumps when logging on hitting
"null" data...
\<8: thanls to Hartmut Raschick\>
9. Removed a static variable for thread-safety.
10. Improve the performance of keyset-driven fetch operation.
11. Map (var)char type to SQL type more properly.
12. Change create_params for NUMERIC type to (precision, scale).
13. Add type cast to remove some compiler warnings.

1.30 by hinoue
Wed Nov 12 05:15:21 2003 (8 days, 1 hour ago)
Changed since 1.29: +3 -3 \[Select for Diff\]
Diffs to version 1.29 (long / unified)
Diffs ignoring whitespace to version 1.29 (long / unified)
\[7.03.0204\]
1) Clear statements' cursors on commit/rollback.
2) Change SQLColumns, SQLTablePrivileges to take show_system_tables
option into account.
3) When an ODBC 2.x application working with an ODBC 3.x driver
1.calls SQLExecDirect, SQLExecute, or SQLParamData, and a searched
2.update or delete statement was executed but did not affect any
3.rows at the data source, the ODBC 3.x driver should return
4.SQL_SUCCESS. When an ODBC 3.x application working with an ODBC
5.3.x driver calls SQLExecDirect, SQLExecute, or SQLParamData
6.with the same result, the ODBC 3.x driver should return SQL_NO_DATA.

1.29 by hinoue
Mon Nov 10 02:10:07 2003 (10 hours, 1 minute ago)
Changed since 1.28: +3 -3 \[Select for Diff\]
Diffs to version 1.28 (long / unified)
Diffs ignoring whitespace to version 1.28 (long / unified)
\[7.03.0203\]
1) Add conversion from text to SQL_C_BINARY(bug report from Brad Wilson).
2) Change SQLColumns, SQLTables etc to initialize the statement handle
properly(bug report from Yutaka Tanida).
3) Improve the handling of mutex under \*nix(bug report from Steve Wampler).

1.28 by hinoue
Wed Nov 5 20:35:02 2003 (4 days, 15 hours ago)
Changed since 1.27: +3 -3 \[Select for Diff\]
Diffs to version 1.27 (long / unified)
Diffs ignoring whitespace to version 1.27 (long / unified)
\[7.03.0202\]
1) Improve SQLColAttributes(SQL_COLUMN_DISPLAY_SIZE) (bug report from Benjamin Riefenstahl).
2) Correct EN_set_odbc3 macro (bug report from tomas@nocrew.org(Tomas Skäre)).

1.27 by hinoue
Sat Oct 25 00:19:23 2003 (2 weeks, 2 days ago)
Changed since 1.26: +3 -3 \[Select for Diff\]
Diffs to version 1.26 (long / unified)
Diffs ignoring whitespace to version 1.26 (long / unified)
\[7.03.0201\]
1) Revise the handling of descriptors and implement SQLCopyDesc.
2) Handle data_at_execution columns for SQLSetPos or SQLBulkOperations.

1.26 by dpage
Wed Oct 22 09:07:25 2003 (2 weeks, 5 days ago)
CVS Tags: REL-07_03_0200
Changed since 1.25: +3 -3 \[Select for Diff\]
Diffs to version 1.25 (long / unified)
Diffs ignoring whitespace to version 1.25 (long / unified)
Version number bump.

1.25 by hinoue
Sun Oct 19 22:37:42 2003 (3 weeks ago)
Changed since 1.24: +3 -3 \[Select for Diff\]
Diffs to version 1.24 (long / unified)
Diffs ignoring whitespace to version 1.24 (long / unified)
\[7.03.0116\]
1. Fix a memory leak in case of updatable KEYSET_DRIVEN cursors.
2. Change SQLMoreResults to initialize the cursor position.
3. Hopefully, SQLDescribeCol() returns a more appropriate
error code(SQLSTATE).

1.24 by hinoue
Fri Sep 19 05:25:17 2003 (7 weeks, 3 days ago)
Changed since 1.23: +3 -3 \[Select for Diff\]
Diffs to version 1.23 (long / unified)
Diffs ignoring whitespace to version 1.23 (long / unified)
1) Correct the return info for SQLGetInfo(.., SQL_CURSOR_COMMIT
(ROLLBACK)_BEHAVIOR) call.
2) Explicitly reject the SQLBrowseConnect call.

1.23 by hinoue
Mon Sep 15 04:25:03 2003 (8 weeks ago)
Changed since 1.22: +3 -3 \[Select for Diff\]
Diffs to version 1.22 (long / unified)
Diffs ignoring whitespace to version 1.22 (long / unified)
1. Serialize the connection use for communication(multi-thread).
2. Let escape strings case-insensitive.

1.22 by hinoue
Tue Sep 2 07:25:53 2003 (2 months, 1 week ago)
Changed since 1.21: +1 -1 \[Select for Diff\]
Diffs to version 1.21 (long / unified)
Diffs ignoring whitespace to version 1.21 (long / unified)
Fix a bug when handling data_at_execution parameter array
reported by Jol Valentine \<jolv@genaware.com\>.

1.21 by hinoue
Wed Aug 27 06:17:53 2003 (2 months, 2 weeks ago)
Changed since 1.20: +3 -3 \[Select for Diff\]
Diffs to version 1.20 (long / unified)
Diffs ignoring whitespace to version 1.20 (long / unified)
1) Fix a bug on SQLSetStmtAttr(.., SQL_CONCURRENCY) reported by Shachar Shemesh.
2) Apply a patch to changes the storage type of the debug "func" variables
to be stored in the constant area of the program(Shachar Shemesh).
3) Correct the column size in case of bytea as LO.

1.20 by hinoue
Wed Jul 30 21:57:50 2003 (3 months, 1 week ago)
Changed since 1.19: +3 -3 \[Select for Diff\]
Diffs to version 1.19 (long / unified)
Diffs ignoring whitespace to version 1.19 (long / unified)
1) Distinguish the rowset size for SQLFetch(Scroll) from that for
SQLExtendedFetch.
2) Implement SQLGetDiagFieldW() for the Unicode driver.

1.19 by hinoue
Fri Jul 11 06:47:17 2003 (4 months ago)
Changed since 1.18: +3 -3 \[Select for Diff\]
Diffs to version 1.18 (long / unified)
Diffs ignoring whitespace to version 1.18 (long / unified)
Improve SQLGetDiagField so that an MFC application reports
error messages properly(bug report from Silvio Scarpati).

1.18 by hinoue
Wed Jul 9 04:09:11 2003 (4 months ago)
Changed since 1.17: +3 -3 \[Select for Diff\]
Diffs to version 1.17 (long / unified)
Diffs ignoring whitespace to version 1.17 (long / unified)
Add quotes around table names to use ::regclass in parse.c .

1.17 by hinoue
Sun Jun 29 23:51:24 2003 (4 months, 1 week ago)
Changed since 1.16: +3 -3 \[Select for Diff\]
Diffs to version 1.16 (long / unified)
Diffs ignoring whitespace to version 1.16 (long / unified)
Change SQLGetFunctions to contain ODBC3 APIs.

1.16 by hinoue
Thu May 29 22:30:33 2003 (5 months, 1 week ago)
Changed since 1.15: +3 -3 \[Select for Diff\]
Diffs to version 1.15 (long / unified)
Diffs ignoring whitespace to version 1.15 (long / unified)
1) Improve the SQL_C_VARBOOKMARK type handling.
2) Cleanup copy_and_convert_field() a little.
3) Set an error number and an error message for SQLAllocHandle(
SQL_HANDLE_DESC).

1.15 by hinoue
Tue May 27 22:26:37 2003 (5 months, 2 weeks ago)
Changed since 1.14: +3 -3 \[Select for Diff\]
Diffs to version 1.14 (long / unified)
Diffs ignoring whitespace to version 1.14 (long / unified)
Fix a bug about a bookmark handling in SQLBulkOperaitons(SQL_ADD,..)

1.14 by hinoue
Tue May 27 19:23:41 2003 (5 months, 2 weeks ago)
Changed since 1.13: +3 -3 \[Select for Diff\]
Diffs to version 1.13 (long / unified)
Diffs ignoring whitespace to version 1.13 (long / unified)
Remove date cast(::date) from the conversion of {d escape before
7.3 servers. 7.2 servers are poor in implicit cast from date -\>
timestamp without time zone.

1.13 by hinoue
Tue May 27 01:20:09 2003 (5 months, 2 weeks ago)
Changed since 1.12: +3 -3 \[Select for Diff\]
Diffs to version 1.12 (long / unified)
Diffs ignoring whitespace to version 1.12 (long / unified)
1) Correct the handling of bookmark in SQLBulkOperations.
2) Correct the calculation of a global index in SC_pos_add.
3) Cleanup global/rowset/resultset index handling a little.

1.12 by hinoue
Sun May 25 20:47:50 2003 (5 months, 2 weeks ago)
Changed since 1.11: +3 -3 \[Select for Diff\]
Diffs to version 1.11 (long / unified)
Diffs ignoring whitespace to version 1.11 (long / unified)
Correct a handling of offset in SQLBulkOperations.

1.11 by hinoue
Tue May 20 10:11:22 2003 (5 months, 3 weeks ago)
Changed since 1.10: +4 -4 \[Select for Diff\]
Diffs to version 1.10 (long / unified)
Diffs ignoring whitespace to version 1.10 (long / unified)
Fix a crash bug in SQLDriverConnect().

1.10 by hinoue
Tue May 20 06:39:28 2003 (5 months, 3 weeks ago)
Changed since 1.9: +3 -3 \[Select for Diff\]
Diffs to version 1.9 (long / unified)
Diffs ignoring whitespace to version 1.9 (long / unified)
The previous change wasn't in time of 7.3.0100.
It belongs to 7.03.0101.

1.9 by hinoue
Mon May 19 06:12:09 2003 (5 months, 3 weeks ago)
Changed since 1.8: +1 -1 \[Select for Diff\]
Diffs to version 1.8 (long / unified)
Diffs ignoring whitespace to version 1.8 (long / unified)
1) Revert a \*int8 as\* DSN option to \*numeric\* for MS Jet.
2) Add a SQL_CA2_SENSITIVITY_ADDTIONS mask to the reply for
SQLGetInfo(.., SQL_ATTR_KEYSET_DRIVEN_CURSOR_ATTRIBUTES2)
which would make a little OpenOffice happy.
3) Fix a bug in SQLGetInfo(.., SQL_ATTR_ROW_NUMBER) for keyset_driven
cursors.

1.8 by dpage
Thu May 15 04:24:40 2003 (5 months, 3 weeks ago)
CVS Tags: REL-07_03_0100
Changed since 1.7: +3 -3 \[Select for Diff\]
Diffs to version 1.7 (long / unified)
Diffs ignoring whitespace to version 1.7 (long / unified)
Updates for 07.03.0100

1.7 by dpage
Thu May 15 03:38:16 2003 (5 months, 3 weeks ago)
Changed since 1.6: +3 -3 \[Select for Diff\]
Diffs to version 1.6 (long / unified)
Diffs ignoring whitespace to version 1.6 (long / unified)
Updates for 07.03.0001

1.6 by hinoue
Mon May 12 11:44:08 2003 (5 months, 4 weeks ago)
Changed since 1.5: +3 -3 \[Select for Diff\]
Diffs to version 1.5 (long / unified)
Diffs ignoring whitespace to version 1.5 (long / unified)
1) Add a \*bytea as LO\* DSN option button.
2) Add japanese resources to psqlodbc.rc.
3) Add a dialog for the driver change.

1.5 by hinoue
Wed Apr 23 05:49:47 2003 (6 months, 2 weeks ago)
Changed since 1.4: +3 -3 \[Select for Diff\]
Diffs to version 1.4 (long / unified)
Diffs ignoring whitespace to version 1.4 (long / unified)
1) Avoid password leak in the log.
2) Reduce time() calls for the performance.
3) Improve the handling of per Driver resource.
4) #define HAVE_STRTOLL
etc.

1.4 by hinoue
Wed Nov 27 04:30:53 2002 (11 months, 1 week ago)
CVS Tags: REL-07_02_0005
Changed since 1.3: +3 -3 \[Select for Diff\]
Diffs to version 1.3 (long / unified)
Diffs ignoring whitespace to version 1.3 (long / unified)
Upgrade to 7.2.0005.

1.3 by hinoue
Thu Oct 10 02:37:54 2002 (13 months ago)
CVS Tags: REL-07_02_0004
Changed since 1.2: +3 -3 \[Select for Diff\]
Diffs to version 1.2 (long / unified)
Diffs ignoring whitespace to version 1.2 (long / unified)
1) Change SQLDescribeCol or SQLColAttribute to return right ResultNumCols for
keyset/static cursors.
2) Change PG_TYPE_TEXT -\> PG_TYPE_VARCHAR for character columns of the
result of catalog functions (bug report from Mike Mascari).

1.2 by hinoue
Mon Sep 23 04:08:22 2002 (13 months, 2 weeks ago)
CVS Tags: REL-07_02_0003
Changed since 1.1: +3 -3 \[Select for Diff\]
Diffs to version 1.1 (long / unified)
Diffs ignoring whitespace to version 1.1 (long / unified)
1) Removed most static variables for thread safety.
2) Trial implementation of the thread safe driver under
Windows.
3) Fix a bug about DSN options setting.
4) Fix a bug about Driver options setting.
5) Implement SQLProcedureColumns().
6) Changed default numeric precision/scale.
7) Fix some memory leaks in Unicode driver.
8) datetime -\> timestamp with time zone since 7.0.
9) Change manually started(using begin) transactions never
be implicitly committed.

1.1 by inoue
Thu May 9 00:10:22 2002 (18 months ago)
\[Select for Diff\]
Add a file for version control.
