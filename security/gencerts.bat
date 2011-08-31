@ECHO OFF
SETLOCAL
PATH=C:\usr\prog\openssl\openssl-0.9.8h-1-bin\bin\;%PATH%

REM a subject
SET SUB=/O=Phineas Health/C=US/L=Madison/ST=Wisconsin/OU=Phineas Project/emailAddress=tdunnick@wisc.edu

IF NOT "%*." == "." GOTO :genself
CALL :genself shrek.pem "Shrek Server"
CALL :genself phineas.pem "Phineas Application"
GOTO :eof

REM create a self signed certificate and key
REM %1 is name, %2 is the DN
:genself
SET key=%~dpn1.key
SET cert=%~dpn1.cert
CALL :genrsa "%key%"
SET REQ=req -x509 -new -sha1 -subj
openssl.exe %REQ% "/CN=%~2%SUB%" -days 356 -key "%~dpn1.key" -out "%cert%"
copy %key% + %cert% %1
del %key% %cert%
GOTO :eof

REM create a signed certificate
:gensigned
GOTO :eof

REM generate an RSA key file
:genrsa
openssl.exe genrsa -out %1 2048
GOTO :eof
