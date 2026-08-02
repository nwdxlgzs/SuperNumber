@echo off
setlocal EnableExtensions
cd /d "%~dp0"

rem ============================================================
rem SuperNumber Lab - one-click web build
rem   build.bat              full: assemble + modular + smoke + single
rem   build.bat modular      modular only + smoke
rem   build.bat single       single-html only
rem   build.bat smoke        smoke only (needs snlab.mjs)
rem   build.bat clean        delete outputs
rem   build.bat nosmoke      full without smoke
rem   build.bat assemble     only merge js/* -> app.js
rem Extra args after subcommand are forwarded to build.ps1
rem   e.g. build.bat modular -Emsdk D:\project\emsdk-3.1.69
rem ============================================================

set "PS=powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1""

if /I "%~1"=="" (
  echo [build.bat] full pipeline (assemble + modular + smoke + single-html^)
  echo [build.bat] no args needed - just double-click or run build.bat
  %PS%
  exit /b %ERRORLEVEL%
)

if /I "%~1"=="help" goto :help
if /I "%~1"=="-h" goto :help
if /I "%~1"=="/?" goto :help

if /I "%~1"=="modular" (
  shift
  %PS% -ModularOnly %*
  exit /b %ERRORLEVEL%
)
if /I "%~1"=="single" (
  shift
  %PS% -SingleOnly %*
  exit /b %ERRORLEVEL%
)
if /I "%~1"=="smoke" (
  if not exist snlab.mjs (
    echo snlab.mjs missing - building modular first...
    %PS% -ModularOnly
    if errorlevel 1 exit /b %ERRORLEVEL%
  )
  node _smoke.mjs
  exit /b %ERRORLEVEL%
)
if /I "%~1"=="clean" (
  %PS% -Clean -ModularOnly -SkipSmoke 2>nul
  del /q snlab.mjs snlab.wasm shell-single.generated.html snlab-single.html 2>nul
  echo cleaned
  exit /b 0
)
if /I "%~1"=="nosmoke" (
  shift
  %PS% -SkipSmoke %*
  exit /b %ERRORLEVEL%
)
if /I "%~1"=="assemble" (
  node tools\assemble-app.js
  exit /b %ERRORLEVEL%
)

rem Unknown first arg: forward everything to build.ps1 (advanced)
%PS% %*
exit /b %ERRORLEVEL%

:help
echo.
echo SuperNumber Lab web build
echo   build.bat            full build (no args = full, recommended)
echo   build.bat modular    modular + smoke
echo   build.bat single     single-html
echo   build.bat smoke      node _smoke.mjs
echo   build.bat clean      remove outputs
echo   build.bat nosmoke    full without smoke
echo   build.bat assemble   only merge js/* -^> app.js
echo.
echo Root shortcuts: build.bat / build_web.bat (repo root)
echo.
exit /b 0
