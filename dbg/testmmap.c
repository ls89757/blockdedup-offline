#include<stdio.h>
#include<unistd.h>
#include<fcntl.h>
#include <sys/mman.h>
#include<string.h>
#include<assert.h>
int main(){

    int fd = open("./test", O_CREAT|O_RDWR);
    assert(fd>0);
    void* start =  mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED,fd , 0);
    memset(start, 1, 4096);
    assert(msync(start, 4096, MS_SYNC)==0);
    assert(munmap(start, 4096)==0);

}