@ECHO OFF
REM build script for TCC
SETLOCAL
REM nearly all of the compile configuration is in cc.bat except for
REM the resource compiler...
SET MINGW=C:\MinGW\bin

REM building our icon resource file if needed...
IF NOT EXIST src\icon.o CALL :genicon

REM where binarys go...
SET BIN=%~dp0bin\

REM general control over artifacts...
REM CMDLINE command line version of this program - log to stderr
REM UNITTEST generate a test version of this module

REM the following control what gets included in the build
REM __SENDER__ sender support
REM __RECEIVER__ receiver support
REM __SERVER__ server support
REM __CONSOLE__ console support
REM __FILEQ__ file based queues
REM __ODBCQ__ ODBC based queues

REM our basic build options
SET DEFS=-D__SERVER__ -D__CONSOLE__ -D__FILEQ__ -D__ODBCQ__

REM sources
SET SRC=dbuf.c util.c b64.c xmln.c xml.c mime.c task.c ^
  crypt.c net.c log.c queue.c fileq.c odbcq.c filter.c ebxml.c ^
  xcrypt.c payload.c cpa.c console.c cfg.c config.c server.c ^
  basicauth.c find.c fpoller.c qpoller.c ebxml_sender.c ^
  ebxml_receiver.c applink.c

SET OPTS=

FOR %%o IN (%*) DO CALL :option %%o
GOTO :eof

:option
SET o=%1
IF NOT %o:~0,1% == - GOTO :target
SET OPTS=%OPTS% %1
GOTO :eof

REM build targets
:target
IF "%1" == "sender" GOTO :sender
IF "%1" == "receiver" GOTO :receiver
IF "%1" == "phineas" GOTO :phineas
IF "%1" == "command" GOTO :command
IF "%1" == "service" GOTO :service
IF "%1" == "cpa" GOTO :cpa
IF "%1" == "xmlb" GOTO :xmlb
IF "%1" == "chelp" GOTO :chelp
IF "%1" == "psetup" GOTO :psetup
IF "%1" == "xcrypt" GOTO :xcrypt
IF "%1" == "isapi" GOTO :isapi
IF "%1" == "run" GOTO :run
IF "%1" == "clean" GOTO :clean
IF "%1" == "test" GOTO :test
REM our default build
IF "%1" == "all" GOTO :all
IF "%TARGET%." == "." ECHO Unknown target %1
GOTO :eof

REM build a sender
:sender
SET TARGET=phineass
SET BLD=-D__SENDER__ %DEFS% %SRC% main.c icon.o 
GOTO :build

REM build a receiver
:receiver
SET TARGET=phineasr
SET BLD=-D__RECEIVER__ %DEFS% %SRC% main.c icon.o
GOTO :build

REM build a tranceiver
:phineas
SET TARGET=phineas
SET BLD=-D__RECEIVER__ -D__SENDER__ %DEFS% %SRC% ^
  main.c icon.o
GOTO :build

REM build a tranceiver (command line)
:command
SET TARGET=phineasc
SET BLD=-DCOMMAND -D__RECEIVER__ -D__SENDER__ %DEFS% %SRC% ^
  main.c icon.o
GOTO :build

REM build a service
:service
SET TARGET=phineasd
SET BLD=-DSERVICE -D__RECEIVER__ -D__SENDER__ -ladvapi32 %DEFS% %SRC% ^
  main.c icon.o
GOTO :build

REM build cpa generator
:cpa
SET TARGET=cpa
SET BLD=-DCMDLINE cpa.c
GOTO :build

REM build xml beautifier
:xmlb
SET TARGET=xmlb
SET BLD=-DCMDLINE xmlb.c
GOTO :build

REM build config help extractor
:chelp
SET TARGET=chelp
SET BLD=-DCMDLINE chelp.c
GOTO :build

REM build config setup utility
:psetup
SET TARGET=psetup
SET BLD=-DCMDLINE psetup.c
GOTO :build

REM build xml encryption utility
:xcrypt
SET TARGET=xcrypt
SET BLD=-DCMDLINE xcrypt.c
GOTO :build

:build
ECHO building %TARGET%
IF EXIST %BIN%%TARGET%.exe DEL %BIN%%TARGET%.exe
CALL cc -o %BIN%%TARGET%.exe %OPTS% %BLD% 
GOTO :eof

:run
IF EXIST %BIN%%TARGET%.exe %BIN%%TARGET%.exe
GOTO :eof

REM build an ISAPI dll
:isapi
GOTO :eof

REM clean out bin
:clean
DEL bin\*.def > nul 2>&1
FOR %%f IN (bin\*.xml.*) DO IF NOT %%f==bin\Phineas.xml DEL %%f
GOTO :eof

REM test all sources
:test
SET ok=true
SET USRC=chelp.c psetup.c
FOR %%f IN (%SRC%) DO CALL :testone %%f
FOR %%f IN (%USRC%) DO CALL :testone %%f
GOTO :eof

:testone
IF %ok% == false GOTO :eof
call test.bat %1
IF %ERRORLEVEL%==0 GOTO :eof
ECHO %1 failed unit test!
SET ok=false
GOTO :eof

REM build all artifacts
:all
CALL :genicon
CALL :phineas
CALL :command
CALL :service
CALL :cpa
CALL :xmlb
CALL :isapi
CALL :chelp
CALL :psetup
CALL :xcrypt
CALL :clean
GOTO :eof

REM generate our icon resource using MinGW (wish tinyc had this!)
:genicon
SET P=%PATH%
PATH=%MINGW%;%PATH%
CD src
windres -O coff -i icon.rc -o icon.o
CD ..
PATH=%P%
GOTO :eof

