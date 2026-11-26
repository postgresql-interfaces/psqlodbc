---
title: "Business Objects"
weight: 6
aliases:
  - /howto-bo.html
---

*Author: Peter S. Hamlen (phamlen@teachforamerica.org)\
Release Date: 26 May 2003\
Description: PostgreSQL and Business Objects*\
\
Will Postgresql ODBC work with Business Objects?\
\
Yes, with three caveats:

1.  Technically, Business Objects does not support PostgreSQL. They do support generic ODBC access to databases, however.
2.  You must change the ODBC parameter file on any machine that uses Designer. The ODBC file is usually *C:\Program Files\Business Objects\Data Access X.X\ODBC\ODBC10EN.PRM*. You must change the OWNER parameter to N and the QUALIFIER parameter to N (ie, OWNER=N, QUALIFIER=N).\
    \
    NOTE: It is only necessary to change this file for people who use Designer. BusinessObjects, WebIntelligence, and all other BO programs that use existing Universes are unaffected.
3.  As with all other generic ODBC connections, you cannot do outer joins in Designer.

Martín de la Herrán also adds the following notes:

Also add the following parameters to C:\Program Files\Business Objects\Data Access X.X\ODBC\ODBC10EN.PRM\
\
BACK_QUOTE_SUPPORTED=Y\
TABLE_DELIMITER="\
\
These parameters are necessary to use tables with spaces and unusual chararacters in them.
