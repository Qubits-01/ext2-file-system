#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>

#define ROOT_INODE_NUM 2
#define ADDR_SB 1024

int do_seek(FILE *fp, int offset, int whence);

// superblock struct.
struct SB
{
    uint32_t totalInodes;
    uint32_t totalBlocks;
    uint32_t blockSizeMult;
    uint32_t blocksPerBG;
    uint32_t inodesPerBG;
    uint16_t inodeSize;
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

    // Read the superblock.
    struct SB sb;

    do_seek(ext2FS, ADDR_SB, SEEK_SET);
    fread(&sb.totalInodes, sizeof(sb.totalInodes), 1, ext2FS);

    do_seek(ext2FS, ADDR_SB + 4, SEEK_SET);
    fread(&sb.totalBlocks, sizeof(sb.totalBlocks), 1, ext2FS);

    do_seek(ext2FS, ADDR_SB + 24, SEEK_SET);
    fread(&sb.blockSizeMult, sizeof(sb.blockSizeMult), 1, ext2FS);

    do_seek(ext2FS, ADDR_SB + 32, SEEK_SET);
    fread(&sb.blocksPerBG, sizeof(sb.blocksPerBG), 1, ext2FS);

    do_seek(ext2FS, ADDR_SB + 40, SEEK_SET);
    fread(&sb.inodesPerBG, sizeof(sb.inodesPerBG), 1, ext2FS);

    do_seek(ext2FS, ADDR_SB + 88, SEEK_SET);
    fread(&sb.inodeSize, sizeof(sb.inodeSize), 1, ext2FS);

    // Print the superblock info.
    printf("totalInodes: %d\n", sb.totalInodes);
    printf("totalBlocks: %d\n", sb.totalBlocks);
    printf("blockSizeMult: %d\n", sb.blockSizeMult);
    printf("blocksPerBG: %d\n", sb.blocksPerBG);
    printf("inodesPerBG: %d\n", sb.inodesPerBG);
    printf("inodeSize: %d\n", sb.inodeSize);

    // Close the ext2 file system.
    fclose(ext2FS);

    return 0;
}

// Utility methods.
int do_seek(FILE *fp, int offset, int whence)
{
    if (fseek(fp, offset, whence) != 0)
    {
        perror("fseek failed");
        exit(1);
    }

    return 0;
}
