@echo off
setlocal
cd /d "%~dp0"
"%~dp0RTSCameraTerrain.exe" --project "projects\default.rtsproject.json" %*
exit /b %errorlevel%
