#ifndef HUFFMAN_H
#define HUFFMAN_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>

#define MAX_TREE_HT 257

typedef struct MinHeapNode
{
    unsigned char data;
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

typedef struct
{
    unsigned char buffer;
    int bitPos;
} BitReader;

void compress(char *inputFile, char *outputFile);
void decompress(char *inputFile, char *outputFile);

#endif