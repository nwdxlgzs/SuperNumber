@echo off
setlocal EnableExtensions
cd /d "%~dp0"

rem One-click SuperNumber Lab web build from repo root.
rem Usage: build_web.bat   (no args = full pipeline)
rem        build_web.bat modular | single | smoke | clean | nosmoke | help

echo == SuperNumber Lab one-click web build ==
call "%~dp0webapp\build.bat" %*
exit /b %ERRORLEVEL%
