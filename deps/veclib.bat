@echo off

rem https://www.cs.dartmouth.edu/~campbell/cs50/buildlib.html
gcc -O3 vec.c -c -o vec.o
ar -cvq vec.a vec.o
del *.o

rem https://stackoverflow.com/questions/2727187/creating-dll-and-lib-files-with-the-vc-command-line
cl /O2 /DLL /c vec.c
lib /OUT:vec.lib vec.obj
link /DLL vec.obj
del *.obj
