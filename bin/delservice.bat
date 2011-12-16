@ECHO OFF
FOR /F "tokens=* usebackq" %%m IN (`sc query Phineas`) DO SET ok=%%m
IF "%ok%" == "The specified service does not exist as an installed service." GOTO :notinstalled
sc stop Phineas
sc delete Phineas
ECHO You must reboot to complete service removal of Phineas
GOTO :eof
:notinstalled
ECHO Phineas is not installed as a service
