// Create a 4GB txt file that contains repeated values of "abcd"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{
    FILE *fp;
    char *filename = "4gb.txt";
    char *str = "abcd";
    int i, j, k;

    fp = fopen(filename, "w");
    if (fp == NULL)
    {
        printf("Error opening file!\n");
        exit(1);
    }

    for (i = 0; i < 1024; i++)
    {
        for (j = 0; j < 1024; j++)
        {
            for (k = 0; k < 1024; k++)
            {
                fprintf(fp, "%s", str);
            }
        }
    }

    fclose(fp);

    return 0;
}
