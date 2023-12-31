@echo off
setlocal EnableDelayedExpansion
rem set "PATH=;%PATH%"

rem Sets up the env so `cl` is on the path
rem cl >nul
rem echo !errorlevel!
rem IF errorlevel 1 ( goto setup ) ELSE ( goto compile )

rem :setup
rem vcvars32
rem goto compile

rem :compile
mkdir bin
rem set c=clang-cl --target=i386-pc-win32 /c /MD /EHsc /O2 /D_CRT_SECURE_NO_WARNINGS /I"./deps/include/" /I"./lib/" /Fo"./bin/" main.c
rem set c=clang-cl --target=i386-pc-win32 /c /MDd /Z7 /EHsc /Od /D_CRT_SECURE_NO_WARNINGS /I"./deps/include/" /I"./lib/" /Fo"./bin/" main.c
rem set c=clang-cl --target=i386-pc-win32 /c /MD /Z7 /EHsc /O2 /DTRACY_ENABLE /D_CRT_SECURE_NO_WARNINGS /I"./deps/include/" /I"./lib/" /Fo"./bin/" .\deps\extern\tracy\TracyClient.cpp main.c
set c=clang-cl --target=i386-pc-win32 /c /MD /Z7 /EHsc /O2 /D_CRT_SECURE_NO_WARNINGS /I"./deps/include/" /I"./lib/" /Fo"./bin/" main.c
rem for /f "tokens=*" %%F in ('dir /b /a:-d "*.c"') do call set c=%%c%% "%%F"
for /f "tokens=*" %%F in ('dir /b /a:-d ".\deps\*.c"') do call set c=%%c%% ".\deps\%%F"
for /f "tokens=*" %%F in ('dir /b /a:-d ".\lib\*.c"') do call set c=%%c%% ".\lib\%%F"
echo running %c%
%c%
rem lld-link freetype.lib glew32s.lib Gdi32.lib user32.lib shell32.lib opengl32.lib glfw3.lib msvcrtd.lib ./bin/*.obj /nologo /DEBUG:FULL /ignore:4099 /MACHINE:X86 /LIBPATH:"./deps/lib/" /NODEFAULTLIB:libcmt.lib /OUT:main.exe /SUBSYSTEM:CONSOLE
lld-link freetype.lib glew32s.lib Gdi32.lib user32.lib shell32.lib opengl32.lib glfw3.lib msvcrt.lib ./bin/*.obj /nologo /DEBUG:FULL /ignore:4099 /MACHINE:X86 /LIBPATH:"./deps/lib/" /NODEFAULTLIB:libcmt.lib /OUT:main.exe /SUBSYSTEM:CONSOLE
rem lld-link freetype.lib glew32s.lib Gdi32.lib user32.lib shell32.lib opengl32.lib glfw3.lib zlib.lib msvcrt.lib ./bin/*.obj /nologo /MACHINE:X86 /LIBPATH:"./deps/lib/" /NODEFAULTLIB:libcmt.lib /OUT:main.exe /ENTRY:mainCRTStartup /SUBSYSTEM:WINDOWS

rem gcc -O3 -Id:\projects\libs\include\ -I"d:\Apps and Files\Apps\vcpkg\installed\x86-windows\include" main.c util.c -lfreetype -lglew32s -lshell32 -lGdi32 -luser32 -lopengl32 -lglfw3 -Ld:\projects\libs\bin

rem /ENTRY:mainCRTStartup /SUBSYSTEM:WINDOWS

rem cloc . --exclude-dir=res,.ccls-cache,.vscode

cd bin
del *.obj
cd ..
rmdir bin

main

rem cmake build: 
rem D:\projects\tools\CMake\bin\cmake.EXE --build d:/projects/rust/apcsp/cdungeon/build --config Debug --target cdungeon -j 6 --
