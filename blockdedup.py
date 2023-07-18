import fcntl
import os
import sqlite3
import struct
import hashlib
import time
import sys
import pickle
from stat import *
BLOCK_SIZE = 4096
inline_data_size = 0
file_cnt = 0
class fiemap_layout:
    def __init__(self):
        self.fm_start = 0
        self.fm_length = 0
        self.fm_flags = 0
        self.fm_mapped_extents = 0
        self.fm_extent_count = 0
        self.fm_extents = []

class fiemap_extent:
    def __init__(self,fe_logical, fe_physical, fe_length,fe_reserved64_0, fe_reserved64_1,fe_flags,fe_reserved0,fe_reserved1,fe_reserved2):
        self.fe_logical = fe_logical
        self.fe_physical = fe_physical
        self.fe_length = fe_length
        self.fe_reserved64_0 = fe_reserved64_0
        self.fe_reserved64_1 = fe_reserved64_1
        self.fe_flags = fe_flags
        self.fe_reserved_0 = fe_reserved0
        self.fe_reserved_1 = fe_reserved1
        self.fe_reserved_2 = fe_reserved2

def get_fiemap_info(file):
    # 打开文件
    global file_cnt
    file_cnt +=1
    print(file,":",file_cnt)
    fd = os.open(file, os.O_RDONLY)

    # 获取文件系统块大小
    block_size = os.statvfs(file).f_bsize

    # 定义 struct fiemap 结构体
    fiemap = fiemap_layout()
    _FIEMAP = 3223348747
    max_extents = 16
    fiemap.fm_extent_count = max_extents
    extents_fmt = 'Q Q Q 2Q I 3I '
    fmt = 'Q Q I I I I ' + extents_fmt*max_extents
    fmt_header_size = 32
    extext_fields_num = 9
    ret = fcntl.ioctl(fd, _FIEMAP, struct.pack(fmt, 0,  2 ** 64 - 1, 0, 0, fiemap.fm_extent_count, 0,  *([0]*extext_fields_num*max_extents)))

    fiemap.fm_mapped_extents = struct.unpack(fmt,ret)[3]
    extents_size = struct.calcsize(extents_fmt)
    fm_extents = [struct.unpack(extents_fmt, ret[i:i+extents_size]) for i in range(fmt_header_size, len(ret), extents_size)]
    fm_extents = fm_extents[:fiemap.fm_mapped_extents]
    for extent in fm_extents:
        fiemap.fm_extents.append(fiemap_extent(*extent))
    #fiemap.fm_extents = fiemap.fm_extents[:fiemap.fm_mapped_extents]

    # 关闭文件
    os.close(fd)   

    return (block_size, fiemap)

def create_table():
    conn = sqlite3.connect('dedup.db')
    conn.execute('pragma foreign_keys = on')
    c = conn.cursor()
    c.execute('''
    CREATE TABLE ino_lbn (ino INTEGER, lbn INTEGER, pbn INTEGER, fingerprint TEXT, PRIMARY KEY (ino, lbn))
    ''')
    c.execute('''
    CREATE TABLE ino (ino INTEGER, du INTEGER , fingerprint TEXT, PRIMARY KEY (ino))
    ''')
    # c.execute('''
    # CREATE TABLE fingerprint (fingerprint TEXT, PRIMARY KEY (fingerprint))
    # ''')
    # c.execute('''
    # CREATE TABLE pbn (pbn INTEGER, fingerprint TEXT, ino INTEGER, lbn INTEGER, 
    # PRIMARY KEY  (pbn),
    # FOREIGN KEY(fingerprint) REFERENCES fingerprint(fingerprint),
    # FOREIGN KEY(ino,lbn) REFERENCES ino_lbn(ino,lbn))
    #     ''')
    conn.commit()
    conn.close()


def insert_data():
    conn = sqlite3.connect('dedup.db')
    c = conn.cursor()
    # ino_lbn_data = [
    #     (1, 0), (1,1) , (1,2) ,(2,0) , (2,1) , (2,2)
    # ]
    # c.executemany('''
    # INSERT INTO ino_lbn (ino, lbn) VALUES( ?, ?) 
    # ''' , ino_lbn_data)
    # fingerprint_data = [
    #     ("abcd1234564a",), ("12356446ddea",),
    # ]
    # c.executemany('''
    # INSERT INTO fingerprint (fingerprint) VALUES (?)
    # ''' ,fingerprint_data)
    # c.execute('''
    # INSERT INTO pbn (pbn, fingerprint, ino ,lbn) VALUES (1234, ? , ? ,? )
    # ''', (fingerprint_data[0][0], ino_lbn_data[0][0], ino_lbn_data[0][1]))

    c.execute('''
    INSERT INTO ino_lbn (ino, lbn, fingerprint) VALUES (?, ? , ?)
    ''', (12345,54321,"1234556787asdas"));

    conn.commit()

    # c.executemany('''
    # SELECT * from ?
    # ''', [("fingerprint") , ("ino_lbn"), ("pbn")])
def compute_hash(bytes):
    md5 = hashlib.md5()
    md5.update(bytes)
    return  md5.hexdigest()
def store_metadata(file_path,fiemap, ino,cur):
    global inline_data_size
    if fiemap.fm_mapped_extents == 0:
        pass
        #assert( os.path.getsize(file_path) == 0)
    else:
        with open(file_path,'rb') as f:
            ino_lbn_data = []
            #print(file_path)
            for extents in fiemap.fm_extents:
                if(extents.fe_length%BLOCK_SIZE !=0):
                    print("[inline data size]:", extents.fe_length)
                    inline_data_size += extents.fe_length
                    continue
                assert(extents.fe_length%BLOCK_SIZE ==0)
                assert(extents.fe_logical%BLOCK_SIZE==0)
                assert(extents.fe_physical%BLOCK_SIZE==0)
                print(f'[{extents.fe_logical//4096},{extents.fe_logical//BLOCK_SIZE + extents.fe_length//BLOCK_SIZE}):[{extents.fe_physical//BLOCK_SIZE},{extents.fe_physical//BLOCK_SIZE+extents.fe_length//BLOCK_SIZE})')
                start_block_num = extents.fe_logical//BLOCK_SIZE
                block_counts = extents.fe_length//BLOCK_SIZE
                for i in range(0,block_counts):
                    #ino_lbn_data.append((ino, start_block_num+i,))
                    file_pos = extents.fe_logical + i*BLOCK_SIZE
                    f.seek(file_pos)
                    #assert(file_pos == f.tell())
                    datablock = bytearray(f.read(BLOCK_SIZE))
                    fingerprint = compute_hash(datablock)
                    ino_lbn_data.append((ino,start_block_num+i, extents.fe_physical//BLOCK_SIZE+i,fingerprint))
            try:
                cur.executemany('''
                INSERT INTO ino_lbn (ino,lbn,pbn,fingerprint) VALUES(?,?,?,?)
                ''', ino_lbn_data)
                #cur.executemany('''
                #INSERT INTO fingerprint (fingerprint) VALUES(?)
                #''', fingerprint_data)
                #cur.executemany('''
                #INSERT INTO pbn (pbn, fingerprint, ino , lbn) VALUES#(?, ? ,? ,? )
                #''', pbn_data)
            except sqlite3.Error as e:
                print("An error occurred:", e.args[0])
                
def store_file_metadata(file_path,ino,cur):
    try:
        md5txt = os.popen(f'md5sum "{file_path}"').read().split(' ')[0]
        disk_space = os.popen(f'du -s --block-size=1 "{file_path}"').read().rstrip('\n').split('\t')[0]
        cur.execute('''
        INSERT INTO ino (ino,fingerprint,du) VALUES(?,?,?)
        ''', (ino,md5txt,int(disk_space)))
    except Exception as e:
        print("error:",e)
        return 
def build_index(file_path):
    # fixme: store_file_metadata为每个文件都建立的指纹索引
    # 而store_metadata仅仅针对4k对齐的块数据建立的指纹索引
    conn = sqlite3.connect('dedup.db')
    cur = conn.cursor()
    try:
        if(os.path.islink(file_path)):
            print("[symbolic link]:",file_path)
            return
        stat_info = os.stat(file_path)
        if(S_ISFIFO(stat_info.st_mode)):
            print("[pipe file]:", file_path)
            return
        ino = stat_info.st_ino
        file_size = stat_info.st_size
        (block_size, fiemap) =  get_fiemap_info(file_path)
        assert(block_size==4096)
    except Exception as e:
        print("err:",e)
        return
    store_metadata(file_path,fiemap, ino,cur)
    store_file_metadata(file_path,ino,cur)
    conn.commit()
    conn.close()


def traverse(path):
    for root, dirs, files in os.walk(path):
        for name in files:
            file_path = os.path.join(root, name)
            #print(file_path)
            build_index(file_path=file_path)
        # for name in dirs:
        #     dir_path = os.path.join(root, name)
        #     traverse(dir_path) # 递归遍历目录


def get_dedup_info():
    conn = sqlite3.connect('dedup.db')
    c = conn.cursor()
    c.execute('''
    SELECT fingerprint,GROUP_CONCAT(ino,','),GROUP_CONCAT(lbn,','), GROUP_CONCAT(pbn,',') from ino_lbn group by fingerprint having count(*)>1  order by fingerprint 
        ''')
    result = c.fetchall()
    count = 0
    print("duplicate entry counts:",len(result))
    os.system("sudo umount /mnt/sdcmount")
    f2fshack.open_device('./f2fs.img')
    for entry in result:
        finger = entry[0]
        ino_list = entry[1].split(',')
        lbn_list = entry[2].split(',')
        pbn_list = entry[3].split(',')
        ino_list_c = f2fshack.new_intArray(len(ino_list))
        lbn_list_c = f2fshack.new_intArray(len(lbn_list))
        pbn_list_c = f2fshack.new_intArray(len(pbn_list))
        for i in range(0,len(ino_list)):
            f2fshack.intArray_setitem(ino_list_c,i, int(ino_list[i]))
        for i in range(0,len(lbn_list)):
            f2fshack.intArray_setitem(lbn_list_c,i, int(lbn_list[i]))
        for i in range(0,len(pbn_list)):
            f2fshack.intArray_setitem(pbn_list_c,i, int(pbn_list[i]))
        f2fshack.dedup_one_finger(ino_list=ino_list_c, lbn_list= lbn_list_c, pbn_list= pbn_list_c, size=len(ino_list))
        count += 1
    f2fshack.close_with_sync()
    conn.close()

def list_table():
    conn = sqlite3.connect('dedup.db')
    c = conn.cursor()
    # c.execute('''
    #           SELECT * from ino_lbn 
    #           WHERE fingerprint IN (
    #           SELECT fingerprint from ino_lbn GROUP BY fingerprint HAVING COUNT(*) > 1
    #           ) ORDER BY fingerprint
    #           ''')
    # c.execute('''
    #             SELECT * from ino_lbn group by fingerprint having count(*)>1  order by fingerprint 
    # ''')
    c.execute('select * from ino_lbn')
    result = c.fetchall()
    print("all blocks count in db:",len(result))
    c.execute('''
    SELECT fingerprint,GROUP_CONCAT(ino,','),GROUP_CONCAT(lbn,','), GROUP_CONCAT(pbn,',') from ino_lbn group by fingerprint having count(*)>1  order by fingerprint 
        ''')
    result = c.fetchall()
    print("all unique blocks count in db:", len(result))
    result = sorted(result, key=lambda item: len(item[1].split(',')),reverse=True)
    print("Hot 100 fingerprint:")
    for i in range(0,100):
        print(i, ": fingerprint:", result[i][0]," count:",len(result[i][1].split(',')))
        ino_list = result[i][1].split(',')
        lbn_list = result[i][2].split(',')
        for j in range(0,min(100,len(ino_list))):
            print(f'[{ino_list[j]},{lbn_list[j]}]', end=' ')
        print()
    #print(result[1])
    #f = open('./tmp', 'w')
    #pickle.dump(result[0:50],f)
    
    # for i in range(0,50):
    #     f.writelines(result[i])
    #f.close()
    #print(result[0])
    #print(result[0][0])
    conn.close()

create_table()
#os.system("sudo mount ./f2fs.img /mnt/sdcmount -o noinline_xattr,nouser_xattr,noinline_data,noinline_dentry")
#traverse("/mnt/android/f2fs/")
traverse("/home/host/blockdedup-offline/test/")

#traverse("/home/host/blockdedup-offline")
#traverse("/home/host/src-highlite")
#traverse("/home/host/portdedup")
# sys.path.append('./')
# import f2fshack
# get_dedup_info()
#list_table()

# sys.path.append('/home/host/blockdedup-offline')
# import f2fshack
# print(f2fshack.open_device())
#get_dedup_info()

