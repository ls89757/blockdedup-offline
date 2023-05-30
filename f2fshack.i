// f2fshack.c

%module f2fshack
%{
extern int open_device(char *);
extern int dedup_one_finger( int *ino_list, int *lbn_list, int *pbn_list, int size);
extern void close_with_sync();
%}
%include "carrays.i"
%array_functions(int,intArray);
int open_device(char *);
int dedup_one_finger( int *ino_list, int *lbn_list, int *pbn_list, int size);
void close_with_sync();