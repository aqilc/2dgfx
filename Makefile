
main.exe: main.c
	mkdir bin
	cl /c /MD /EHsc /O2 /std:c17 /I"./deps/include/" /Fo"./bin/" main.c ".\deps\2dgraphics.c" ".\deps\futil.c" ".\deps\glapi.c" ".\deps\hash.c" ".\deps\hashfunc.c" ".\deps\spng.c" ".\deps\util.c"
	link freetype.lib glew32s.lib shell32.lib Gdi32.lib user32.lib opengl32.lib glfw3.lib zlib.lib ./bin/*.obj /nologo /LIBPATH:"./deps/lib/" /NODEFAULTLIB:libcmt.lib /OUT:main.exe
	cd bin && del *.obj
	rmdir bin

clean:
	del main.exe
