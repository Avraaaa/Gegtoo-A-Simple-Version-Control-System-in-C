#include "huffman.h"

MinHeapNode *newNode(char data, unsigned freq)
{
    MinHeapNode *newNode = (MinHeapNode *)malloc(sizeof(MinHeapNode));

    newNode->data = data;
    newNode->freq = freq;
    newNode->left = NULL;
    newNode->right = NULL;
    return newNode;
}


MinHeap *createMinHeap(unsigned capacity)
{
    MinHeap *newHeap = (MinHeap *)malloc(sizeof(MinHeap));

    newHeap->size = 0;
    newHeap->capacity = capacity;
    newHeap->array = (MinHeapNode **)malloc(((newHeap->capacity) + 1) * sizeof(MinHeapNode *));
    return newHeap;
}


void swapMinHeapNode(MinHeapNode **a, MinHeapNode **b)
{
    MinHeapNode *temp = *a;
    *a = *b;
    *b = temp;
    return;
}


void minHeapify(MinHeap *minHeap, int index)
{
    int smallest = index;
    int left = (2 * index);
    int right = 2 * index + 1;

    if (left <= minHeap->size && (minHeap->array[left]->freq) < (minHeap->array[smallest]->freq))
    {
        smallest = left;
    }

    if (right <= minHeap->size && minHeap->array[right]->freq < minHeap->array[smallest]->freq)
    {
        smallest = right;
    }

    if (smallest != index)
    {
        swapMinHeapNode(&minHeap->array[smallest], &minHeap->array[index]);

        minHeapify(minHeap, smallest);
    }
}


bool isSizeOne(MinHeap *minHeap)
{
    if (minHeap->size == 1)
    {
        return true;
    }
    else
    {
        return false;
    }
}


MinHeapNode *extractMin(MinHeap *minHeap)
{
    MinHeapNode *temp = minHeap->array[1];
    minHeap->array[1] = minHeap->array[minHeap->size];

    --minHeap->size;
    minHeapify(minHeap, 1);

    return temp;
}


void insertMinHeap(MinHeap *minHeap, MinHeapNode *minHeapNode)
{
    ++minHeap->size;
    int i = minHeap->size;

    while (i > 1 && minHeapNode->freq < (minHeap->array[i / 2]->freq))
    {
        minHeap->array[i] = minHeap->array[i / 2];
        i = i / 2;
    }

    minHeap->array[i] = minHeapNode;
}


void buildMinHeap(MinHeap *minHeap)
{
    int n = minHeap->size;

    for (int i = (n) / 2; i >= 1; i--)
    {
        minHeapify(minHeap, i);
    }
}


void printArr(int arr[], int n)
{
    for (int i = 0; i < n; i++)
    {
        printf("%d", arr[i]);
    }
    printf("\n");
}


bool isLeaf(MinHeapNode *root)
{
    if (root->left == NULL && root->right == NULL)
    {
        return true;
    }
    else
        return false;
}


MinHeap *createAndBuildMinHeap(char data[], int freq[], int size)
{
    MinHeap *minHeap = createMinHeap(size);

    for (int i = 1; i <= size; i++)
    {
        minHeap->array[i] = newNode(data[i - 1], freq[i - 1]);
    }

    minHeap->size = size;
    buildMinHeap(minHeap);

    return minHeap;
}


MinHeapNode *buildHuffmanTree(char data[], int freq[], int size)
{
    MinHeapNode *left, *right, *top;

    MinHeap *minHeap = createAndBuildMinHeap(data, freq, size);

    if (minHeap->size == 1)
    {
        left = extractMin(minHeap);
        top = newNode('$', left->freq);
        top->left = left;
        top->right = NULL;
        return top;
    }

    while (!isSizeOne(minHeap))
    {
        left = extractMin(minHeap);
        right = extractMin(minHeap);

        top = newNode('$', left->freq + right->freq);
        top->left = left;
        top->right = right;

        insertMinHeap(minHeap, top);
    }

    return extractMin(minHeap);
}


void generateCodes(MinHeapNode *root, char codeArr[], int depth, char **codeTable)
{
    if (root == NULL)
    {
        return;
    }

    if (depth >= MAX_TREE_HT)
    {
        fprintf(stderr, "Error: Tree depth limit exceeded\n", depth);
        exit(1);
    }

    if (isLeaf(root))
    {
        unsigned char charIndx = (unsigned char)root->data;
        codeTable[charIndx] = (char *)malloc(depth + 1);

        if (codeTable[charIndx] == NULL)
        {
            perror("Error: Memory allocation failed");
            exit(1);
        }

        for (int i = 0; i < depth; i++)
        {
            codeTable[charIndx][i] = codeArr[i];
        }

        codeTable[charIndx][depth] = '\0';

        return;
    }
    else
    {
        codeArr[depth] = '0';
        generateCodes(root->left, codeArr, depth + 1, codeTable);

        codeArr[depth] = '1';
        generateCodes(root->right, codeArr, depth + 1, codeTable);
    }
}


void writeHeader(FILE *output, __uint32_t freq[256], __uint64_t fileSize)
{
    __uint32_t magic = 0x48554646;

    fwrite(&magic, sizeof(__uint32_t), 1, output);

    fwrite(&fileSize, sizeof(__uint64_t), 1, output);

    fwrite(freq, sizeof(__uint32_t), 256, output);
}


void writeBit(FILE *output, int bit, BitWriter *writer)
{
    bit = bit & 1;
    writer->buffer |= (bit << (7 - writer->bitPos));

    writer->bitPos++;

    if (writer->bitPos == 8)
    {
        fputc(writer->buffer, output);

        if (ferror(output))
        {
            perror("Error writing to file");
            exit(1);
        }

        writer->buffer = 0;
        writer->bitPos = 0;
    }
}


void flushBits(FILE *output, BitWriter *writer)
{
    if (writer->bitPos == 0)
    {
        return;
    }

    fputc(writer->buffer, output);

    if (ferror(output))
    {
        perror("Error writing to file");
        exit(1);
    }

    writer->buffer = 0;
    writer->bitPos = 0;
}


void HuffmanCodes(char data[], int freq[], int size)
{
    MinHeapNode *root = buildHuffmanTree(data, freq, size);

    int arr[MAX_TREE_HT], top = 0;
}


void calculateFrequency(FILE *input, __uint32_t freq[256])
{
    int byte;

    for (int i = 0; i < 256; i++)
    {
        freq[i] = 0;
    }

    unsigned char buffer[4096];
    size_t bytesRead;

    while ((bytesRead = fread(buffer, 1, 4096, input)) > 0)
    {
        for (size_t i = 0; i < bytesRead; i++)
        {
            freq[buffer[i]]++;
        }
    }

    fseek(input, 0, SEEK_SET);
}


void compressFile(FILE *input, FILE *output, char **codeTable, __uint32_t freq[256], __uint64_t fileSize)
{
    BitWriter writer;
    int byte;

    writeHeader(output, freq, fileSize);

    writer.buffer = 0;
    writer.bitPos = 0;

    while ((byte = fgetc(input)) != EOF)
    {
        char *code = codeTable[byte];

        for (int i = 0; code[i] != '\0'; i++)
        {
            int bit;

            if (code[i] == '1')
            {
                bit = 1;
            }
            else
                bit = 0;

            writeBit(output, bit, &writer);
        }
    }
    flushBits(output, &writer);
}


void freeCodeTable(char **codeTable)
{
    if (codeTable == NULL)
    {
        return;
    }

    for (int i = 0; i < 256; i++)
    {
        if (codeTable[i] != NULL)
        {
            free(codeTable[i]);
        }
    }
    free(codeTable);
}


void freeHuffmanTree(MinHeapNode *root)
{
    if (root == NULL)
    {
        return;
    }

    freeHuffmanTree(root->left);
    freeHuffmanTree(root->right);
    free(root);
}


void compress(char *inputFile, char *outputFile)
{
    FILE *input = fopen(inputFile, "rb");
    if (!input)
    {
        perror("Error opening input file");
        return;
    }

    fseek(input, 0, SEEK_END);
    __uint64_t fileSize = ftell(input);
    fseek(input, 0, SEEK_SET);

    __uint32_t freq[256];

    calculateFrequency(input, freq);

    int uniqueChars = 0;

    for (int i = 0; i < 256; i++)
    {
        if (freq[i] > 0)
        {
            uniqueChars++;
        }
    }

    char *data = (char *)malloc(uniqueChars);
    int *freqData = (int *)malloc(uniqueChars * sizeof(int));
    int indx = 0;

    for (int i = 0; i < 256; i++)
    {
        if (freq[i] > 0)
        {
            data[indx] = (char)i;
            freqData[indx] = freq[i];
            indx++;
        }
    }

    MinHeapNode *root = buildHuffmanTree(data, freqData, uniqueChars);

    char **codeTable = (char **)calloc(256, sizeof(char *));
    char codeArr[MAX_TREE_HT];
    generateCodes(root, codeArr, 0, codeTable);

    FILE *output = fopen(outputFile, "wb");

    if (!output)
    {
        perror("Error creating output file");
        fclose(input);
        return;
    }

    compressFile(input, output, codeTable, freq, fileSize);

    fclose(input);
    fclose(output);
    free(data);
    free(freqData);
    freeCodeTable(codeTable);
    freeHuffmanTree(root);

    printf("Compressed %s into %s\n", inputFile, outputFile);
}