#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <linux/fiemap.h>
#include <unistd.h>
#include <string.h>

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))
int main() {
   
    int fd = open("/home/host/blockdedup-offline/dedup.db" ,O_RDONLY);
    if (fd < 0) {
        perror("Failed to open file");
        return 1;
    }
   // printf("helloworld\n");
    // struct fiemap f;
    // struct fiemap* fiemap = &f;
    struct fiemap* fiemap = malloc(sizeof(struct fiemap)+ 64*sizeof(struct fiemap_extent));

    if (!fiemap) {
        perror("Failed to allocate memory for fiemap");
        close(fd);
        return 1;
    }
    //printf("%ld\n",FS_IOC_FIEMAP );
    memset(fiemap, 0, sizeof(struct fiemap));
    //fiemap->fm_start = 0;
    fiemap->fm_length = ~0ULL; // 
    //fiemap->fm_flags = 0;
    fiemap->fm_extent_count = 64;
    //fiemap->fm_reserved = 0;
    //fiemap->fm_extents = 0;

    if (ioctl(fd, FS_IOC_FIEMAP, fiemap) < 0) {
        perror("Failed to get file extent map");
        close(fd);
        free(fiemap);
        return 1;
    }

    printf("Got %d extents\n", fiemap->fm_mapped_extents);
    struct fiemap_extent* fe;
    for (size_t i = 0; i < MIN(64, fiemap->fm_mapped_extents); i++)
    {
        fe = &(fiemap->fm_extents[i]);
        printf("l->p: %lld->%lld, len: %lld\n", fe->fe_logical, fe->fe_physical, fe->fe_length);
        
    }
    
  //  printf(" fiemap fm_start %lld\n", fiemap->fm_start);
    //printf("%d\n",fd);
    close(fd);
   free(fiemap);
    return 0;
}
