#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define SB_ADDR 1024
#define BGD_SIZE 32
#define ROOT_INODE_NUM 2

// superblock struct.
struct SB
{
    uint32_t totalInodes;
    uint32_t totalBlocks;
    uint32_t blockSizeMult;
    uint32_t blocksPerBG;
    uint32_t inodesPerBG;
    uint16_t inodeSize;

    // Other derived values that is relevant to the program.
    uint32_t blockSize;
};

struct Inode
{
    uint16_t type;
    uint32_t FSizeLower;     // Lower 32 bits of the file size.
    uint32_t DBlockPtrs[12]; // Direct block pointers.
    uint32_t SIBlockPtr;     // Singly indirect block pointer.
    uint32_t DIBlockPtr;     // Doubly indirect block pointer.
    uint32_t TIBlockPtr;     // Triply indirect block pointer.
};

struct DirEntry
{
    uint32_t inodeNum;
    uint16_t entrySize;
    uint8_t nameLen;
    // Assumption: max file name length is 255 chars (inclusive of '/0').
    unsigned char name[255];
};

struct Node
{
    void *data; // Generic pointer (i.e., generics).
    struct Node *next;
    struct Node *prev;
};

struct Node *createNode(void *data);
void append(struct Node **head, void *newData);
void freeList(struct Node *head);
int do_fseek(FILE *fp, int offset, int whence);
int do_fclose(FILE *fp);

int main(int argc, char *argv[])
{
    // GET CMD LINE ARGUMENTS --------------------------------------------------
    // Must be able to take in one or more two command line arguments.
    // Check if at least one argument is provided
    if (argc < 2)
    {
        perror("Must provide at least one argument");
        exit(1);
    }

    // Get the ext2 file system file path.
    char *ext2FP = argv[1];
    printf("arg1: %s\n", ext2FP);

    // Check if a second argument is provided
    if (argc == 3)
    {
        // Get the absolute file path.
        char *abs_file_path = argv[2];
        printf("arg2: %s\n", abs_file_path);
    }
    // -------------------------------------------------------------------------

    // Open the ext2 file system.
    FILE *ext2FS = fopen(ext2FP, "rb");
    if (ext2FS == NULL)
    {
        perror("fopen failed");
        exit(1);
    }

    // PARSE THE SUPERBLOCK ----------------------------------------------------
    // Read the superblock.
    struct SB sb;

    do_fseek(ext2FS, SB_ADDR, SEEK_SET);
    fread(&sb.totalInodes, sizeof(sb.totalInodes), 1, ext2FS);

    do_fseek(ext2FS, SB_ADDR + 4, SEEK_SET);
    fread(&sb.totalBlocks, sizeof(sb.totalBlocks), 1, ext2FS);

    do_fseek(ext2FS, SB_ADDR + 24, SEEK_SET);
    fread(&sb.blockSizeMult, sizeof(sb.blockSizeMult), 1, ext2FS);

    do_fseek(ext2FS, SB_ADDR + 32, SEEK_SET);
    fread(&sb.blocksPerBG, sizeof(sb.blocksPerBG), 1, ext2FS);

    do_fseek(ext2FS, SB_ADDR + 40, SEEK_SET);
    fread(&sb.inodesPerBG, sizeof(sb.inodesPerBG), 1, ext2FS);

    do_fseek(ext2FS, SB_ADDR + 88, SEEK_SET);
    fread(&sb.inodeSize, sizeof(sb.inodeSize), 1, ext2FS);

    // Calculate the block size.
    sb.blockSize = 1024 << sb.blockSizeMult;
    // -------------------------------------------------------------------------

    // Print the superblock info.
    printf("totalInodes: %d\n", sb.totalInodes);
    printf("totalBlocks: %d\n", sb.totalBlocks);
    printf("blockSizeMult: %d\n", sb.blockSizeMult);
    printf("blocksPerBG: %d\n", sb.blocksPerBG);
    printf("inodesPerBG: %d\n", sb.inodesPerBG);
    printf("inodeSize: %d\n", sb.inodeSize);
    printf("blockSize: %d\n", sb.blockSize);

    // TODO: Generalize the inode access and parsing.

    // Determine which block group the corresponding inode is in.
    int inodeBGNum = (ROOT_INODE_NUM - 1) / sb.inodesPerBG;
    printf("inodeBGNum: %d\n", inodeBGNum);

    // Determine the index of the inode in the inode table.
    int inodeIndex = (ROOT_INODE_NUM - 1) % sb.inodesPerBG;
    printf("inodeIndex: %d\n", inodeIndex);

    // Get the block group descriptor table entry address.
    int bgdtEntryAddr = sb.blockSize + (inodeBGNum * BGD_SIZE);
    printf("bgdtEntryAddr: %d\n", bgdtEntryAddr);

    // Get the inode table address.
    uint32_t relInodeTableAddr;
    do_fseek(ext2FS, bgdtEntryAddr + 8, SEEK_SET);
    fread(&relInodeTableAddr, sizeof(relInodeTableAddr), 1, ext2FS);
    int inodeTableAddr = relInodeTableAddr * sb.blockSize;
    printf("RelInodeTableAddr (2): %d\n", relInodeTableAddr);
    printf("inodeTableAddr (2): %d\n", inodeTableAddr);

    // Get the inode address.
    uint32_t inodeAddr = inodeTableAddr + (inodeIndex * sb.inodeSize);

    // Parse the inode.
    struct Inode inode2;

    // Get the type.
    uint16_t type;
    do_fseek(ext2FS, inodeAddr, SEEK_SET);
    fread(&inode2.type, sizeof(inode2.type), 1, ext2FS);
    printf("type: %d\n", inode2.type);

    // Get lower 32 bits of the file size.
    do_fseek(ext2FS, inodeAddr + 4, SEEK_SET);
    fread(&inode2.FSizeLower, sizeof(inode2.FSizeLower), 1, ext2FS);
    printf("FSizeLower: %d\n", inode2.FSizeLower);

    // Get the 12 direct block pointers.
    do_fseek(ext2FS, inodeAddr + 40, SEEK_SET);
    for (int i = 0; i < 12; i++)
    {
        fread(&inode2.DBlockPtrs[i], sizeof(inode2.DBlockPtrs[i]), 1, ext2FS);
        printf("directBlockPtrs[%d]: %d\n", i, inode2.DBlockPtrs[i]);
    }

    // Get the singly indirect block pointer.
    uint32_t singlyIBlockPtr;
    do_fseek(ext2FS, inodeAddr + 88, SEEK_SET);
    fread(&inode2.SIBlockPtr, sizeof(inode2.SIBlockPtr), 1, ext2FS);
    printf("singlyIBlockPtr: %d\n", inode2.SIBlockPtr);

    // Get the doubly indirect block pointer.
    uint32_t doublyIBlockPtr;
    do_fseek(ext2FS, inodeAddr + 92, SEEK_SET);
    fread(&inode2.DIBlockPtr, sizeof(inode2.DIBlockPtr), 1, ext2FS);
    printf("doublyIBlockPtr: %d\n", inode2.DIBlockPtr);

    // Get the triply indirect block pointer.
    uint32_t triplyIBlockPtr;
    do_fseek(ext2FS, inodeAddr + 96, SEEK_SET);
    fread(&inode2.TIBlockPtr, sizeof(inode2.TIBlockPtr), 1, ext2FS);
    printf("triplyIBlockPtr: %d\n", inode2.TIBlockPtr);

    // Read the corresponding data block/s.
    // Read the direct blocks.
    // Dynamically allocate memory (using the fileSizeLower).
    unsigned char *data = (unsigned char *)malloc(inode2.FSizeLower * sizeof(unsigned char));
    for (int i = 0; i < 12; i++)
    {
        if (inode2.DBlockPtrs[i] == 0)
        {
            break;
        }

        // Get the direct block pointer.
        uint32_t DBlockPtr = inode2.DBlockPtrs[i];
        printf("directBlockPtr: %d\n", DBlockPtr);

        // Get the direct block address.
        uint32_t DBlockAddr = DBlockPtr * sb.blockSize;
        printf("directBlockAddr: %d\n", DBlockAddr);

        // Read the entire direct block.
        do_fseek(ext2FS, DBlockAddr, SEEK_SET);
        fread(&data[i * sb.blockSize], sb.blockSize, 1, ext2FS);

        // // Print the data (per block).
        // printf("Direct Block %d:\n", i);
        // // Print per byte.
        // for (int j = 0; j < sb.blockSize; j++)
        // {
        //     printf("%02x ", data[i * sb.blockSize + j]);
        // }
    }
    printf("\n");

    // TODO: Read the singly indirect block.
    // TODO: Read the doubly indirect block.
    // TODO: Read the triply indirect block.

    // TODO: Generalize the directory entry access and parsing.

    struct Node *dirEntriesList = NULL;
    struct DirEntry *dirEntry;

    // Parse (if dir content/s).
    // Traverse every byte of data.
    uint32_t i = 0;
    while (i < inode2.FSizeLower)
    {
        struct DirEntry *dirEntry = (struct DirEntry *)malloc(sizeof(struct DirEntry));

        // Get the inode number (byte 0 to byte 3 - in little endian).
        dirEntry->inodeNum = data[i] | data[i + 1] << 8 | data[i + 2] << 16 | data[i + 3] << 24;
        i += 4;

        // Get the entry size (byte 4 to byte 5 - in little endian).
        dirEntry->entrySize = data[i] | data[i + 1] << 8;
        i += 2;

        // Get the name length (byte 6).
        dirEntry->nameLen = data[i];
        i += 1;
        i += 1; // Skip the file type byte.

        // Get the name (byte 8 to byte 8 + nameLen - 1).
        for (int j = 0; j < dirEntry->nameLen; j++)
        {
            dirEntry->name[j] = data[i + j];
        }
        // Add the null terminator.
        dirEntry->name[dirEntry->nameLen] = '\0';

        // Append the directory entry into the linked list.
        append(&dirEntriesList, dirEntry);

        // Update i for the next directory entry.
        i += dirEntry->entrySize - 8;
    }

    // Print the directory entries.
    struct Node *temp = dirEntriesList;
    do
    {
        struct DirEntry *dirEntry = (struct DirEntry *)temp->data;

        printf("inodeNum: %d\n", dirEntry->inodeNum);
        printf("entrySize: %d\n", dirEntry->entrySize);
        printf("nameLen: %d\n", dirEntry->nameLen);
        printf("name: %s\n\n", dirEntry->name);

        temp = temp->next;
    } while (temp != dirEntriesList);

    // TODO: Parse (if file content/s).

    // Free the dynamically allocated memory.
    free(data);               // Free the data gathered from the data blocks.
    freeList(dirEntriesList); // Free the linked list of directory entries.

    // Close the ext2 file system.
    do_fclose(ext2FS);

    return 0;
}

// Utility methods.
// Circular doubly linked list.
struct Node *createNode(void *data)
{
    struct Node *newNode = (struct Node *)malloc(sizeof(struct Node));
    if (newNode == NULL)
    {
        perror("malloc failed");
        exit(1);
    }

    newNode->data = data;
    newNode->next = NULL;
    newNode->prev = NULL;

    return newNode;
}

void append(struct Node **head, void *data)
{
    struct Node *newNode = createNode(data);

    // If the list is empty, make the new node as head.
    if (*head == NULL)
    {
        *head = newNode;
        newNode->next = newNode;
        newNode->prev = newNode;
    }
    else
    {
        // Insert the new node at the end.
        struct Node *last = (*head)->prev;
        last->next = newNode;
        newNode->prev = last;
        newNode->next = *head;
        (*head)->prev = newNode;
    }
}

void freeList(struct Node *head)
{
    if (head == NULL)
    {
        return;
    }

    struct Node *current = head;
    struct Node *next;

    do
    {
        next = current->next;
        free(current->data);
        free(current);
        current = next;
    } while (current != head);
}

int do_fseek(FILE *fp, int offset, int whence)
{
    if (fseek(fp, offset, whence) != 0)
    {
        perror("fseek failed");
        exit(1);
    }

    return 0;
}

int do_fclose(FILE *fp)
{
    if (fclose(fp) != 0)
    {
        perror("fclose failed");
        exit(1);
    }

    return 0;
}
