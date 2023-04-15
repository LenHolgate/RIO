@Echo off

setlocal 

set NAME=%1%
set WORK=%2%
set EXE_PATH=%NAME%.exe

Echo Running Test: %NAME%

logman create counter test -f bin -si 01 -cf %NAME%-Counters.txt -o %NAME% -v mmddhhmm > NUL
logman start test > NUL

:: Delay for 2 seconds
choice /c delay /t 2 /d y >NUL

%EXE_PATH% %WORK%

:: Delay for 2 seconds
choice /c delay /t 2 /d y >NUL

logman stop test > NUL
logman delete test > NUL
