#include <stdio.h>
#include <stdint.h>

#define ROOT_INODE_NUM 2

// superblock struct.
struct Superblock
{
    uint32_t totalInodes;
    uint32_t totalBlocks;
    uint32_t blockSizeMult;
    uint32_t blocksPerBG;
    uint32_t inodesPerBG;
};

int main(int argc, char *argv[])
{
    // GET CMD LINE ARGUMENTS --------------------------------------------------
    // Must be able to take in one or more two command line arguments.
    // Check if at least one argument is provided
    if (argc < 2)
    {
        printf("[ERROR] No argument/s provided.\n");
        return 1;
    }

    // Get the ext2 file system file path.
    char *ext2FP = argv[1];
    printf("ext2 file path: %s\n", ext2FP);

    // Check if a second argument is provided
    if (argc == 3)
    {
        // Get the absolute file path.
        char *abs_file_path = argv[2];
        printf("absolute file path: %s\n", abs_file_path);
    }
    // -------------------------------------------------------------------------

    // Open the ext2 file system file for reading.
    FILE *ext2FS = fopen(ext2FP, "r");
    if (ext2FS == NULL)
    {
        printf("[ERROR] Unable to open the ext2 file system.\n");
        return 1;
    }

    // Read the superblock.
    struct Superblock superblock;
    fseek(ext2FS, 1024, SEEK_SET);

    // Close the ext2 file system.
    fclose(ext2FS);

    return 0;
}
