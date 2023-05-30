

default:
	swig -python example.i
	gcc -c example.c example_wrap.c -I/usr/include/python3.8/ -fPIC
	gcc -shared example.o example_wrap.o -o _example.so