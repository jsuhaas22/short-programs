vm : lc3.o helper.o operations.o
	cc lc3.o helper.o operations.o -o vm

lc3.o : lc3.c helper.h global.h operations.h
	cc -c lc3.c

helper.o : helper.c helper.h global.h
	cc -c helper.c

operations.o : operations.c operations.h global.h helper.h
	cc -c operations.c

clean :
	rm lc3.o helper.o operations.o
