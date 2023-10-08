@echo off

::We go inside the Build folder and build the solution
cd Build
cmake ..
cmake --build . --config Release
cd Release

::We then check wether it is a 64 or 32 bit computer (for the dlls)
reg query HKEY_LOCAL_MACHINE\HARDWARE\DESCRIPTION\System\CentralProcessor\0 /v "Identifier" | find "64 Family"

if %errorlevel% == 0 GoTo 64BITS
else GoTo 32BITS

::We copy the dlls into the output folder
:64BITS
if not exist SDL2.dll xcopy ..\..\SDL2\lib\x64\SDL2.dll .
if not exist SDL2_image.dll xcopy ..\..\SDL2_image\lib\x64\SDL2_image.dll .
if not exist SDL2_ttf.dll xcopy ..\..\SDL2_ttf\lib\x64\SDL2_ttf.dll .
GoTo ENDCOMPILE

:32BITS
if not exist SDL2.dll xcopy ..\..\SDL2\lib\x86\SDL2.dll .
if not exist SDL2_image.dll xcopy ..\..\SDL2_image\lib\x86\SDL2_image.dll .
if not exist SDL2_ttf.dll xcopy ..\..\SDL2_ttf\lib\x86\SDL2_ttf.dll .

::And we copy the font and sprites folders (we don't use 'if not exist' in case of changes made to them) and start the project
:ENDCOMPILE
xcopy ..\..\Fonts .\Fonts /y /i
xcopy ..\..\Sprites .\Sprites /y /i
START /W EditBMP.exe
cd ../..