@echo off
setlocal

set "ROOT_DIR=%~dp0"
if "%ROOT_DIR:~-1%"=="\" set "ROOT_DIR=%ROOT_DIR:~0,-1%"
if "%BUILD_DIR%"=="" set "BUILD_DIR=%ROOT_DIR%\build-windows"
if "%CONFIG%"=="" set "CONFIG=Release"

echo Configuring pointmod for Windows...
cmake -S "%ROOT_DIR%" -B "%BUILD_DIR%"
if errorlevel 1 exit /b 1

echo Building pointmod...
cmake --build "%BUILD_DIR%" --target pointmod --config "%CONFIG%"
if errorlevel 1 exit /b 1

set "APP_PATH=%BUILD_DIR%\%CONFIG%\pointmod.exe"
if not exist "%APP_PATH%" set "APP_PATH=%BUILD_DIR%\pointmod.exe"

if not exist "%APP_PATH%" (
  echo Could not find built pointmod executable in "%BUILD_DIR%"
  exit /b 1
)

echo Built "%APP_PATH%"
