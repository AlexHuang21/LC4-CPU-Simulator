all: clean trace
trace: LC4.o loader.o trace2.c
 #
 #NOTE: CIS 240 students - this Makefile is broken, you must fix it before it will work!!
 #
	clang -g LC4.o loader.o trace2.c -o trace
LC4.o:
 #
 #CIS 240 TODO: update this target to produce LC4.o
 #
	clang -c LC4.c
loader.o:
 #
 #CIS 240 TODO: update this target to produce loader.o
 #
	clang -c loader.c
clean:
	rm -rf *.o
clobber: clean
	rm -rf trace