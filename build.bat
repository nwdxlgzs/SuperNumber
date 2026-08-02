@echo off
rem Alias: full SuperNumber Lab web build (no args = full pipeline)
call "%~dp0build_web.bat" %*
exit /b %ERRORLEVEL%
