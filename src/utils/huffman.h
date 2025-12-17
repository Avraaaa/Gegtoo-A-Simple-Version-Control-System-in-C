#ifndef HUFFMAN_H
#define HUFFMAN_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define MAX_TREE_HT 100

typedef struct MinHeapNode
{
    char data;
    unsigned freq;
    struct MinHeapNode *left, *right;
} MinHeapNode;

typedef struct MinHeap
{
    unsigned size;
    unsigned capacity;
    struct MinHeapNode **array;
} MinHeap;

void HuffmanCodes(char data[], int freq[], int size);

#endif