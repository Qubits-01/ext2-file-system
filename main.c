#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define SB_ADDR 1024
#define BGD_SIZE 32
#define ROOT_INODE_NUM 2

int do_fseek(FILE *fp, int offset, int whence);
int do_fclose(FILE *fp);

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
    uint32_t fileSizeLower;
    uint32_t fileSizeUpper;
    uint32_t directBlockPtrs[12];
    uint32_t singlyIBlockPtr;
    uint32_t doublyIBlockPtr;
    uint32_t triplyIBlockPtr;
};

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
    // Get the type.
    uint16_t type;
    do_fseek(ext2FS, inodeAddr, SEEK_SET);
    fread(&type, sizeof(type), 1, ext2FS);
    printf("type: %04x\n", type);

    // Get lower 32 bits of the file size.
    uint32_t fileSizeLower;
    do_fseek(ext2FS, inodeAddr + 4, SEEK_SET);
    fread(&fileSizeLower, sizeof(fileSizeLower), 1, ext2FS);
    printf("fileSizeLower: %d\n", fileSizeLower);

    // Get the 12 direct block pointers.
    uint32_t directBlockPtrs[12];
    do_fseek(ext2FS, inodeAddr + 40, SEEK_SET);
    for (int i = 0; i < 12; i++)
    {
        fread(&directBlockPtrs[i], sizeof(directBlockPtrs[i]), 1, ext2FS);
        printf("directBlockPtrs[%d]: %d\n", i, directBlockPtrs[i]);
    }

    // Get the singly indirect block pointer.
    uint32_t singlyIBlockPtr;
    do_fseek(ext2FS, inodeAddr + 88, SEEK_SET);
    fread(&singlyIBlockPtr, sizeof(singlyIBlockPtr), 1, ext2FS);
    printf("singlyIBlockPtr: %d\n", singlyIBlockPtr);

    // Get the doubly indirect block pointer.
    uint32_t doublyIBlockPtr;
    do_fseek(ext2FS, inodeAddr + 92, SEEK_SET);
    fread(&doublyIBlockPtr, sizeof(doublyIBlockPtr), 1, ext2FS);
    printf("doublyIBlockPtr: %d\n", doublyIBlockPtr);

    // Get the triply indirect block pointer.
    uint32_t triplyIBlockPtr;
    do_fseek(ext2FS, inodeAddr + 96, SEEK_SET);
    fread(&triplyIBlockPtr, sizeof(triplyIBlockPtr), 1, ext2FS);
    printf("triplyIBlockPtr: %d\n", triplyIBlockPtr);

    // Close the ext2 file system.
    do_fclose(ext2FS);

    return 0;
}

// Utility methods.
// int getBGNum(int inodeNum, int inodesPerBG)
// {
//     return (inodeNum - 1) / inodesPerBG;
// }

// int getInodeIndex(int inodeNum, int inodesPerBG)
// {
//     return (inodeNum - 1) % inodesPerBG;
// }

// int getInodeTableAddr(int inodeBGNum)
// {
//     // Get the corresponding block group descriptor table entry address.
//     int bgdtEntryAddr = BGDT_ADDR + (inodeBGNum * BGD_SIZE);

//     return 0;
// }

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
