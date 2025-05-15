@echo off
setlocal EnableDelayedExpansion

rem Test Graphics Library functions

mkdir bin
set c=clang-cl --target=x86_64-pc-win32 /c /MDd /Z7 /EHsc /Od /D_CRT_SECURE_NO_WARNINGS /I"../deps/include/" /I"../lib/" /Fo"./bin/" tetris.c impls.c
rem for /f "tokens=*" %%F in ('dir /b /a:-d "..\deps\*.c"') do call set c=%%c%% "..\deps\%%F"
for /f "tokens=*" %%F in ('dir /b /a:-d "..\lib\*.c"') do call set c=%%c%% "..\lib\%%F"
rem set c=%c% /DTRACY_ENABLE ..\deps\extern\tracy\TracyClient.cpp
echo running %c%
%c%
lld-link glfw3.lib zlib.lib freetype.lib glew.lib Gdi32.lib user32.lib ntdll.lib shell32.lib opengl32.lib msvcrtd.lib ./bin/*.obj /nologo /DEBUG:FULL /ignore:4099 /MACHINE:X64 /LIBPATH:"../deps/lib/64bit/" /NODEFAULTLIB:libcmt.lib /OUT:tetris.exe /SUBSYSTEM:CONSOLE
cd bin
del *.obj
cd ..
rmdir bin

tetris.exe
