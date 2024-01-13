#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h> // for mkdir.

#define SB_ADDR 1024
#define BGD_SIZE 32
#define DBLOCK_PTR_SIZE 4
#define DBLOCK_PTR_COUNT 12
#define ROOT_INODE_NUM 2
#define newLine printf("\n")

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
} sb;

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
    // Assumption: max file name length is 256 chars (inclusive of '/0').
    unsigned char name[256];
};

struct Node
{
    void *data; // Generic pointer (i.e., generics).
    struct Node *next;
    struct Node *prev;
};

// Function prototypes.
struct Inode *getFileObjInode(FILE *ext2FS, char *filePath, unsigned char *fileObjName);
int extractFileObj(struct Inode *fileObjInode, unsigned char name[256], FILE *ext2FS);
int extractFile(struct Inode *fileObjInode, unsigned char name[256], FILE *ext2FS);
int enumeratePaths(struct Inode *inode, FILE *ext2FS, char *currentPath);
int isInodeDir(struct Inode *inode);
int parseSuperblock(FILE *ext2FS);
struct Inode *parseInode(uint32_t inodeNum, FILE *ext2FS);
unsigned char *readAllDataBlocks(struct Inode *inode, FILE *ext2FS);
int readDataBlock(unsigned char *data,
                  uint64_t dBlockAddr,
                  size_t *bytesToRead,
                  struct Inode *inode,
                  FILE *ext2FS);
int read12DBlockPtrs(unsigned char *data,
                     size_t *bytesToRead,
                     struct Inode *inode,
                     FILE *ext2FS);
int readSIBlockPtr(unsigned char *data,
                   uint64_t sIBlockAddr,
                   size_t *bytesToRead,
                   struct Inode *inode,
                   FILE *ext2FS);
int readDIBlockPtr(unsigned char *data,
                   uint64_t dIBlockAddr,
                   size_t *bytesToRead,
                   struct Inode *inode,
                   FILE *ext2FS);
int readTIBlockPtr(unsigned char *data,
                   uint64_t tIBlockAddr,
                   size_t *bytesToRead,
                   struct Inode *inode,
                   FILE *ext2FS);
struct Node *parseDirEntryInfo(unsigned char *data, struct Inode *inode);
struct Node *createNode(void *data);
void append(struct Node **head, void *newData);
struct Node *pop(struct Node **head);
void freeList(struct Node *head);
void *do_malloc(size_t size);
void *do_calloc(size_t nmemb, size_t size);
FILE *do_fopen(char *name, char *mode);
int do_fseek(FILE *fp, uint64_t offset, int whence);
int do_fread(void *buffer, size_t size, size_t count, FILE *file);
int do_fwrite(void *buffer, size_t size, size_t count, FILE *file);
int do_fclose(FILE *fp);
int do_mkdir(char *name);

int main(int argc, char *argv[])
{
    // GET CMD LINE ARGUMENTS -------------------------------------------------
    // Must be able to take in one or more two command line arguments.
    // Check if at least one argument is provided.
    if (argc < 2)
    {
        fprintf(stderr, "Arg1 is required");
        exit(1);
    }
    // ------------------------------------------------------------------------

    // Open the ext2 file system.
    FILE *ext2FS = do_fopen(argv[1], "rb");

    // Read and parse the superblock.
    parseSuperblock(ext2FS);
    // Print the superblock data.
    printf("totalInodes: %d\n", sb.totalInodes);
    printf("totalBlocks: %d\n", sb.totalBlocks);
    printf("blockSizeMult: %d\n", sb.blockSizeMult);
    printf("blocksPerBG: %d\n", sb.blocksPerBG);
    printf("inodesPerBG: %d\n", sb.inodesPerBG);
    printf("inodeSize: %d\n", sb.inodeSize);
    printf("blockSize: %d\n", sb.blockSize);
    newLine;

    // Read and parse the root inode.
    struct Inode *rootInode = parseInode(ROOT_INODE_NUM, ext2FS);

    // PATH ENUMERATION. ------------------------------------------------------
    if (argc == 2)
    {
        // Start path enumeration from the root directory.
        enumeratePaths(rootInode, ext2FS, "/");
    }
    // ------------------------------------------------------------------------

    // FILE SYSTEM OBJECT EXTRACTION ------------------------------------------
    if (argc == 3)
    {
        // The getFileObjInode function implicitly begins from the
        // root directory and it also verifies the file path's validity.
        unsigned char fileObjName[256] = "/";
        struct Inode *fileObjInode = getFileObjInode(ext2FS, argv[2], fileObjName);

        // Print the file object inode data.
        printf("name: %s\n", fileObjName);
        printf("isDir: %d\n", fileObjInode->type >> 12 == 4 ? 1 : 0);
        printf("file size: %d\n", fileObjInode->FSizeLower);
        printf("direct block pointers: ");
        for (int i = 0; i < 12; i++)
        {
            printf("%d ", fileObjInode->DBlockPtrs[i]);
        }
        newLine;
        printf("singly indirect block pointer: %d\n", fileObjInode->SIBlockPtr);
        printf("doubly indirect block pointer: %d\n", fileObjInode->DIBlockPtr);
        printf("triply indirect block pointer: %d\n", fileObjInode->TIBlockPtr);
        newLine;

        // Extract the file object.
        extractFileObj(fileObjInode, fileObjName, ext2FS);

        // Free the allocated memory.
        free(fileObjInode);
    }
    // ------------------------------------------------------------------------

    // Free the allocated memory.
    free(rootInode);
    do_fclose(ext2FS);

    return 0;
}

// UTILITY METHODS ------------------------------------------------------------
// This function also verifies the file path's validity by using
// the proper Directory Entry Tables.
struct Inode *getFileObjInode(FILE *ext2FS, char *filePath, unsigned char *fileObjName)
{
    // Initialize the root inode.
    struct Inode *rootInode = parseInode(ROOT_INODE_NUM, ext2FS);

    // Create a copy of the file path.
    char *filePathCopy = (char *)do_malloc(sizeof(char) * (strlen(filePath) + 1));
    strcpy(filePathCopy, filePath);

    // Tokenize the file path.
    // Note: strtok modifies the original string.
    struct Node *tokensList = NULL;
    char *token = strtok(filePathCopy, "/");

    while (token != NULL)
    {
        char *tokenCopy = (char *)do_malloc(sizeof(char) * (strlen(token) + 1));
        strcpy(tokenCopy, token);
        append(&tokensList, tokenCopy);

        token = strtok(NULL, "/");
    }

    // If the tokens list is still empty at this point,
    // then it means that it is a root directory.
    if (tokensList == NULL)
    {
        return rootInode;
    }

    // Build and initialize the seen nodes list.
    struct Node *seenNodesList = NULL;
    append(&seenNodesList, rootInode);

    // TRAVERSE THE TOKENS LIST ----------------------------------
    struct Node *currToken = tokensList;
    do
    {
        // Get the current token name.
        char *tokenName = (char *)currToken->data;

        // If tokenName is a dot (.), do nothing.
        if (strcmp(tokenName, ".") == 0)
        {
            currToken = currToken->next;
            continue;
        }

        // If tokenName is a double dot (..), pop and free the last inode
        // from the seen nodes list (given that it is not the root inode).
        if (strcmp(tokenName, "..") == 0)
        {
            if (seenNodesList->prev != seenNodesList)
            {
                struct Node *lastNode = pop(&seenNodesList);
                free(lastNode->data); // Free the inode data.
                free(lastNode);       // Free the node itself.
            }

            currToken = currToken->next;
            continue;
        }

        // Get the current directory inode
        // (i.e, the last inode in the seen nodes list).
        struct Inode *currInode = (struct Inode *)seenNodesList->prev->data;

        // Read all the data blocks.
        unsigned char *data = readAllDataBlocks(currInode, ext2FS);

        // Parse the directory entries.
        struct Node *dirEntriesList = parseDirEntryInfo(data, currInode);

        // Initialize the isFileObjFound flag.
        int isFileObjFound = 0;

        // TRAVERSE THE DIRECTORY ENTRIES -------------------------------------
        struct Node *currDirEntry = dirEntriesList;
        do
        {
            // Get the data of the current directory entry.
            struct DirEntry *dirEntry = (struct DirEntry *)currDirEntry->data;

            // If the current directory entry name is equal to the current token name,
            // then get the inode number and parse the inode.
            if (strcmp(dirEntry->name, tokenName) == 0)
            {
                // Update the isFileObjFound flag.
                isFileObjFound = 1;

                // Update the file object name.
                strcpy(fileObjName, dirEntry->name);

                // Get the inode number.
                uint32_t inodeNum = dirEntry->inodeNum;

                // Get the inode of the current directory entry.
                struct Inode *currInode = parseInode(inodeNum, ext2FS);

                // Determine if the inode is a directory or a file.
                int isDir = isInodeDir(currInode);

                // If the inode is a directory, add it to the seen nodes list.
                if (isDir)
                {
                    append(&seenNodesList, currInode);
                    break;
                }

                // If the inode is a file and it is not the last token,
                // print an error message and exit the program.
                if (!isDir && currToken->next != tokensList)
                {
                    fprintf(stderr, "INVALID PATH\n");
                    exit(-1);
                }

                // If the inode is a file and it is the last token,
                // add it to the seen nodes list.
                if (!isDir && currToken->next == tokensList)
                {
                    append(&seenNodesList, currInode);
                }

                break;
            }

            currDirEntry = currDirEntry->next;
        } while (currDirEntry != dirEntriesList);
        // --------------------------------------------------------------------

        // Free the allocated memory.
        freeList(dirEntriesList);
        free(data);

        // If the file object was not found, then this
        // means that the file path is invalid.
        if (!isFileObjFound)
        {
            fprintf(stderr, "INVALID PATH\n");
            exit(-1);
        }

        // Move to the next token.
        currToken = currToken->next;
    } while (currToken != tokensList);
    // -----------------------------------------------------------

    // Create a copy of the last inode in the seen nodes list.
    struct Inode *fileObjInode = (struct Inode *)do_malloc(sizeof(struct Inode));
    memcpy(fileObjInode, seenNodesList->prev->data, sizeof(struct Inode));

    // Determine if the last token follows the proper file path format.
    // That is, if the last token is a directory, then it must end with a slash (/).
    // If the last token is a file, then it must not end with a slash (/).
    if ((isInodeDir(fileObjInode) && filePath[strlen(filePath) - 1] != '/') ||
        (!isInodeDir(fileObjInode) && filePath[strlen(filePath) - 1] == '/'))
    {
        fprintf(stderr, "INVALID PATH\n");
        exit(-1);
    }

    // Free the allocated memory.
    freeList(tokensList);
    freeList(seenNodesList);
    free(filePathCopy);

    // Return the file object inode.
    return fileObjInode;
}

int extractFileObj(struct Inode *fileObjInode, unsigned char name[256], FILE *ext2FS)
{
    // Determine if the file object is a directory or a file.
    int isDir = isInodeDir(fileObjInode);

    // Dir
    if (isDir)
    {
        // Create "output" directory.
        do_mkdir("output");
    }
    // File
    else
    {
        // The file will be extracted in the current directory of this program.
        extractFile(fileObjInode, name, ext2FS);
    }

    return 0;
}

int extractFile(struct Inode *fileObjInode, unsigned char name[256], FILE *ext2FS)
{

    // Get the data.
    unsigned char *data = readAllDataBlocks(fileObjInode, ext2FS);

    // Open the the file in binary write mode.
    FILE *fileObj = do_fopen(name, "wb");

    // Write the data into the file.
    do_fwrite(data, sizeof(unsigned char), fileObjInode->FSizeLower, fileObj);

    // Close the file.
    do_fclose(fileObj);

    // Free the allocated memory.
    free(data);

    return 0;
}

int enumeratePaths(struct Inode *inode, FILE *ext2FS, char *currentPath)
{
    // Print the current path.
    printf("%s\n", currentPath);

    // Determine if the inode is a directory or a file.
    int isDir = isInodeDir(inode);

    // If the inode is a directory, get the directory entries.
    if (isDir)
    {
        // Read all the data blocks.
        unsigned char *data = readAllDataBlocks(inode, ext2FS);

        // Parse the directory entries.
        struct Node *dirEntriesList = parseDirEntryInfo(data, inode);

        // Traverse the directory entries.
        struct Node *current = dirEntriesList;
        do
        {
            // Get the data of the current directory entry.
            struct DirEntry *currDirEntry = (struct DirEntry *)current->data;

            // Disregard the current directory (.) and parent directory (..).
            if (strcmp(currDirEntry->name, ".") == 0 ||
                strcmp(currDirEntry->name, "..") == 0)
            {
                current = current->next;
                continue;
            }

            // Get the inode number.
            uint32_t inodeNum = currDirEntry->inodeNum;

            // Edge case. The directory "lost+found" sometimes have an inode number of 0
            if (inodeNum == 0)
            {
                current = current->next;
                continue;
            }

            // Get the inode of the current directory entry.
            struct Inode *currInode = parseInode(inodeNum, ext2FS);

            // Get the file object name.
            // Note: nameLen does not include the null terminator.
            // Due to this, +2 is for the null terminator and the potential slash (/).
            char *fileObjName = (char *)do_malloc(sizeof(char) * (currDirEntry->nameLen + 2));
            strcpy(fileObjName, currDirEntry->name);

            // Add the slash (/) at the end of the file object name if it is a directory.
            if (currInode->type >> 12 == 4)
            {
                strcat(fileObjName, "/\0");
            }

            // Append the file object name into the current path.
            char *newPath = (char *)do_malloc(sizeof(char) * (strlen(currentPath) + strlen(fileObjName) + 1));
            strcpy(newPath, currentPath);
            strcat(newPath, fileObjName);

            // Recursively enumerate the paths.
            enumeratePaths(currInode, ext2FS, newPath);

            // Free the allocated memory.
            free(currInode);
            free(fileObjName);
            free(newPath);

            current = current->next;
        } while (current != dirEntriesList);

        // Free the allocated memory.
        freeList(dirEntriesList);
        free(data);
    }

    return 0;
}

int isInodeDir(struct Inode *inode)
{
    return inode->type >> 12 == 4 ? 1 : 0;
}

int parseSuperblock(FILE *ext2FS)
{
    do_fseek(ext2FS, SB_ADDR, SEEK_SET);
    do_fread(&sb.totalInodes, sizeof(sb.totalInodes), 1, ext2FS);

    do_fseek(ext2FS, SB_ADDR + 4, SEEK_SET);
    do_fread(&sb.totalBlocks, sizeof(sb.totalBlocks), 1, ext2FS);

    do_fseek(ext2FS, SB_ADDR + 24, SEEK_SET);
    do_fread(&sb.blockSizeMult, sizeof(sb.blockSizeMult), 1, ext2FS);

    do_fseek(ext2FS, SB_ADDR + 32, SEEK_SET);
    do_fread(&sb.blocksPerBG, sizeof(sb.blocksPerBG), 1, ext2FS);

    do_fseek(ext2FS, SB_ADDR + 40, SEEK_SET);
    do_fread(&sb.inodesPerBG, sizeof(sb.inodesPerBG), 1, ext2FS);

    do_fseek(ext2FS, SB_ADDR + 88, SEEK_SET);
    do_fread(&sb.inodeSize, sizeof(sb.inodeSize), 1, ext2FS);

    // Calculate the block size.
    sb.blockSize = 1024 << sb.blockSizeMult;

    return 0;
}

struct Inode *parseInode(uint32_t inodeNum, FILE *ext2FS)
{
    // COMPUTE FOR THE INODE ADDRESS (BYTE OFFSET) ------------------------------
    // Determine which block group the corresponding inode is in.
    uint32_t inodeBGNum = (inodeNum - 1) / sb.inodesPerBG;

    // Determine the index of the inode in the inode table.
    uint32_t inodeIndex = (inodeNum - 1) % sb.inodesPerBG;

    // Get the block group descriptor table entry address.
    uint64_t bgdtEntryAddr = sb.blockSize + (inodeBGNum * BGD_SIZE);

    // Get the inode table address.
    uint32_t relInodeTableAddr;
    do_fseek(ext2FS, bgdtEntryAddr + 8, SEEK_SET);
    do_fread(&relInodeTableAddr, sizeof(relInodeTableAddr), 1, ext2FS);
    uint64_t inodeTableAddr = relInodeTableAddr * sb.blockSize;

    // Get the inode address.
    uint64_t inodeAddr = inodeTableAddr + (inodeIndex * sb.inodeSize);
    // -------------------------------------------------------------------------

    // PARSE THE INODE AND BUILD ITS INODE STRUCT ------------------------------
    // Allocate memory for the inode struct.
    struct Inode *inode = (struct Inode *)do_malloc(sizeof(struct Inode));

    // Get the type.
    do_fseek(ext2FS, inodeAddr, SEEK_SET);
    do_fread(&inode->type, sizeof(inode->type), 1, ext2FS);

    // Get lower 32 bits of the file size.
    do_fseek(ext2FS, inodeAddr + 4, SEEK_SET);
    do_fread(&inode->FSizeLower, sizeof(inode->FSizeLower), 1, ext2FS);

    // Get the 12 direct block pointers.
    do_fseek(ext2FS, inodeAddr + 40, SEEK_SET);
    for (int i = 0; i < 12; i++)
    {
        do_fread(&inode->DBlockPtrs[i], sizeof(inode->DBlockPtrs[i]), 1, ext2FS);
    }

    // Get the singly indirect block pointer.
    do_fseek(ext2FS, inodeAddr + 88, SEEK_SET);
    do_fread(&inode->SIBlockPtr, sizeof(inode->SIBlockPtr), 1, ext2FS);

    // Get the doubly indirect block pointer.
    do_fseek(ext2FS, inodeAddr + 92, SEEK_SET);
    do_fread(&inode->DIBlockPtr, sizeof(inode->DIBlockPtr), 1, ext2FS);

    // Get the triply indirect block pointer.
    do_fseek(ext2FS, inodeAddr + 96, SEEK_SET);
    do_fread(&inode->TIBlockPtr, sizeof(inode->TIBlockPtr), 1, ext2FS);
    // -------------------------------------------------------------------------

    return inode;
}

// Get all the block data pointed by the 12 direct block pointers,
// singly indirect block pointer, and doubly indirect block pointer.
unsigned char *readAllDataBlocks(struct Inode *inode, FILE *ext2FS)
{
    // Allocate memory for the data.
    unsigned char *data = (unsigned char *)do_calloc(inode->FSizeLower, sizeof(unsigned char));

    // Read all the data blocks.
    size_t readBytes = 0;
    read12DBlockPtrs(data, &readBytes, inode, ext2FS);
    readSIBlockPtr(data, inode->SIBlockPtr * sb.blockSize, &readBytes, inode, ext2FS);
    readDIBlockPtr(data, inode->DIBlockPtr * sb.blockSize, &readBytes, inode, ext2FS);
    readTIBlockPtr(data, inode->TIBlockPtr * sb.blockSize, &readBytes, inode, ext2FS);

    return data;
}

int readDataBlock(unsigned char *data,
                  uint64_t dBlockAddr,
                  size_t *readBytes,
                  struct Inode *inode,
                  FILE *ext2FS)
{
    // Read byte by byte (per data block).
    do_fseek(ext2FS, dBlockAddr, SEEK_SET);
    for (int i = 0; i < sb.blockSize; i++)
    {
        if (*readBytes == inode->FSizeLower)
        {
            break;
        }

        do_fread(&data[*readBytes], 1, 1, ext2FS);
        *readBytes += 1;
    }

    return 0;
}

int read12DBlockPtrs(unsigned char *data,
                     size_t *readBytes,
                     struct Inode *inode,
                     FILE *ext2FS)
{
    for (int i = 0; i < DBLOCK_PTR_COUNT; i++)
    {
        if (*readBytes == inode->FSizeLower)
        {
            break;
        }

        // Get the direct block pointer.
        uint32_t dBlockPtr = inode->DBlockPtrs[i];

        // Get the direct block address.
        uint64_t dBlockAddr = dBlockPtr * sb.blockSize;

        // Read the corresponding direct data block.
        readDataBlock(data, dBlockAddr, readBytes, inode, ext2FS);
    }

    return 0;
}

int readSIBlockPtr(unsigned char *data,
                   uint64_t sIBlockAddr,
                   size_t *readBytes,
                   struct Inode *inode,
                   FILE *ext2FS)
{
    // Read byte by byte (per data block).
    int numOfDBlockPtrs = sb.blockSize / DBLOCK_PTR_SIZE;
    for (int i = 0; i < numOfDBlockPtrs; i++)
    {
        // Adjust the file pointer to the corresponding direct block.
        do_fseek(ext2FS, sIBlockAddr + (4 * i), SEEK_SET);

        if (*readBytes == inode->FSizeLower)
        {
            break;
        }

        // Get the direct block pointer (four bytes - in little endian).
        uint32_t dBlockPtr;
        do_fread(&dBlockPtr, sizeof(dBlockPtr), 1, ext2FS);

        // Get the direct block address.
        uint64_t dBlockAddr = dBlockPtr * sb.blockSize;

        // Read the corresponding direct data block.
        readDataBlock(data, dBlockAddr, readBytes, inode, ext2FS);
    }

    return 0;
}

int readDIBlockPtr(unsigned char *data,
                   uint64_t dIBlockAddr,
                   size_t *readBytes,
                   struct Inode *inode,
                   FILE *ext2FS)
{
    // Read byte by byte (per data block).
    int numOfSIBlockPtrs = sb.blockSize / DBLOCK_PTR_SIZE;
    for (int i = 0; i < numOfSIBlockPtrs; i++)
    {
        // Adjust the file pointer to the corresponding singly indirect block.
        do_fseek(ext2FS, dIBlockAddr + (4 * i), SEEK_SET);

        if (*readBytes == inode->FSizeLower)
        {
            break;
        }

        // Get the singly indirect block pointer (four bytes - in little endian).
        uint32_t sIBlockPtr;
        do_fread(&sIBlockPtr, sizeof(sIBlockPtr), 1, ext2FS);

        // Get the singly indirect block address.
        uint64_t sIBlockAddr = sIBlockPtr * sb.blockSize;

        // Read the corresponding singly indirect data block.
        readSIBlockPtr(data, sIBlockAddr, readBytes, inode, ext2FS);
    }

    return 0;
}

int readTIBlockPtr(unsigned char *data,
                   uint64_t tIBlockAddr,
                   size_t *readBytes,
                   struct Inode *inode,
                   FILE *ext2FS)
{
    // Read byte by byte (per data block).
    int numOfDIBlockPtrs = sb.blockSize / DBLOCK_PTR_SIZE;
    for (int i = 0; i < numOfDIBlockPtrs; i++)
    {
        // Adjust the file pointer to the corresponding doubly indirect block.
        do_fseek(ext2FS, tIBlockAddr + (4 * i), SEEK_SET);

        if (*readBytes == inode->FSizeLower)
        {
            break;
        }

        // Get the doubly indirect block pointer (four bytes - in little endian).
        uint32_t dIBlockPtr;
        do_fread(&dIBlockPtr, sizeof(dIBlockPtr), 1, ext2FS);

        // Get the doubly indirect block address.
        uint64_t dIBlockAddr = dIBlockPtr * sb.blockSize;

        // Read the corresponding doubly indirect data block.
        readDIBlockPtr(data, dIBlockAddr, readBytes, inode, ext2FS);
    }

    return 0;
}

// Parse the data block/s (when it is a directory entry information).
// Note: A directory is never empty (i.e., it always has at least two entries:
//       the current directory (.) and the parent directory (..)).
struct Node *parseDirEntryInfo(unsigned char *data, struct Inode *inode)
{
    struct Node *dirEntriesList = NULL;

    // Traverse the every byte of data.
    uint32_t i = 0;
    while (i < inode->FSizeLower)
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

    return dirEntriesList;
}

// Circular doubly linked list.
struct Node *createNode(void *data)
{
    struct Node *newNode = (struct Node *)do_malloc(sizeof(struct Node));
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

struct Node *pop(struct Node **head)
{
    if (*head == NULL)
    {
        printf("List is empty, cannot pop.\n");
        return NULL;
    }

    struct Node *lastNode = (*head)->prev;
    if (*head == lastNode)
    {
        *head = NULL;
    }
    else
    {
        lastNode->prev->next = *head;
        (*head)->prev = lastNode->prev;
    }

    return lastNode;
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

void *do_malloc(size_t size)
{
    void *ptr = malloc(size);
    if (ptr == NULL)
    {
        perror("malloc failed");
        exit(1);
    }

    return ptr;
}

void *do_calloc(size_t nmemb, size_t size)
{
    void *ptr = calloc(nmemb, size);
    if (ptr == NULL)
    {
        perror("calloc failed");
        exit(1);
    }

    return ptr;
}

FILE *do_fopen(char *name, char *mode)
{
    FILE *fp = fopen(name, mode);
    if (fp == NULL)
    {
        perror("fopen failed");
        exit(1);
    }

    return fp;
}

int do_fseek(FILE *fp, uint64_t offset, int whence)
{
    if (fseek(fp, offset, whence) != 0)
    {
        perror("fseek failed");
        exit(1);
    }

    return 0;
}

int do_fread(void *buffer, size_t size, size_t count, FILE *file)
{
    if (fread(buffer, size, count, file) != count)
    {
        fprintf(stderr, "fread failed\n");
        exit(1);
    }

    return 0;
}

int do_fwrite(void *buffer, size_t size, size_t count, FILE *file)
{
    if (fwrite(buffer, size, count, file) != count)
    {
        fprintf(stderr, "fwrite failed\n");
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

int do_mkdir(char *name)
{
    // Create a new directory with read, write, and execute permissions
    // for owner, group, and others.
    if (mkdir(name, 0777) != 0)
    {
        perror("mkdir failed");
        exit(1);
    }

    return 0;
}
