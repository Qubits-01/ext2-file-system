#include <stdio.h>

#define ROOT_INODE_NUM 2

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
    char *ext2_file_path = argv[1];
    printf("ext2 file path: %s\n", ext2_file_path);

    // Check if a second argument is provided
    if (argc == 3)
    {
        // Get the absolute file path.
        char *abs_file_path = argv[2];
        printf("absolute file path: %s\n", abs_file_path);
    }
    // -------------------------------------------------------------------------

    return 0;
}
