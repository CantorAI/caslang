@echo off
setlocal

set CANTOR_BIN=D:\CantorAI\out\build\x64-Debug\bin
set CANTOR_EXE=%CANTOR_BIN%\Cantor.exe

echo Checking if Cantor is running...
tasklist /FI "IMAGENAME eq Cantor.exe" 2>NUL | find /I /N "Cantor.exe">NUL
if "%ERRORLEVEL%"=="0" (
    echo Cantor is already running.
) else (
    echo Starting Cantor...
    start "" /D "%CANTOR_BIN%" "%CANTOR_EXE%"
    echo Waiting for Cantor to initialize...
    timeout /t 5 /nobreak
)

endlocal
