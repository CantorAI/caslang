@echo off
setlocal

set BUILD_SCRIPT=D:\CantorAI\build_all.bat
set STOP_SCRIPT=%~dp0stop_cantor.bat
set START_SCRIPT=%~dp0start_cantor.bat

echo ==============================================
echo Building Cantor...
echo ==============================================
pushd D:\CantorAI
call "%BUILD_SCRIPT%"
popd
if %ERRORLEVEL% NEQ 0 (
    echo Build failed!
    exit /b %ERRORLEVEL%
)

echo ==============================================
echo Restarting Cantor...
echo ==============================================
call "%STOP_SCRIPT%"
timeout /t 2 /nobreak
call "%START_SCRIPT%"

endlocal
