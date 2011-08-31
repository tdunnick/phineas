@ECHO OFF
SETLOCAL
REM Set the following based on your installation...
REM If any locations contain spaces, be sure to quote
REM
REM For my setup, I use the standard 0.9.25 TinyC binary zip
REM and put lib .def file I generate in their own lib folder
REM and include the psdk (from the ReactOS project) there as well.
REM
REM the compiler install location
SET TCCDIR=C:\usr\prog\tcc-0.9.25
REM the compiler base location
SET TCC=%TCCDIR%\tcc
REM library locations
SET LIBDIR=-L%TCC%\lib -L%TCCDIR%\lib
REM include locations
SET INC=-I%TCC%\include -I%TCC%\include\winapi -I%TCCDIR%\psdk ^
  -IC:\usr\prog\openssl\include 
REM 
REM the rest should just "work"...
REM librarys referenced
SET LIB=-lws2_32 -lshell32 -luser32 -lssleay32 -llibeay32 -lodbc32
REM include locations
SET INC=-I%TCC%\include -I%TCC%\include\winapi -I%TCCDIR%\psdk ^
  -IC:\usr\prog\openssl\include -Iinclude
REM compile
CD src
%TCC%\tcc.exe %LIBDIR% %LIB% %INC% %*
