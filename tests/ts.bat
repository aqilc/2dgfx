@echo off
setlocal EnableDelayedExpansion

rem Test Standalone

mkdir bin
set c=clang-cl --target=i386-pc-win32 /c /MDd /Z7 /EHsc /Od /D_CRT_SECURE_NO_WARNINGS /I"../deps/include/" /I"../lib/" /Fo"./bin/" %1.c
rem for /f "tokens=*" %%F in ('dir /b /a:-d "..\deps\*.c"') do call set c=%%c%% "..\deps\%%F"
echo running %c%
%c%
lld-link freetype.lib glew32s.lib Gdi32.lib user32.lib shell32.lib opengl32.lib glfw3.lib zlib.lib msvcrt.lib ./bin/*.obj /nologo /DEBUG:FULL /ignore:4099 /MACHINE:X86 /LIBPATH:"../deps/lib/" /NODEFAULTLIB:libcmt.lib /OUT:test.exe /SUBSYSTEM:CONSOLE
cd bin
del *.obj
cd ..
rmdir bin

test
rem del test.exe
del test.pdb
