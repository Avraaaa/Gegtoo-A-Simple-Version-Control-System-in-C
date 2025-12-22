#ifndef HUFFMAN_H
#define HUFFMAN_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define MAX_TREE_HT 257

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

typedef struct
{
    unsigned char buffer;
    int bitPos;
} BitWriter;

void HuffmanCodes(char data[], int freq[], int size);
void compress(char *inputFile, char *outputFile);

#endif