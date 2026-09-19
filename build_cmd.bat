@echo off
call "D:\ViusalStudioCommunity2026\VC\Auxiliary\Build\vcvars64.bat" -vcvars_ver=14.51.36231 >nul 2>&1
"D:\ViusalStudioCommunity2026\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build out/cfg
exit /b %ERRORLEVEL%
