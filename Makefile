

default:
	swig -python f2fshack.i
	gcc -c f2fshack.c f2fshack_wrap.c -I/usr/include/python3.8/ -fPIC -g -O2
	gcc -shared f2fshack.o f2fshack_wrap.o -o _f2fshack.so -g 

clean:
	rm -rf ./*.o
