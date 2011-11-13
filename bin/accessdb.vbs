' delete if exists
Function DeleteFile (fname)
  On Error Resume Next
  dim filesys, dbfile
  set filesys = createobject ("Scripting.FileSystemObject")
  set dbfile = filesys.GetFile (fname & ".mdb")
  dbfile.Delete
  set dbfile = filesys.GetFile (fname & ".dsn")
  dbfile.Delete
  set dbfile = Nothing
  set filesys = Nothing
  On Error Goto 0
End Function

' create a file DSN for this database
Function CreateDSN (dbname)
  dim filesys, dsnfile, dbfile
  set filesys = createObject ("Scripting.FileSystemObject")
  set dbfile = filesys.GetFile (dbname & ".mdb")
  set dsnfile = filesys.OpenTextFile (dbname & ".dsn", 2, 1, 0)
  dsnfile.Write _
    "[ODBC]" & vbCrLf & _
    "DRIVER=Microsoft Access Driver (*.mdb)" & vbCrLf & _
    "UID=admin" & vbCrLf & _
    "UserCommitSync=Yes" & vbCrLf & _
    "Threads=3" & vbCrLf & _
    "SafeTransactions=0" & vbCrLf & _
    "ReadOnly=0" & vbCrLf & _
    "PageTimeout=5" & vbCrLf & _
    "MaxScanRows=8" & vbCrLf & _
    "MaxBufferSize=512" & vbCrLf & _
    "ImplicitCommitSync=Yes" & vbCrLf & _
    "FIL=MS Access" & vbCrLf & _
    "DriverId=25" & vbCrLf & _
    "DefaultDir=" & dbfile.ParentFolder & vbCrLf & _
    "DBQ=" & dbfile.Path & vbCrLf

  dsnfile.Close
  set dsnfile = Nothing
  set filesys = Nothing
End Function

' create a new dB
Function CreateDB (dbname)
  Dim datasource : datasource = "provider=microsoft.jet.oledb.4.0;" & _ 
  "data source=" & dbname & ".mdb"
  Dim catalog : set catalog = createobject("ADOX.Catalog")
  catalog.Create datasource
  set catalog = Nothing
  set datasource = Nothing
End Function

' run a command
Function RunSQL (dbname, strSQL)
  Dim datasource : datasource = "provider=microsoft.jet.oledb.4.0;" & _ 
  "data source=" & dbname & ".mdb"
  Dim conADO : set conADO = createobject("ADODB.Connection")
  conADO.Open datasource
  conADO.Execute strSQL
  conADO.Close
  set conADO = Nothing
  set datasource = Nothing
End Function

' add a receive queue
Function AddReceiveQ (dbname, tablename)
  RunSQL dbname, _
"CREATE TABLE " & tablename & _
"(" & _
"  recordId COUNTER," & _
"  messageId VARCHAR(255)," & _
"  payloadName VARCHAR(255)," & _
"  payloadBinaryContent IMAGE," & _
"  payloadTextContent VARCHAR(255)," & _
"  localFileName VARCHAR(255)," & _
"  service VARCHAR(255)," & _
"  [action] VARCHAR(255)," & _
"  arguments VARCHAR(255)," & _
"  fromPartyId VARCHAR(255)," & _
"  messageRecipient VARCHAR(255)," & _
"  errorCode VARCHAR(255)," & _
"  errorMessage VARCHAR(255)," & _
"  processingStatus VARCHAR(255)," & _
"  applicationStatus VARCHAR(255)," & _
"  encryption VARCHAR(255)," & _
"  receivedTime VARCHAR(255)," & _
"  lastUpdateTime VARCHAR(255)," & _
"  processId VARCHAR(255)" & _
")" 
End Function

Function AddTransportQ (dbname, tablename)
  RunSQL dbname, _
"CREATE TABLE " & tablename & _
"(" & _
"   recordId COUNTER," & _
"   messageId VARCHAR(255)," & _
"   payloadFile VARCHAR(255)," & _
"   payloadContent IMAGE," & _
"   destinationFilename VARCHAR(255)," & _
"   routeInfo VARCHAR(255)," & _
"   service VARCHAR(255)," & _
"   [action] VARCHAR(255)," & _
"   arguments VARCHAR(255)," & _
"   messageRecipient VARCHAR(255)," & _
"   messageCreationTime VARCHAR(255)," & _
"   encryption VARCHAR(10)," & _
"   signature VARCHAR(10)," & _
"   publicKeyLdapAddress VARCHAR(255)," & _
"   publicKeyLdapBaseDN VARCHAR(255)," & _
"   publicKeyLdapDN VARCHAR(255)," & _
"   certificateURL VARCHAR(255)," & _
"   processingStatus VARCHAR(255)," & _
"   transportStatus VARCHAR(255)," & _
"   transportErrorCode VARCHAR(255)," & _
"   applicationStatus VARCHAR(255)," & _
"   applicationErrorCode VARCHAR(255)," & _
"   applicationResponse VARCHAR(255)," & _
"   messageSentTime VARCHAR(255)," & _
"   messageReceivedTime VARCHAR(255)," & _
"   responseMessageId VARCHAR(255)," & _
"   responseArguments VARCHAR(255)," & _
"   responseLocalFile VARCHAR(255)," & _
"   responseFilename VARCHAR(255)," & _
"   responseContent IMAGE," & _
"   responseMessageOrigin VARCHAR(255)," & _
"   responseMessageSignature VARCHAR(255)," & _
"   priority INTEGER" & _
")" 
End Function 

' stuff to do...
' read command line params to flesh this out or call this
' stuff from the configuration web page
Dim dbname
dbname = "queues\\phineas"
DeleteFile dbname
CreateDB dbname
CreateDSN dbname
AddTransportQ dbname, "TransportQ"
AddReceiveQ dbname, "ReceiveQ"
set dbname = Nothing
