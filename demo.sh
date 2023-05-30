#!/bin/bash
# make a f2fs img and mount it
dd if=/dev/zero of=./f2fs.img bs=1M count=10K
mkfs.f2fs ./f2fs.img 
sudo mount ./f2fs.img /mnt/sdcmount -o noinline_xattr,nouser_xattr,noinline_data,noinline_dentry

# make duplicate files
dd if=/dev/urandom of=/mnt/sdcmount/file113 bs=1M count=1500
cp /mnt/sdcmount/file113 /mnt/sdcmount/file114
cp /mnt/sdcmount/file113 /mnt/sdcmount/file115
sync
md5sum /mnt/sdcmount/file113
md5sum /mnt/sdcmount/file114
md5sum /mnt/sdcmount/file115
# view current space usage
df | grep sdcmount
sudo umount /mnt/sdcmount

# run blockdedup python script
# now it is umounted 

python ./blockdedup.py

# now it is umounted


# view filehash and space usage
# sudo mount ./f2fs.img /mnt/sdcmount -o noinline_xattr,nouser_xattr,noinline_data,noinline_dentry
# md5sum /mnt/sdcmount/file113
# md5sum /mnt/sdcmount/file114
# md5sum /mnt/sdcmount/file115
# df | grep sdcmount