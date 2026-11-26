---
title: "C#"
weight: 1
aliases:
  - /howto-csharp.html
---

*Author: Dave Page (dpage@postgresql.org)\
Release Date: 12 April 2002\
Description: Example based Mini-Howto on Accessing PostgreSQL from C#*\
\
This document provides some sample code to get you started with C# & PostgreSQL.\
\
Requirements to get the code to work:

- A C# Compiler.
- The Microsoft .NET Framework.
- The Microsoft ODBC .NET Data Provider.
- A PostgreSQL datasource.

The example code shown below may need some modification to make it actually work in your environment. There is one table used in the example:

> 
>     CREATE TABLE vbtest(
>         id serial,
>         data text,
>         accessed timestamp
>     );
>     INSERT INTO csharptest(data, accessed) VALUES('Rows: 1', now());
>     INSERT INTO csharptest(data, accessed) VALUES('Rows: 2', now());
>     INSERT INTO csharptest(data, accessed) VALUES('Rows: 3', now());

## Code

> 
>
>     using System;
>     using System.Data;
>     using Microsoft.Data.Odbc;
>      
>
>     class psqlODBC_Howto
>     {
>
>       [STAThread]
>       static void Main(string[] args)
>       {
>
>         // Setup a connection string
>         string szConnect = "DSN=dsnname;" +
>                            "UID=postgres;" +
>                            "PWD=********";
>
>         // Attempt to open a connection
>         OdbcConnection cnDB = new OdbcConnection(szConnect);
>          
>         // The following code demonstrates how to catch & report an ODBC exception.
>         // To keep things simple, this is the only exception handling in this example.
>         // Note: The ODBC data provider requests ODBC3 from the driver. At the time of
>         //       writing, the psqlODBC driver only supports ODBC2.5 - this will cause
>         //       an additional error, but will *not* throw an exception.
>         try 
>         {
>           cnDB.Open();
>         } 
>         catch (OdbcException ex) 
>         {
>           Console.WriteLine (ex.Message + "\n\n" + "StackTrace: \n\n" + ex.StackTrace);
>           // Pause for the user to read the screen.
>           Console.WriteLine("\nPress  to continue...");
>           Console.Read();
>           return;
>         }
>         
>         // Create a dataset
>         DataSet dsDB = new DataSet(); 
>         OdbcDataAdapter adDB = new OdbcDataAdapter();
>         OdbcCommandBuilder cbDB = new OdbcCommandBuilder(adDB);
>         adDB.SelectCommand = new OdbcCommand(
>                                  "SELECT id, data, accessed FROM csharptest", 
>                                  cnDB);
>         adDB.Fill(dsDB);
>
>         // Display the record count
>         Console.WriteLine("Table 'csharptest' contains {0} rows.\n", 
>                           dsDB.Tables[0].Rows.Count);
>         
>         // List the columns (using a foreach loop)
>         Console.WriteLine("Columns\n=======\n");
>         
>         foreach(DataColumn dcDB in dsDB.Tables[0].Columns)
>           Console.WriteLine("{0} ({1})", dcDB.ColumnName, dcDB.DataType);
>         Console.WriteLine("\n");
>
>         // Iterate through the rows and display the data in the table (using a for loop).
>         // Display the data column last for readability.
>         Console.WriteLine("Data\n====\n");
>         for(int i=0;i to continue...");
>         Console.Read();
>       }
>     }
