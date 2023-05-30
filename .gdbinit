#file f2fshack 
#set args ./f2fs.img
#b dedup_one_finger
#tui enable
#run > out
#file sh
#set args ./demo.sh
#set follow-fork-mode child
#add-symbol-file _f2fshack.so
#b dedup_one_finger
#starti

