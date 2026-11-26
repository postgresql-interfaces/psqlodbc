---
title: "Visual Basic"
weight: 4
aliases:
  - /howto-vb.html
---

*Author: Dave Page (dpage@postgresql.org)\
Release Date: 13 November 2001\
Description: Example based Mini-Howto on Accessing PostgreSQL from Visual Basic*\
\
This document provides some sample code to get you started with Visual Basic & PostgreSQL.\
\
Requirements to get the subroutines to work:

- Visual Basic 5/6
- A reference in the VB project to Microsoft ActiveX Data Objects
- A PostgreSQL datasource.

The example code shown below may need some modification to make it actually work in your environment. There is one table used in the example:

> ` CREATE TABLE vbtest(`\
> `   id int4,`\
> `   data text,`\
> `   accessed timestamp`\
> `); `

## Code

> 
>
>     Sub Main()
>     Dim cn as New ADODB.Connection
>     Dim rs as New ADODB.Recordset
>      
>       'Open the connection
>       cn.Open "DSN=<MyDataSourceName>;" & _
>               "UID=<MyUsername>;" & _
>               "PWD=<MyPassword>;" & _
>               "Database=<MyDatabaseName>"
>      
>       'Clear the table
>       cn.Execute "DELETE FROM vbtest;"
>       
>       'For updateable recordsets we would typically open a Dynamic recordset.
>       'Forward Only recordsets are much faster but can only scroll forward and 
>       'are read only. Snapshot recordsets are read only, but scroll in both 
>       'directions. 
>       rs.Open "SELECT id, data, accessed FROM vbtest", cn, adOpenDynamic, adLockOptimistic
>      
>       'Loop though the recordset and print the results
>       'We will also update the accessed column, but this time access it through 
>       'the Fields collection. ISO-8601 formatted dates/times are the safest IMHO.
>       While Not rs.EOF
>         Debug.Print rs!id & ": " & rs!data
>         rs.Fields("accessed") = Format(Now, "yyyy-MM-dd hh:mm:ss")
>         rs.Update
>         rs.MoveNext
>       Wend
>      
>       'Add a new record to the recordset
>       rs.AddNew
>       rs!id = 76
>       rs!data = 'More random data'
>       rs!accessed = Format(Now, "yyyy-MM-dd hh:mm:ss")
>       rs.Update
>
>       'Insert a new record into the table
>       cn.Execute "INSERT INTO vbtest (id, data) VALUES (23, 'Some random data');"
>
>       'Refresh the recordset to get that last record...
>       rs.Requery
>
>       'Get the record count
>       rs.MoveLast
>       rs.MoveFirst
>       MsgBox rs.RecordCount & " Records are in the recordset!"
>
>       'Cleanup
>       If rs.State <> adStateClosed Then rs.Close
>       Set rs = Nothing
>       If cn.State <> adStateClosed Then cn.Close
>       Set cn = Nothing
>     End Sub

## Useful Functions

> 
>
>     ' The escapeString function can be used to fix strings for us in INSERT and 
>     ' UPDATE SQL statements. It will take a value, and return it ready for 
>     ' use in your statement as NULL, or quoted with backslashes and single quotes
>     ' escaped.
>
>     Function escapeString(vValue As Variant) As String
>
>       If IsNull(vValue) Then
>         escapeString = "NULL"
>       else
>         escapeString = "'" & Replace(Replace(vValue, "", ""), "'", "''") & "'" 
>       end if
>
>     End Function
