@ECHO OFF
SETLOCAL
PATH=C:\usr\prog\openssl\openssl-0.9.8h-1-bin\bin\;%PATH%

REM configuration
SET CNF=-config C:\usr\prog\openssl\openssl-0.9.8h-1-bin\share\openssl.cnf
openssl.exe %* 
