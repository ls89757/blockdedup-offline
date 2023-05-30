// #include<iostream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <f2fs_fs.h>

#define BLOCKS_IN_SEG (512)
#define LOG_BLOCKS_IN_SEG (9)
#define MAX_LIST_SIZE (10)
struct stat st;
void *f2fs_start;
struct f2fs_super_block *sb_begin;
struct f2fs_nat_entry *nat_entry_begin;
struct f2fs_super_block *sb;
struct f2fs_checkpoint* cp;
struct f2fs_checkpoint* cp_e;
void *nat_bitmap;
void *sit_bitmap;

#define SEG_COUNT_IN_SIT_SEG ( BLOCKS_IN_SEG * SIT_ENTRY_PER_BLOCK ) 

#define SIT_BLOCK_OFFSET(segno)  ((segno)/SIT_ENTRY_PER_BLOCK)

int f2fs_test_bit(unsigned int nr, const char *p)
{
    int mask;
    char *addr = (char *)p;

    addr += (nr >> 3);
    mask = 1 << (7 - (nr & 0x07));
    return (mask & *addr) != 0;
}

int f2fs_set_bit(unsigned int nr, char *addr)
{
	int mask;
	int ret;

	addr += (nr >> 3);
	mask = 1 << (7 - (nr & 0x07));
	ret = mask & *addr;
	*addr |= mask;
	return ret;
}

int f2fs_clear_bit(unsigned int nr, char *addr)
{
	int mask;
	int ret;

	addr += (nr >> 3);
	mask = 1 << (7 - (nr & 0x07));
	ret = mask & *addr;
	*addr &= ~mask;
	return ret;
}

struct f2fs_nat_entry* lookup_nat(nid_t nid){
        pgoff_t block_index = NAT_BLOCK_OFFSET(nid);
        pgoff_t seg_index = block_index >> LOG_BLOCKS_IN_SEG;
        pgoff_t block_no = (sb_begin->nat_blkaddr + (seg_index << LOG_BLOCKS_IN_SEG << 1) + (block_index & ((1 << LOG_BLOCKS_IN_SEG) - 1)));
        if (f2fs_test_bit(block_index, nat_bitmap))
                block_no += BLOCKS_IN_SEG;
        // block_no 是相对于FS起始位置,nid表示条目在文件系统中处于的nat块的index
        // block_index是相对于nat表起始的块index
         struct f2fs_nat_entry *entry = (struct f2fs_nat_entry *)((void *)f2fs_start + (__u64)block_no * F2FS_BLKSIZE + (nid % NAT_ENTRY_PER_BLOCK) * sizeof(struct f2fs_nat_entry));
         return entry;
}


struct f2fs_sit_entry* lookup_sit(unsigned int ori_pbn){
    // 每个sit_entry标识一个seg的分配情况，segno表示ori_pbn数据块所在的seg编号（相对main_area开始)
    // 返回ori_pbn此block所在的sit_entry即可
    
    pgoff_t segno = (ori_pbn - sb->main_blkaddr)/BLOCKS_IN_SEG;
    pgoff_t block_index = SIT_BLOCK_OFFSET(segno);
    pgoff_t seg_index = block_index >> LOG_BLOCKS_IN_SEG;
    pgoff_t block_no = (sb_begin->sit_blkaddr + (seg_index << LOG_BLOCKS_IN_SEG << 1) + (block_index & (( 1 << LOG_BLOCKS_IN_SEG) -1 )));
    if(f2fs_test_bit(block_index, sit_bitmap))
        block_no += BLOCKS_IN_SEG;
    
    struct f2fs_sit_entry* entry = (struct f2fs_sit_entry*)((void*)f2fs_start + (__u64)block_no * F2FS_BLKSIZE + (segno%SIT_ENTRY_PER_BLOCK)*sizeof(struct f2fs_sit_entry));
    return entry;
}

int dedup_one_finger( int *ino_list, int *lbn_list, int *pbn_list, int size)
{
    assert(size > 1);
    int standard_pbn = pbn_list[0];
    for (int i = 1; i < size; i++)
    {
        int ino = ino_list[i];
        int lbn = lbn_list[i];
        int ori_pbn = pbn_list[i];
        //printf("i=%d, ino:%d lbn:%d ori_pbn:%d, standard_pbn:%d\n", i,ino,lbn,ori_pbn, standard_pbn);
        int nid = ino;
        struct f2fs_nat_entry* entry = lookup_nat(nid);
        assert(entry->ino == nid);
        assert(entry->block_addr > 0);
        struct f2fs_inode *inode = (struct f2fs_inode *)((void *)f2fs_start + ((__u64)entry->block_addr * F2FS_BLKSIZE));
        //printf("nid:%ld, ino:%d, blk_addr:%d\n", nid, entry->ino, entry->block_addr);
        
        //printf("i_name:%s\n", inode->i_name);
        // 获得block index并修改
        if(lbn < DEF_ADDRS_PER_INODE){
            // 直接索引，直接修改
            assert(inode->i_addr[lbn] == ori_pbn);
            inode->i_addr[lbn] = standard_pbn;
        }else if(lbn < DEF_ADDRS_PER_INODE + DEF_ADDRS_PER_BLOCK ){
            // 被i_nid[0]索引
            entry =  lookup_nat(inode->i_nid[0]);
          struct direct_node *dnode = (struct direct_node *)((void *)f2fs_start + (__u64)entry->block_addr * F2FS_BLKSIZE);
          assert(dnode->addr[lbn - DEF_ADDRS_PER_INODE] ==  ori_pbn);
          dnode->addr[lbn - DEF_ADDRS_PER_INODE] = standard_pbn;

        }else if(lbn< DEF_ADDRS_PER_INODE + 2*DEF_ADDRS_PER_BLOCK){
            entry = lookup_nat(inode->i_nid[1]);
            struct direct_node *dnode = (struct direct_node *)((void *)f2fs_start + (__u64)entry->block_addr * F2FS_BLKSIZE);
             assert(dnode->addr[lbn - DEF_ADDRS_PER_INODE - DEF_ADDRS_PER_BLOCK] ==  ori_pbn);
              dnode->addr[lbn - DEF_ADDRS_PER_INODE- DEF_ADDRS_PER_BLOCK ] = standard_pbn;
            // 被i_nid[1]索引
        }else if(lbn< DEF_ADDRS_PER_INODE + 2*DEF_ADDRS_PER_BLOCK + NIDS_PER_BLOCK*NIDS_PER_BLOCK){
            entry = lookup_nat(inode->i_nid[2]);
            struct indirect_node * indnode = (struct indirect_node *)((void *)f2fs_start + (__u64)entry->block_addr * F2FS_BLKSIZE);
            off_t off_in_indnode = (lbn - DEF_ADDRS_PER_INODE -2* DEF_ADDRS_PER_BLOCK)/NIDS_PER_BLOCK;
            off_t off_in_dnode =(lbn - DEF_ADDRS_PER_INODE -2* DEF_ADDRS_PER_BLOCK)%NIDS_PER_BLOCK;
            entry =  lookup_nat(indnode->nid[off_in_indnode]);
            struct direct_node* dnode = (struct direct_node*)((void *)f2fs_start + (__u64)entry->block_addr * F2FS_BLKSIZE);
           // printf("addr:%d\n", dnode->addr[off_in_dnode]);
            assert(dnode->addr[off_in_dnode] == ori_pbn);
            dnode->addr[off_in_dnode] = standard_pbn;
             assert(dnode->addr[off_in_dnode] = standard_pbn);
            // 被i_nid[2]索引
        }else if(lbn< DEF_ADDRS_PER_INODE + 2*DEF_ADDRS_PER_BLOCK + 2*NIDS_PER_BLOCK*NIDS_PER_BLOCK){
            entry = lookup_nat(inode->i_nid[3]);
            struct indirect_node * indnode = (struct indirect_node *)((void *)f2fs_start + (__u64)entry->block_addr * F2FS_BLKSIZE);
            off_t off_in_indnode = (lbn - DEF_ADDRS_PER_INODE -2* DEF_ADDRS_PER_BLOCK - NIDS_PER_BLOCK*NIDS_PER_BLOCK)/NIDS_PER_BLOCK;
            off_t off_in_dnode =(lbn - DEF_ADDRS_PER_INODE -2* DEF_ADDRS_PER_BLOCK- NIDS_PER_BLOCK*NIDS_PER_BLOCK)%NIDS_PER_BLOCK;
            entry =  lookup_nat(indnode->nid[off_in_indnode]);
            struct direct_node* dnode = (struct direct_node*)((void *)f2fs_start + (__u64)entry->block_addr * F2FS_BLKSIZE);
            assert(dnode->addr[off_in_dnode] == ori_pbn);
            dnode->addr[off_in_dnode] = standard_pbn;
            // 被i_nid[3]索引
        }else{
            // 被i_nid[4]索引,暂不支持>8G的文件
            assert(0);
        }
        // 归还block, 修改block对应的SIT, 调整cp中有有效块数
        struct f2fs_sit_entry* sit_entry = lookup_sit(ori_pbn);
        pgoff_t off_in_seg = (ori_pbn - sb->main_blkaddr)%BLOCKS_IN_SEG;
        assert(f2fs_test_bit(off_in_seg, sit_entry->valid_map)==1);
        f2fs_clear_bit(off_in_seg, sit_entry->valid_map);
        sit_entry->vblocks -= 1;

        cp->valid_block_count -= 1;
    }
    return 0;
}
int open_device(char* f2fs_img){
    int f2fs_fd;
    //char * f2fs_img = "/home/host/blockdedup-offline/f2fs.img";
    assert(stat(f2fs_img, &st) == 0);
    assert((f2fs_fd = open(f2fs_img, O_RDWR))>0);
    f2fs_start = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, f2fs_fd, 0);
    if(f2fs_start==MAP_FAILED)
        return errno;
    sb_begin = (struct f2fs_super_block *)(f2fs_start + F2FS_SUPER_OFFSET);
    assert(sb_begin->magic == F2FS_SUPER_MAGIC);
    nat_entry_begin = (struct f2fs_nat_entry *)((void *)f2fs_start + F2FS_BLKSIZE * sb_begin->nat_blkaddr);
    sb = sb_begin;
    int cp1_start_blk_no = get_sb(cp_blkaddr);
    int cp2_start_blk_no = cp1_start_blk_no + BLOCKS_IN_SEG;
    struct f2fs_checkpoint *cp1 = (struct f2fs_checkpoint *)((void *)f2fs_start + cp1_start_blk_no * F2FS_BLKSIZE);
    struct f2fs_checkpoint *cp2 = (struct f2fs_checkpoint *)((void *)f2fs_start + cp2_start_blk_no * F2FS_BLKSIZE);
    cp = (cp1->checkpoint_ver > cp2->checkpoint_ver) ? cp1 : cp2;
    cp_e = (struct f2fs_checkpoint *)((void *)f2fs_start + (cp1_start_blk_no + cp->cp_pack_total_block_count -1) * F2FS_BLKSIZE);
    nat_bitmap = &(cp->sit_nat_version_bitmap[cp->sit_ver_bitmap_bytesize]);
    sit_bitmap = &(cp->sit_nat_version_bitmap);
    //printf("valid_block_count:%d\n",cp->valid_block_count);
    return 0;
}

void close_with_sync(){
     // printf("valid_block_count:%d\n",cp->valid_block_count);
      assert(msync(f2fs_start, st.st_size, MS_SYNC)==0);
      assert(munmap(f2fs_start, st.st_size)==0);
}

int main(){
    char * f2fs_img = "/home/host/blockdedup-offline/f2fs.img";
    open_device(f2fs_img);
    return 0;
}
