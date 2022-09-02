@echo off
cls
mkdir ..\..\build
pushd ..\..\build
cl -Zi -FC ..\handmade\code\src\win32_handmade.cpp user32.lib Gdi32.lib
popd

pause
cls