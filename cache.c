/*
 * EECS 370, University of Michigan
 * Project 4: LC-2K Cache Simulator
 * Instructions are found in the project spec.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>

#define MAX_CACHE_SIZE 256
#define MAX_BLOCK_SIZE 256

// **Note** this is a preprocessor macro. This is not the same as a function.
// Powers of 2 have exactly one 1 and the rest 0's, and 0 isn't a power of 2.
#define is_power_of_2(val) (val && !(val & (val - 1)))


/*
 * Accesses 1 word of memory.
 * addr is a 16-bit LC2K word address.
 * write_flag is 0 for reads and 1 for writes.
 * write_data is a word, and is only valid if write_flag is 1.
 * If write flag is 1, mem_access does: state.mem[addr] = write_data.
 * The return of mem_access is state.mem[addr].
 */
extern int mem_access(int addr, int write_flag, int write_data);

/*
 * Returns the number of times mem_access has been called.
 */
extern int get_num_mem_accesses(void);

//Use this when calling printAction. Do not modify the enumerated type below.
enum actionType
{
    cacheToProcessor,
    processorToCache,
    memoryToCache,
    cacheToMemory,
    cacheToNowhere
};

/* You may add or remove variables from these structs */
typedef struct blockStruct
{
    int data[MAX_BLOCK_SIZE];
    int dirty;
    int lruLabel;
    int tag;
    int valid;
} blockStruct;

typedef struct cacheStruct
{
    blockStruct blocks[MAX_CACHE_SIZE];
    int blockSize;
    int numSets;
    int blocksPerSet;
    //int size;
} cacheStruct;

/* Global Cache variable */
cacheStruct cache;

void printAction(int, int, enum actionType);
void printCache(void);

/*
 * Set up the cache with given command line parameters. This is
 * called once in main(). You must implement this function.
 */
void cache_init(int blockSize, int numSets, int blocksPerSet)
{
    if (blockSize <= 0 || numSets <= 0 || blocksPerSet <= 0) {
        printf("error: input parameters must be positive numbers\n");
        exit(1);
    }
    if (blocksPerSet * numSets > MAX_CACHE_SIZE) {
        printf("error: cache must be no larger than %d blocks\n", MAX_CACHE_SIZE);
        exit(1);
    }
    if (blockSize > MAX_BLOCK_SIZE) {
        printf("error: blocks must be no larger than %d words\n", MAX_BLOCK_SIZE);
        exit(1);
    }
    if (!is_power_of_2(blockSize)) {
        printf("warning: blockSize %d is not a power of 2\n", blockSize);
    }
    if (!is_power_of_2(numSets)) {
        printf("warning: numSets %d is not a power of 2\n", numSets);
    }
    printf("Simulating a cache with %d total lines; each line has %d words\n",
        numSets * blocksPerSet, blockSize);
    printf("Each set in the cache contains %d lines; there are %d sets\n",
        blocksPerSet, numSets);

    cache.blockSize = blockSize;
    cache.numSets = numSets;
    cache.blocksPerSet = blocksPerSet;

    for (int i = 0; i < 256; i++) {
        cache.blocks[i].valid = 0;
    }

    //cache.size = blockSize * numSets * blocksPerSet;
    //return;
}

int tagVal(int addr) {
    int blockOffsetBits = (int)log2(cache.blockSize);
    int setIndexBits = (int)log2(cache.numSets);
    int tagBits = 16 - (blockOffsetBits + setIndexBits);
    return (addr >> (blockOffsetBits + setIndexBits)) & ((1 << tagBits) - 1);
}

int setIndVal(int addr) {
    int blockOffsetBits = (int)log2(cache.blockSize);
    int setIndexBits = (int)log2(cache.numSets);
    //int tagBits = 16 - (blockOffsetBits + setIndexBits);
    return (addr >> blockOffsetBits) & ((1 << setIndexBits) - 1);
}

int blockOffsetVal(int addr) {
    int blockOffsetBits = (int)log2(cache.blockSize);
    //int setIndexBits = (int)log2(cache.numSets);
    //int tagBits = 16 - (blockOffsetBits + setIndexBits);
    return addr & ((1 << blockOffsetBits) - 1);
}

int startAddrCalc(int addr) {
    int x = tagVal(addr) << ((int)log2(cache.blockSize) + (int)log2(cache.numSets));
    int y = setIndVal(addr) << (int)log2(cache.blockSize);
    return x + y;
}

int memAddrCalc(int newSetInd, int oldTag) {
    int x = oldTag << ((int)log2(cache.blockSize) + (int)log2(cache.numSets));
    int y = newSetInd << (int)log2(cache.blockSize);
    return x + y;
}

void cacheDataWF0(int addr, int *data, bool *cacheResolved) {
    int tag = tagVal(addr);
    int setInd = setIndVal(addr);
    int blockOffset = blockOffsetVal(addr);

    int indexI = setInd*cache.blocksPerSet;

    for (int i = 0; i < cache.blocksPerSet; i++) {
        if (cache.blocks[indexI].valid) {
            if (cache.blocks[indexI].tag == tag) {
                *cacheResolved = true;
                *data = cache.blocks[indexI].data[blockOffset];
                printAction(addr, 1, cacheToProcessor);

                int x = cache.blocks[indexI].lruLabel;
                cache.blocks[indexI].lruLabel = cache.blocksPerSet - 1;
                int indexJ = setInd*cache.blocksPerSet;
                for (int j = 0; j < cache.blocksPerSet; j++) {
                    if ((indexJ != indexI) && (cache.blocks[indexJ].lruLabel > x)) {
                        cache.blocks[indexJ].lruLabel--;
                    }
                    indexJ++;
                }

                break;
            }
        }
        indexI++;
    }
}

void cacheDataWF1(int addr, int write_data, bool *cacheResolved) {
    int tag = tagVal(addr);
    int setInd = setIndVal(addr);
    int blockOffset = blockOffsetVal(addr);

    int indexI = setInd*cache.blocksPerSet;

    for (int i = 0; i < cache.blocksPerSet; i++) {
        if (cache.blocks[indexI].valid) {
            if (cache.blocks[indexI].tag == tag) {
                *cacheResolved = true;
                cache.blocks[indexI].data[blockOffset] = write_data;
                cache.blocks[indexI].dirty = 1;
                printAction(addr, 1, processorToCache);

                int x = cache.blocks[indexI].lruLabel;
                cache.blocks[indexI].lruLabel = cache.blocksPerSet - 1;
                int indexJ = setInd*cache.blocksPerSet;
                for (int j = 0; j < cache.blocksPerSet; j++) {
                    if ((indexJ != indexI) && (cache.blocks[indexJ].lruLabel > x)) {
                        cache.blocks[indexJ].lruLabel--;
                    }
                    indexJ++;
                }

                break;
            }
        }
        indexI++;
    }
}

bool memToCacheLoader_Empty(int addr) {
    int tag = tagVal(addr);
    int setInd = setIndVal(addr);
    //int blockOffset = blockOffsetVal(addr);

    int indexI = setInd*cache.blocksPerSet;
    for (int i = 0; i < cache.blocksPerSet; i++) {
        if (cache.blocks[indexI].valid == 0) {
            cache.blocks[indexI].dirty = 0;
            cache.blocks[indexI].valid = 1;
            cache.blocks[indexI].tag = tag;
            int startAddr = startAddrCalc(addr);
            for (int j = 0; j < cache.blockSize; j++) {
                cache.blocks[indexI].data[j] = mem_access(startAddr, 0, 0);
                startAddr++;
            }
            printAction(startAddrCalc(addr), cache.blockSize, memoryToCache);
            return true;
        }
        indexI++;
    }
    return false;
}

void memToCacheLoader_LRU(int addr) {
    int tag = tagVal(addr);
    int setInd = setIndVal(addr);
    //int blockOffset = blockOffsetVal(addr);

    int indexI = setInd*cache.blocksPerSet;
    for (int i = 0; i < cache.blocksPerSet; i++) {
        if (cache.blocks[indexI].lruLabel == 0) {
            if (cache.blocks[indexI].dirty == 1) {
                int memAddr = memAddrCalc(setInd, cache.blocks[indexI].tag);
                for (int j = 0; j < cache.blockSize; j++) {
                    mem_access(memAddr, 1, cache.blocks[indexI].data[j]);
                    memAddr++;
                }
                printAction(memAddrCalc(setInd, cache.blocks[indexI].tag), cache.blockSize, cacheToMemory);
                cache.blocks[indexI].dirty = 0;
            } else {
                printAction(memAddrCalc(setInd, cache.blocks[indexI].tag), cache.blockSize, cacheToNowhere);
            }
            cache.blocks[indexI].valid = 1;
            cache.blocks[indexI].tag = tag;
            int startAddr = startAddrCalc(addr);
            for (int j = 0; j < cache.blockSize; j++) {
                cache.blocks[indexI].data[j] = mem_access(startAddr, 0, 0);
                startAddr++;
            }
            printAction(startAddrCalc(addr), cache.blockSize, memoryToCache);
            break;
        }
        indexI++;
    }
}


/*
 * Access the cache. This is the main part of the project,
 * and should call printAction as is appropriate.
 * It should only call mem_access when absolutely necessary.
 * addr is a 16-bit LC2K word address.
 * write_flag is 0 for reads (fetch/lw) and 1 for writes (sw).
 * write_data is a word, and is only valid if write_flag is 1.
 * The return of mem_access is undefined if write_flag is 1.
 * Thus the return of cache_access is undefined if write_flag is 1.
 */
int cache_access(int addr, int write_flag, int write_data)
{
    /* The next line is a placeholder to connect the simulator to
    memory with no cache. You will remove this line and implement
    a cache which interfaces between the simulator and memory. */

    int data;
    bool cacheResolved = false;

    if (write_flag == 0) {

        cacheDataWF0(addr, &data, &cacheResolved);
        if (cacheResolved) {
            return data;
        } else if (memToCacheLoader_Empty(addr)) {
            cacheDataWF0(addr, &data, &cacheResolved);
            return data;
        } else {
            memToCacheLoader_LRU(addr);
            cacheDataWF0(addr, &data, &cacheResolved);
            return data;
        }

    } else { // if write_flag == 1
        cacheDataWF1(addr, write_data, &cacheResolved);
        if (cacheResolved) {
            return 0;
        } else if (memToCacheLoader_Empty(addr)) {
            cacheDataWF1(addr, write_data, &cacheResolved);
            return 0;
        } else {
            memToCacheLoader_LRU(addr);
            cacheDataWF1(addr, write_data, &cacheResolved);
            return 0;
        }
    }
}


/*
 * print end of run statistics like in the spec. **This is not required**,
 * but is very helpful in debugging.
 * This should be called once a halt is reached.
 * DO NOT delete this function, or else it won't compile.
 * DO NOT print $$$ in this function
 */
void printStats(void)
{
    return;
}

/*
 * Log the specifics of each cache action.
 *
 *DO NOT modify the content below.
 * address is the starting word address of the range of data being transferred.
 * size is the size of the range of data being transferred.
 * type specifies the source and destination of the data being transferred.
 *  -    cacheToProcessor: reading data from the cache to the processor
 *  -    processorToCache: writing data from the processor to the cache
 *  -    memoryToCache: reading data from the memory to the cache
 *  -    cacheToMemory: evicting cache data and writing it to the memory
 *  -    cacheToNowhere: evicting cache data and throwing it away
 */
void printAction(int address, int size, enum actionType type)
{
    printf("$$$ transferring word [%d-%d] ", address, address + size - 1);

    if (type == cacheToProcessor) {
        printf("from the cache to the processor\n");
    }
    else if (type == processorToCache) {
        printf("from the processor to the cache\n");
    }
    else if (type == memoryToCache) {
        printf("from the memory to the cache\n");
    }
    else if (type == cacheToMemory) {
        printf("from the cache to the memory\n");
    }
    else if (type == cacheToNowhere) {
        printf("from the cache to nowhere\n");
    }
    else {
        printf("Error: unrecognized action\n");
        exit(1);
    }

}

/*
 * Prints the cache based on the configurations of the struct
 * This is for debugging only and is not graded, so you may
 * modify it, but that is not recommended.
 */
void printCache(void)
{
    printf("\ncache:\n");
    for (int set = 0; set < cache.numSets; ++set) {
        printf("\tset %i:\n", set);
        for (int block = 0; block < cache.blocksPerSet; ++block) {
            printf("\t\t[ %i ]: {", block);
            for (int index = 0; index < cache.blockSize; ++index) {
                printf(" %i", cache.blocks[set * cache.blocksPerSet + block].data[index]);
            }
            printf(" }\n");
        }
    }
    printf("end cache\n");
}
