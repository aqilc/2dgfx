@echo off
setlocal EnableDelayedExpansion
:: set "PATH=;%PATH%"

:: Sets up the env so `cl` is on the path
:: cl >nul
:: echo !errorlevel!
:: IF errorlevel 1 ( goto setup ) ELSE ( goto compile )

:: :setup
:: vcvars32
:: goto compile

:: :compile
mkdir bin
:: set c=clang-cl --target=i386-pc-win32 /c /MD /EHsc /O2 /D_CRT_SECURE_NO_WARNINGS /I"./deps/include/" /I"./lib/" /Fo"./bin/" main.c
:: set c=clang-cl --target=i386-pc-win32 /c /MDd /Z7 /EHsc /Od /D_CRT_SECURE_NO_WARNINGS /I"./deps/include/" /I"./lib/" /Fo"./bin/" main.c
:: set c=clang-cl --target=i386-pc-win32 /c /MD /Zi /EHsc /O2 /D_CRT_SECURE_NO_WARNINGS /I"./deps/include/" /I"./lib/" /Fo"./bin/" main.c
:: set c=clang-cl --target=x86_64-pc-win32 /c /MD /Zi /EHsc /O2 /D_CRT_SECURE_NO_WARNINGS /I"./deps/include/" /I"./lib/" /Fo"./bin/" main.c
set c=clang-cl --target=x86_64-pc-win32 /c /MDd /Z7 /EHsc /Od /D_CRT_SECURE_NO_WARNINGS /DGFX_DEBUG /I"./deps/include/" /I"./lib/" /Fo"./bin/" main.c
:: for /f "tokens=*" %%F in ('dir /b /a:-d "*.c"') do call set c=%%c%% "%%F"
:: for /f "tokens=*" %%F in ('dir /b /a:-d ".\deps\*.c"') do call set c=%%c%% ".\deps\%%F"
for /f "tokens=*" %%F in ('dir /b /a:-d ".\lib\*.c"') do call set c=%%c%% ".\lib\%%F"
:: set c=%c% /DTRACY_ENABLE .\deps\extern\tracy\TracyClient.cpp
echo running %c%
%c%

:: DEBUG:
:: lld-link freetype.lib glew32s.lib Gdi32.lib user32.lib ntdll.lib shell32.lib opengl32.lib glfw3.lib msvcrtd.lib ./bin/*.obj /nologo /DEBUG:FULL /ignore:4099 /MACHINE:X86 /LIBPATH:"./deps/lib/" /NODEFAULTLIB:libcmt.lib /OUT:main.exe /SUBSYSTEM:CONSOLE
lld-link freetype.lib glew.lib Gdi32.lib user32.lib ntdll.lib shell32.lib opengl32.lib glfw3.lib msvcrtd.lib ./bin/*.obj /nologo /DEBUG:FULL /ignore:4099 /MACHINE:X64 /LIBPATH:"./deps/lib/64bit" /NODEFAULTLIB:libcmt.lib /OUT:main.exe /SUBSYSTEM:CONSOLE

:: PROD:
:: lld-link freetype.lib glew32s.lib Gdi32.lib user32.lib shell32.lib opengl32.lib glfw3.lib msvcrt.lib ./bin/*.obj /nologo /DEBUG:FULL /ignore:4099 /MACHINE:X86 /LIBPATH:"./deps/lib/" /NODEFAULTLIB:libcmt.lib /OUT:main.exe /SUBSYSTEM:CONSOLE
:: lld-link freetype.lib glew32s.lib Gdi32.lib user32.lib shell32.lib opengl32.lib glfw3.lib zlib.lib msvcrt.lib ./bin/*.obj /nologo /MACHINE:X86 /LIBPATH:"./deps/lib/" /NODEFAULTLIB:libcmt.lib /OUT:main.exe /ENTRY:mainCRTStartup /SUBSYSTEM:WINDOWS

:: gcc -O3 -Id:\projects\libs\include\ -I"d:\Apps and Files\Apps\vcpkg\installed\x86-windows\include" main.c util.c -lfreetype -lglew32s -lshell32 -lGdi32 -luser32 -lopengl32 -lglfw3 -Ld:\projects\libs\bin

:: /ENTRY:mainCRTStartup /SUBSYSTEM:WINDOWS

:: cloc . --exclude-dir=res,.ccls-cache,.vscode

cd bin
del *.obj
cd ..
rmdir bin

main

:: cmake build: 
:: D:\projects\tools\CMake\bin\cmake.EXE --build d:/projects/rust/apcsp/cdungeon/build --config Debug --target cdungeon -j 6 --
