@echo off

cd Build
cmake ..
cmake --build . --config Release
cd Release

reg query HKEY_LOCAL_MACHINE\HARDWARE\DESCRIPTION\System\CentralProcessor\0 /v "Identifier" | find "64 Family"

if %errorlevel% == 0 GoTo 64BITS
else GoTo 32BITS

:64BITS
if not exist SDL2.dll xcopy E:\Dev\SDL2\lib\x64\SDL2.dll .
if not exist SDL2_image.dll xcopy E:\Dev\SDL2_image\lib\x64\SDL2_image.dll .
if not exist SDL2_ttf.dll xcopy E:\Dev\SDL2_ttf\lib\x64\SDL2_ttf.dll .
GoTo ENDCOMPILE

:32BITS
if not exist SDL2.dll xcopy E:\Dev\SDL2\lib\x86\SDL2.dll .
if not exist SDL2_image.dll xcopy E:\Dev\SDL2_image\lib\x86\SDL2_image.dll .
if not exist SDL2_ttf.dll xcopy E:\Dev\SDL2_ttf\lib\x86\SDL2_ttf.dll .

:ENDCOMPILE
START /W EditBMP.exe
cd ../..