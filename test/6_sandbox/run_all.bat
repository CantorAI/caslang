@echo off
set XLANG=d:\CantorAI\out\build\x64-Debug\bin\xlang.exe

echo Running 1_echo.cas...
%XLANG% -caslang 1_echo.cas
if %errorlevel% neq 0 echo 1_echo.cas FAILED & goto :eof

echo Running 2_error.cas...
%XLANG% -caslang 2_error.cas
if %errorlevel% neq 0 echo 2_error.cas FAILED & goto :eof

echo Running 3_exit_code.cas...
%XLANG% -caslang 3_exit_code.cas
if %errorlevel% neq 0 echo 3_exit_code.cas FAILED & goto :eof

echo Running 4_python.cas...
%XLANG% -caslang 4_python.cas
if %errorlevel% neq 0 echo 4_python.cas FAILED & goto :eof

echo All tests passed!
