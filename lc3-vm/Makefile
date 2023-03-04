vm : lc3.o helper.o
	cc lc3.o helper.o -o vm

lc3.o : lc3.c helper.h
	cc -c lc3.c

helper.o : helper.c helper.h
	cc -c helper.c

clean :
	rm lc3.o helper.o
