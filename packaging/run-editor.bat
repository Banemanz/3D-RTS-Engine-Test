@echo off
setlocal
cd /d "%~dp0"
"%~dp0RTSCameraTerrain.exe" --editor --project "projects\terrain_showcase.rtsproject.json" %*
exit /b %errorlevel%
