@echo off
setlocal enabledelayedexpansion

set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
set "QTBIN=C:\Qt\6.11.2\msvc2022_64\bin"
set "ROOT=%~dp0"

set "PAUSE_CMD=pause"
if /i "%~1"=="-nopause" set "PAUSE_CMD=echo."

if not exist "%VCVARS%" (
    echo [错误] 找不到 vcvars64.bat
    echo        请确认已安装 Visual Studio Build Tools 2022 的 C++ 组件
    %PAUSE_CMD%
    exit /b 1
)

if not exist "%QTBIN%\qmake.exe" (
    echo [错误] 找不到 qmake.exe: %QTBIN%
    echo        请修改本脚本顶部的 QTBIN 路径
    %PAUSE_CMD%
    exit /b 1
)

echo [1/4] 载入 MSVC 环境...
call "%VCVARS%" >nul
if errorlevel 1 (
    echo [错误] vcvars64.bat 执行失败
    %PAUSE_CMD%
    exit /b 1
)

set "PATH=%QTBIN%;%PATH%"

echo [2/4] 准备构建目录...
if not exist "%ROOT%build" mkdir "%ROOT%build"
cd /d "%ROOT%build"

echo [3/4] 生成 Makefile...
qmake "%ROOT%LuoDeck.pro" CONFIG+=release
if errorlevel 1 (
    echo [错误] qmake 失败
    %PAUSE_CMD%
    exit /b 1
)

echo [4/4] 编译...
nmake
if errorlevel 1 (
    echo.
    echo [错误] 编译失败，请查看上方错误信息
    %PAUSE_CMD%
    exit /b 1
)

echo.
echo ============================================
echo   编译成功
echo   输出: %ROOT%bin\LuoDeck.exe
echo ============================================
echo.
echo 提示: 首次运行会在 exe 同目录生成 luodeck.json
echo       以及 themes\ 和 i18n\ 目录，可直接编辑自定义
echo.
%PAUSE_CMD%
