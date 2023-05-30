import sys
sys.path.append('./')
import example
arr = [1,2,3]
arr_ = example.new_intArray(10)
for i in range (0,10):
    example.intArray_setitem(arr_, i, 3*i)
example.print_array(arr_)
#print(example.sum_array(arr,len(arr)))