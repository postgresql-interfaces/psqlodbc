---
title: "Ch"
weight: 7
aliases:
  - /howto-ch.html
---

*Author: Wayne Cheng (wayne@softintegration.com)\
Release Date: 23 September 2003\
Description: Example based Mini-Howto on Accessing PostgreSQL from Ch*\
\
Ch is a C/C++ interpreter from <http://www.softintegration.com>.\
It is designed for cross-platform scripting, shell programming, 2D/3D plotting, numerical computing, and embedded scripting.\
\
Requirements to get the code to work:

- Ch Standard Edition or Professional Edition
- Ch ODBC Toolkit.
- IODBC, UNIXODBC or Microsoft ODBC manager.
- A PostgreSQL datasource.

**How to run ODBC code in Ch**\
You should be able to use the simple.c scripts, start Ch shell first.

> 
>     % ch
>                                        Ch 
>                       Standard edition, version 4.0.0.11291 
>                   (C) Copyright 2001-2003 SoftIntegration, Inc.
>                          http://www.softintegration.com
>     /home/wcheng> cd $CHHOME/package/iodbc/demos
>     /usr/local/ch/package/iodbc/demos> ls
>     odbctest.c  simple.c
>     /usr/local/ch/package/iodbc/demos> ./simple.c
>     SQLAllocHandle() OK
>     SQLSetEnvAttr() ok
>     SQLAllocHandle() ok
>     SQLSetConnectAttr() ok
>     /usr/local/ch/package/iodbc/demos> cat simple.c
>
>     /**************************** simple.c *****************************/ 
>     #include <sqlext.h>
>     #include <stdio.h>
>
>     void ODBC_error (       /* Get and print ODBC error messages */
>         SQLHENV henv,       /* ODBC Environment */
>         SQLHDBC hdbc,       /* ODBC Connection Handle */
>         SQLHSTMT hstmt)     /* ODBC SQL Handle */
>     {
>         UCHAR   sqlstate[10];
>         UCHAR   errmsg[SQL_MAX_MESSAGE_LENGTH];
>         SDWORD  nativeerr;
>         SWORD   actualmsglen;
>         RETCODE rc = SQL_SUCCESS;
>
>         while ( rc != SQL_NO_DATA_FOUND)
>         {
>             rc = SQLError(henv, hdbc, hstmt,
>                           sqlstate, &nativeerr, errmsg,
>                           SQL_MAX_MESSAGE_LENGTH - 1, &actualmsglen);
>                          
>              if (rc == SQL_ERROR) {
>                   printf ("SQLError failed!\n");
>                   return;
>              }
>
>              if (rc != SQL_NO_DATA_FOUND) {
>                    printf ("SQLSTATE = %s\n", sqlstate);
>                    printf ("NATIVE ERROR = %d\n", nativeerr);
>                    errmsg[actualmsglen] = '\0';
>                    printf ("MSG = %s\n\n", errmsg);
>               }
>          }
>          if (hdbc != SQL_NULL_HDBC)
>          {
>             SQLFreeHandle (SQL_HANDLE_DBC, hdbc);
>          }
>          if (henv != SQL_NULL_HENV)
>          {
>             SQLFreeHandle (SQL_HANDLE_ENV, henv);
>          }
>     }
>
>     int main(void)
>     {
>         SQLHENV henv = SQL_NULL_HENV;
>         SQLHDBC hdbc = SQL_NULL_HDBC;
>         SQLHSTMT hstmt = SQL_NULL_HSTMT;
>         RETCODE  rc    = SQL_SUCCESS;
>
>         rc = SQLAllocHandle (SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv);
>
>         if (rc != SQL_ERROR)
>         {
>             printf("SQLAllocHandle() OK\n");
>             rc = SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3,0);
>             if (rc != SQL_ERROR)
>             {
>                 printf("SQLSetEnvAttr() ok\n");
>                 rc = SQLAllocHandle (SQL_HANDLE_DBC, henv, &hdbc);
>                 if ( rc != SQL_ERROR)
>                 {
>                      printf("SQLAllocHandle() ok\n");
>                      rc = SQLSetConnectAttr(hdbc, SQL_ATTR_AUTOCOMMIT, SQL_AUTOCOMMIT_OFF,0);
>                      if (rc != SQL_ERROR)
>                      {
>                            printf("SQLSetConnectAttr() ok\n");
>                            SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
>                            SQLFreeHandle(SQL_HANDLE_ENV, henv);
>                      }
>                  }
>              }
>          }
>
>          if (rc == SQL_ERROR)
>          {
>              ODBC_error (henv, hdbc, hstmt);
>          }
>     }
