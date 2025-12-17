#include "../src/utils/huffman.h"

int main() {
    char arr[] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'm'};
    int freq[] = {5, 9, 12, 13, 16, 45, 28, 3};

    int size = sizeof(arr) / sizeof(arr[0]);
    HuffmanCodes(arr, freq, size);
    return 0;
}