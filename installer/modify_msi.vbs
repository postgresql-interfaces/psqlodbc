'
'	When the dll name of the driver is not of 8.3-format
'		the modification of the FileName is needed
'
' This is to work-around an oversight in Windows Installer, see
' http://wixtoolset.org/issues/1422/
'
' We remove the short name from the filename field in the File-table
' of the two DLLs that need to be registered as ODBC drivers. Strictly
' speaking, that makes the contents of the table invalid, because a short
' name is mandatory, but Windows Installer seems nevertheless install it
' just fine.

Option Explicit

Const msiOpenDatabaseModeTransact = 1
Const msiViewModifyInsert = 1
Const msiViewModifyUpdate = 2
Const query = "SELECT * FROM File"

Dim installer, database
Dim view, record
Dim pos, filename

Set installer = Wscript.CreateObject("WindowsInstaller.Installer")
Set database = installer.OpenDatabase(WScript.Arguments(0), _
                                      msiOpenDatabaseModeTransact)
Set view = database.OpenView(query)
view.Execute

Set record = view.Fetch
Do While Not record Is Nothing

	filename = record.StringData(3)
	pos = InStr(filename, "|psqlodbc")

	If (pos > 0) Then

		' Remove the ShortName part
		filename = Mid(filename, pos + 1)
		WScript.echo record.StringData(3) & " -> " & filename

		record.StringData(3) = filename
		view.Modify msiViewModifyUpdate, record

	End If

	Set record = view.Fetch

Loop

database.Commit
