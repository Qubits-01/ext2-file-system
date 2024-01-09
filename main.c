#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>

#define ROOT_INODE_NUM 2
#define ADDR_SB 1024

int do_lseek(int fd, int offset, int whence);

// superblock struct.
struct SB
{
    uint32_t totalInodes;
    uint32_t totalBlocks;
    uint32_t blockSizeMult;
    uint32_t blocksPerBG;
    uint32_t inodesPerBG;
    uint32_t inodeSize;
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
    printf("ext2 file path: %s\n", ext2FP);

    // Check if a second argument is provided
    if (argc == 3)
    {
        // Get the absolute file path.
        char *abs_file_path = argv[2];
        printf("absolute file path: %s\n", abs_file_path);
    }
    // -------------------------------------------------------------------------

    // Open the ext2 file system for reading (use open not fopen).
    int fd = open(ext2FP, O_RDONLY);
    if (fd < 0)
    {
        perror("Failed to open ext2 file system");
        exit(1);
    }

    // Print the bytes 1024 to 2047 in hexadecimal.
    unsigned char buf[1024];
    do_lseek(fd, 1024, SEEK_SET);
    for (int i = 0; i < 1024; i++)
    {
        read(fd, &buf[i], 1);
        printf("%02x ", buf[i]);
    }
    printf("\n");

    // Read the main superblock.
    struct SB sb;

    do_lseek(fd, ADDR_SB, SEEK_SET);
    read(fd, &sb.totalInodes, sizeof(sb.totalInodes));

    do_lseek(fd, ADDR_SB + 4, SEEK_SET);
    read(fd, &sb.totalBlocks, sizeof(sb.totalBlocks));

    do_lseek(fd, ADDR_SB + 24, SEEK_SET);
    read(fd, &sb.blockSizeMult, sizeof(sb.blockSizeMult));

    do_lseek(fd, ADDR_SB + 32, SEEK_SET);
    read(fd, &sb.blocksPerBG, sizeof(sb.blocksPerBG));

    do_lseek(fd, ADDR_SB + 40, SEEK_SET);
    read(fd, &sb.inodesPerBG, sizeof(sb.inodesPerBG));

    do_lseek(fd, ADDR_SB + 88, SEEK_SET);
    read(fd, &sb.inodeSize, sizeof(sb.inodeSize));

    // Print the superblock information.
    printf("totalInodes: %d\n", sb.totalInodes);
    printf("totalBlocks: %d\n", sb.totalBlocks);
    printf("blockSizeMult: %d\n", sb.blockSizeMult);
    printf("blocksPerBG: %d\n", sb.blocksPerBG);
    printf("inodesPerBG: %d\n", sb.inodesPerBG);
    printf("inodeSize: %d\n", sb.inodeSize);

    // Close the ext2 file system.
    close(fd);

    return 0;
}

// Utility methods.
int do_lseek(int fd, int offset, int whence)
{
    int ret = lseek(fd, offset, whence);
    if (ret < 0)
    {
        perror("Failed to do lseek");
        exit(1);
    }
    return ret;
}
