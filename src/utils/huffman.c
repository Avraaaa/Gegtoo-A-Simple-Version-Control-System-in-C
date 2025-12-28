#include "huffman.h"

MinHeapNode *new_node(char data, unsigned freq)
{
    MinHeapNode *new_node = (MinHeapNode *)malloc(sizeof(MinHeapNode));

    new_node->data = data;
    new_node->freq = freq;
    new_node->left = NULL;
    new_node->right = NULL;
    return new_node;
}


MinHeap *create_minheap(unsigned capacity)
{
    MinHeap *newHeap = (MinHeap *)malloc(sizeof(MinHeap));

    newHeap->size = 0;
    newHeap->capacity = capacity;
    newHeap->array = (MinHeapNode **)malloc(((newHeap->capacity) + 1) * sizeof(MinHeapNode *));
    return newHeap;
}


void swap_minheap_node(MinHeapNode **a, MinHeapNode **b)
{
    MinHeapNode *temp = *a;
    *a = *b;
    *b = temp;
    return;
}


void min_heapify(MinHeap *minHeap, int index)
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
        swap_minheap_node(&minHeap->array[smallest], &minHeap->array[index]);

        min_heapify(minHeap, smallest);
    }
}


bool is_size_one(MinHeap *minHeap)
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


MinHeapNode *extract_min(MinHeap *minHeap)
{
    MinHeapNode *temp = minHeap->array[1];
    minHeap->array[1] = minHeap->array[minHeap->size];

    --minHeap->size;
    min_heapify(minHeap, 1);

    return temp;
}


void insert_minheap(MinHeap *minHeap, MinHeapNode *minHeapNode)
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


void build_minheap(MinHeap *minHeap)
{
    int n = minHeap->size;

    for (int i = (n) / 2; i >= 1; i--)
    {
        min_heapify(minHeap, i);
    }
}


void print_arr(int arr[], int n)
{
    for (int i = 0; i < n; i++)
    {
        printf("%d", arr[i]);
    }
    printf("\n");
}


bool is_leaf(MinHeapNode *root)
{
    if (root->left == NULL && root->right == NULL)
    {
        return true;
    }
    else
        return false;
}


MinHeap *create_and_build_minheap(char data[], int freq[], int size)
{
    MinHeap *minHeap = create_minheap(size);

    for (int i = 1; i <= size; i++)
    {
        minHeap->array[i] = new_node(data[i - 1], freq[i - 1]);
    }

    minHeap->size = size;
    build_minheap(minHeap);

    return minHeap;
}


MinHeapNode *build_huffman_tree(char data[], int freq[], int size)
{
    MinHeapNode *left, *right, *top;

    MinHeap *minHeap = create_and_build_minheap(data, freq, size);

    if (minHeap->size == 1)
    {
        left = extract_min(minHeap);
        top = new_node('$', left->freq);
        top->left = left;
        top->right = NULL;
        return top;
    }

    while (!is_size_one(minHeap))
    {
        left = extract_min(minHeap);
        right = extract_min(minHeap);

        top = new_node('$', left->freq + right->freq);
        top->left = left;
        top->right = right;

        insert_minheap(minHeap, top);
    }

    return extract_min(minHeap);
}


void generate_codes(MinHeapNode *root, char codeArr[], int depth, char **codeTable)
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

    if (is_leaf(root))
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
        generate_codes(root->left, codeArr, depth + 1, codeTable);

        codeArr[depth] = '1';
        generate_codes(root->right, codeArr, depth + 1, codeTable);
    }
}


int read_bit(FILE *input, BitReader *reader)
{
    if (reader->bitPos == 0)
    {
        int byte = fgetc(input);
        if (byte == EOF)
        {
            return -1;
        }

        reader->buffer = (unsigned char)byte;
        reader->bitPos = 8;
    }

    reader->bitPos--;
    return (reader->buffer >> reader->bitPos) & 1;
}


void write_header(FILE *output, __uint32_t freq[256], __uint64_t fileSize)
{
    __uint32_t magic = 0x48554646;

    fwrite(&magic, sizeof(__uint32_t), 1, output);

    fwrite(&fileSize, sizeof(__uint64_t), 1, output);

    fwrite(freq, sizeof(__uint32_t), 256, output);
}


void read_header(FILE *input, __uint32_t freq[256], __uint64_t *fileSize)
{
    __uint32_t magic;

    fread(&magic, sizeof(__uint32_t), 1, input);

    if (magic != 0x48554646)
    {
        fprintf(stderr, "Error: Invalid file format\n");
        exit(1);
    }

    fread(fileSize, sizeof(__uint64_t), 1, input);

    fread(freq, sizeof(__uint32_t), 256, input);
}


void write_bit(FILE *output, int bit, BitWriter *writer)
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


void flush_bits(FILE *output, BitWriter *writer)
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


void huffman_codes(char data[], int freq[], int size)
{
    MinHeapNode *root = build_huffman_tree(data, freq, size);

    int arr[MAX_TREE_HT], top = 0;
}


void calculate_frequency(FILE *input, __uint32_t freq[256])
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


void compress_file(FILE *input, FILE *output, char **codeTable, __uint32_t freq[256], __uint64_t fileSize)
{
    BitWriter writer;
    int byte;

    write_header(output, freq, fileSize);

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

            write_bit(output, bit, &writer);
        }
    }
    flush_bits(output, &writer);
}


void decompress_file(FILE *input, FILE *output, MinHeapNode *root, __uint64_t fileSize)
{
    BitReader reader;
    __uint64_t bytesWritten = 0;

    reader.bitPos = 0;
    reader.buffer = 0;

    MinHeapNode *current = root;

    while (bytesWritten < fileSize)
    {
        int bit = read_bit(input, &reader);

        if (bit == -1)
        {
            fprintf(stderr, "Error: Unexpected EOF\n");
            break;
        }

        if (bit == 0)
        {
            current = current->left;
        }

        else
            current = current->right;

        if (is_leaf(current))
        {
            fputc(current->data, output);
            bytesWritten++;
            current = root;
        }
    }
}


void free_code_table(char **codeTable)
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


void free_huffman_tree(MinHeapNode *root)
{
    if (root == NULL)
    {
        return;
    }

    free_huffman_tree(root->left);
    free_huffman_tree(root->right);
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

    calculate_frequency(input, freq);

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

    MinHeapNode *root = build_huffman_tree(data, freqData, uniqueChars);

    char **codeTable = (char **)calloc(256, sizeof(char *));
    char codeArr[MAX_TREE_HT];
    generate_codes(root, codeArr, 0, codeTable);

    FILE *output = fopen(outputFile, "wb");

    if (!output)
    {
        perror("Error creating output file");
        fclose(input);
        return;
    }

    compress_file(input, output, codeTable, freq, fileSize);

    fclose(input);
    fclose(output);
    free(data);
    free(freqData);
    free_code_table(codeTable);
    free_huffman_tree(root);

    printf("Compressed %s into %s\n", inputFile, outputFile);
}


void decompress(char *inputFile, char *outputFile)
{
    FILE *input = fopen(inputFile, "rb");

    if (!input)
    {
        perror("Error opening input file");
        return;
    }

    __uint32_t freq[256];
    __uint64_t fileSize;

    read_header(input, freq, &fileSize);

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

    MinHeapNode *root = build_huffman_tree(data, freqData, uniqueChars);

    FILE *output = fopen(outputFile, "wb");

    if (!output)
    {
        perror("Error creating output file");
        fclose(input);
        return;
    }

    decompress_file(input, output, root, fileSize);

    fclose(input);
    fclose(output);

    free(data);
    free(freqData);
    free_huffman_tree(root);

    printf("Decompressed %s into %s\n", inputFile, outputFile);
}