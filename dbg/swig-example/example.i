// example.i

%module example

%{
extern int add(int a, int b);
extern void print_array(int* x , int size);
%}
%include "carrays.i"
%array_functions(int,intArray);
int add(int a, int b);
void print_array(int* x, int size);
