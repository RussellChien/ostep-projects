#include <stdio.h>
#include <stdlib.h>

void wcat_file(FILE *fp)
{
    // check that fopen was successful
    if (fp == NULL)
    {
        printf("wcat: cannot open file\n");
        exit(1);
    }

    char buffer[4096]; // store lines from file in buffer

    // use fgets to read and print lines
    while (fgets(buffer, sizeof(buffer), fp) != NULL)
    {
        printf("%s", buffer);
    }

    fclose(fp);
}

int main(int argc, char *argv[])
{
    // print out each file's contents
    for (int i = 1; i < argc; i++)
    {
        FILE *fp = fopen(argv[i], "r");
        wcat_file(fp);
    }

    // if no file is specified, return 0
    return 0;
}
