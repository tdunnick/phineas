@ECHO OFF
FOR /F "tokens=* usebackq" %%m IN (`sc query Phineas`) DO SET ok=%%m
IF "%ok%" == "The specified service does not exist as an installed service." GOTO :install
ECHO Phineas already registered as a service!
GOTO :eof
:install
ECHO installing...
sc create Phineas binPath= %~dp0phineasd.exe start= auto 
sc start Phineas
