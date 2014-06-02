'
'	When the dll name of the driver is not of 8.3-format
'		the modification of the FileName is needed
'
' This is to work-around a bug in the WiX Toolset, see
' http://wixtoolset.org/issues/1422/
'
' We remove the short name from the filename field in the File-table
' of the two DLLs that need to be registered as ODBC drivers. Strictly
' speaking, that makes the contents of the table invalid, because a short
' name is mandatory, but Windows Installer seems nevertheless install it
' just fine.
'
option Explicit

Const msiOpenDatabaseModeTransact = 1

Const msiViewModifyInsert = 1
Const msiViewModifyUpdate = 2

Dim msiPath : msiPath = Wscript.Arguments(0)

Dim installer
Set installer = Wscript.CreateObject("WindowsInstaller.Installer")
Dim database
Set database = installer.OpenDatabase(msiPath, msiOpenDatabaseModeTransact)

Dim query
query = "Select * FROM File"
Dim view
Set view = database.OpenView(query)
view.Execute
Dim record
Set record = view.Fetch
Dim gFile, pos
Do While not record Is Nothing
gFile = record.StringData(1)
If Left(gFile, 8) = "psqlodbc" Then
	gFile = record.StringData(3)
	' Check if the FileName field is ShortName|LongName
	pos = InStr(record.StringData(3), "|")
	If pos > 0 Then
		' Omit the ShortName part
		gFile = Mid(record.StringData(3), pos + 1)
		WScript.echo record.StringData(3) & " -> " & gFile
		' And update the field
		record.StringData(3) = gFile
		view.Modify msiViewModifyUpdate, record
	End If
End If
Set record = view.Fetch
Loop

database.Commit
