#include "huffman.h"


struct MinHeapNode *newNode(char data, unsigned freq)
{
    MinHeapNode *newNode;

    newNode = (MinHeapNode *)malloc(sizeof(MinHeapNode));

    newNode->data = data;
    newNode->freq = freq;
    newNode->left = NULL;
    newNode->right = NULL;

    return newNode;
}



struct MinHeap *createMinHeap(unsigned capacity)
{
    MinHeap *newHeap;

    newHeap = (MinHeap *)malloc(sizeof(MinHeap));
    newHeap->size = 0;
    newHeap->capacity = capacity;
    newHeap->array =
        (MinHeapNode **)malloc((newHeap->capacity + 1) * sizeof(MinHeapNode *));

    return newHeap;
}



void swapMinHeapNode(MinHeapNode **a, MinHeapNode **b)
{
    MinHeapNode *temp;

    temp = *a;
    *a = *b;
    *b = temp;
}



void minHeapify(MinHeap *minHeap, int index)
{
    int smallest;
    int left;
    int right;

    smallest = index;
    left = 2 * index;
    right = 2 * index + 1;

    if (left <= minHeap->size &&
        minHeap->array[left]->freq < minHeap->array[smallest]->freq)
        smallest = left;

    if (right <= minHeap->size &&
        minHeap->array[right]->freq < minHeap->array[smallest]->freq)
        smallest = right;

    if (smallest != index)
    {
        swapMinHeapNode(&minHeap->array[smallest],
                        &minHeap->array[index]);
        minHeapify(minHeap, smallest);
    }
}



bool isSizeOne(MinHeap *minHeap)
{
    return (minHeap->size == 1);
}



struct MinHeapNode *extractMin(MinHeap *minHeap)
{
    MinHeapNode *temp;

    temp = minHeap->array[1];
    minHeap->array[1] = minHeap->array[minHeap->size];
    --minHeap->size;

    minHeapify(minHeap, 1);

    return temp;
}



void insertMinHeap(MinHeap *minHeap, MinHeapNode *minHeapNode)
{
    int i;

    ++minHeap->size;
    i = minHeap->size;

    while (i > 1 &&
           minHeapNode->freq < minHeap->array[i / 2]->freq)
    {
        minHeap->array[i] = minHeap->array[i / 2];
        i = i / 2;
    }

    minHeap->array[i] = minHeapNode;
}



void buildMinHeap(MinHeap *minHeap)
{
    int n;
    int i;

    n = minHeap->size;

    for (i = n / 2; i >= 1; i--)
        minHeapify(minHeap, i);
}



void printArr(int arr[], int n)
{
    int i;

    for (i = 0; i < n; i++)
        printf("%d", arr[i]);

    printf("\n");
}



bool isLeaf(MinHeapNode *root)
{
    return (root->left == NULL && root->right == NULL);
}



struct MinHeap *createAndBuildMinHeap(char data[], int freq[], int size)
{
    MinHeap *minHeap;
    int i;

    minHeap = createMinHeap(size);

    for (i = 1; i <= size; i++)
        minHeap->array[i] = newNode(data[i - 1], freq[i - 1]);

    minHeap->size = size;
    buildMinHeap(minHeap);

    return minHeap;
}



struct MinHeapNode *buildHuffmanTree(char data[], int freq[], int size)
{
    MinHeapNode *left;
    MinHeapNode *right;
    MinHeapNode *top;
    MinHeap *minHeap;

    minHeap = createAndBuildMinHeap(data, freq, size);

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



void printCodes(MinHeapNode *root, int arr[], int top)
{
    if (root->left)
    {
        arr[top] = 0;
        printCodes(root->left, arr, top + 1);
    }

    if (root->right)
    {
        arr[top] = 1;
        printCodes(root->right, arr, top + 1);
    }

    if (isLeaf(root))
    {
        printf("%c: ", root->data);
        printArr(arr, top);
    }
}



void HuffmanCodes(char data[], int freq[], int size)
{
    MinHeapNode *root;
    int arr[MAX_TREE_HT];
    int top;

    root = buildHuffmanTree(data, freq, size);
    top = 0;

    printCodes(root, arr, top);
}
