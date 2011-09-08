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

REM shared sources
SET SRC=util.c dbuf.c b64.c xml.c mime.c task.c net.c ^
  crypt.c log.c queue.c fileq.c filter.c ebxml.c ^
  cpa.c console.c config.c server.c

REM sender and receiver specific sources
SET SSRC=find.c fpoller.c qpoller.c ebxml_sender.c
SET RSRC=ebxml_receiver.c 

REM build targets
IF "%1" == "sender" CALL :sender
IF "%1" == "receiver" CALL :receiver
IF "%1" == "phineas" CALL :phineas
IF "%1" == "cpa" CALL :cpa
IF "%1" == "xmlb" CALL :xmlb
IF "%1" == "isapi" CALL :isapi
IF "%2" == "run" GOTO :run
REM our default build
IF "%1" == "all" GOTO :all
IF "%TARGET%." == "." ECHO Unknown target %1
GOTO :eof

REM build a sender
:sender
SET TARGET=phineass
SET BLD=-DSENDER -DSERVER -DCONSOLE %SRC% %SSRC% main.c icon.o 
GOTO :build

REM build a receiver
:receiver
SET TARGET=phineasr
SET BLD=-DRECEIVER -DSERVER -DCONSOLE %SRC% %RSRC% main.c icon.o
GOTO :build

REM build a tranceiver
:phineas
SET TARGET=phineas
SET BLD=-DRECEIVER -DSENDER -DSERVER -DCONSOLE %SRC% %SSRC% %RSRC% ^
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

:build
ECHO building %TARGET%
IF EXIST %BIN%%TARGET%.exe DEL %BIN%%TARGET%.exe
CALL cc -o %BIN%%TARGET%.exe %BLD%
GOTO :eof

:run
IF EXIST %BIN%%TARGET%.exe %BIN%%TARGET%.exe
GOTO :eof

REM build an ISAPI dll
:isapi
GOTO :eof

REM build all artifacts
:all
CALL :sender
CALL :receiver
CALL :phineas
CALL :cpa
CALL :xmlb
CALL :isapi
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

