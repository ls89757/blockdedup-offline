// example.c
#include<stdio.h>
int add(int a, int b) {
    return a + b;
}

// int sum_array(int* array, int length) {
//     int sum = 0;
//     for (int i = 0; i < length; i++) {
//         sum += array[i];
//     }
//     return sum;
// }

void print_array(int* x , int len) {
int i;
for (i = 0; i < len; i++) {
printf("[%d] = %d\n", i, x[i]);
}
}