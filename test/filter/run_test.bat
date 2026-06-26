@echo off
setlocal

set CANTOR_BIN=D:\CantorAI\out\build\x64-Debug\bin
set XLANG_EXE=%CANTOR_BIN%\xlang.exe
set TEST_SCRIPT=D:\CantorAI\caslang\test\filter\test_caslang_filter.x

echo ==============================================
echo Running Test Script...
echo ==============================================
cd /d "%CANTOR_BIN%"
"%XLANG_EXE%" "%TEST_SCRIPT%"

endlocal
