---
title: "How to specify as a connection option"
weight: 2
aliases:
  - /docs/config-opt.html
---

**How to specify as a connection option**

|  |  |  |
|----|----|----|
| **Definition** | **Keyword** | Abbreviation |
| Data source description | Description | Nothing |
| Name of Server | Servername | Nothing |
| Postmaster listening port | Port | Nothing |
| User Name | Username | Nothing |
| Password | Password | Nothing |
| Debug flag | Debug | B2 |
| Fetch Max Count | Fetch | A7 |
| Socket buffer size | Socket | A8 |
| Database is read only | ReadOnly | A0 |
| Communication to backend logging | CommLog | B3 |
| PostgreSQL backend protocol | Protocol | A1 |
| Connect Settings (commands sent to backend on connection) | ConnSettings | A6 |
| Recognize unique indexes | UniqueIndex | Nothing |
| Unknown result set sizes | UnknownSizes | A9 |
| Use Declare/Fetch cursors | UseDeclareFetch | B6 |
| Text as LongVarchar | TextAsLongVarchar | B7 |
| Unknowns as LongVarchar | UnknownsAsLongVarchar | B8 |
| Bools as Char | BoolsAsChar | B9 |
| Max Varchar size | MaxVarcharSize | B0 |
| Max LongVarchar size | MaxLongVarcharSize | B1 |
| Fakes a unique index on OID | FakeOidIndex | A2 |
| Includes the OID in SQLColumns | ShowOidColumn | A3 |
| Row Versioning | RowVersioning | A4 |
| Show SystemTables | ShowSystemTables | A5 |
| Parse Statements | Parse | C0 |
| SysTable Prefixes | ExtraSysTablePrefixes | C2 |
| Updateable Cursors | UpdatableCursors | C4 |
| LF \<-\> CR/LF conversion | LFConversion | C5 |
| True is -1 | TrueIsMinus1 | C6 |
| Datatype to report int8 columns as | BI | Nothing |
| Bytea as LO (longVarBinary) | ByteaAsLongVarBinary | C7 |
| Use serverside prepare | UseServerSidePrepare | C8 |
| Lower case identifier | LowerCaseIdentifier | C9 |
| SSL mode | SSLmode | CA |
| Extra options | AB | Nothing |
| Abbreviate(simple setup of a recommendation value) | CX | Nothing |
| TCP KEEPALIVE setting: idle time | KeepaliveTime | D1 |
| TCP KEEPALIVE setting: interval | KeepaliveInterval | D2 |
| Libpq parameters (a conninfo style string e.g. pqopt={sslcert=~/mycert sslkey=~/mykey} enclosed with braces in the case of connection strings) | Pqopt | D5 |
| Numeric As: specify the map from numeric items without precision to SQL data types. 2(SQL_NUMERIC default), 8(SQL_DOUBLE), 12(SQL_VARCHAR) or -1(SQL_LONGVARCHAR) can be specified. | NumericAS | D6 |
| Show optional error messages. | OptionalErrors | D7 |
| Chunk size when executing batches for parameterized SQL statements with arrays of parameters. | BatchSize | D8 |
| Ignore SQL_ATTR_QUERY_TIMEOUT set using SQLSetStmtAttr(). | IgnoreTimeout | D9 |

\
