#include <stdio.h>
#include <stdlib.h>

void wunzip_file(FILE *input_file)
{
    // check that fopen was successful
    if (input_file == NULL)
    {
        printf("wunzip: cannot open file\n");
        exit(1);
    }

    int run_length = 0;
    int current_char = 0;

    // read and process the file
    while (fread(&run_length, sizeof(int), 1, input_file) != 0)
    {
        // get current char from input_file and print it out based on the run_length
        current_char = fgetc(input_file);
        for (int j = 0; j < run_length; j++)
        {
            putchar(current_char);
        }
    }
}

int main(int argc, char *argv[])
{
    // if no files specified, exit with 1
    if (argc < 2)
    {
        printf("wunzip: file1 [file2 ...]\n");
        return 1;
    }

    // loop through each file
    for (int i = 1; i < argc; i++)
    {
        FILE *input_file = fopen(argv[i], "rb"); // open in binary mode

        wunzip_file(input_file);

        fclose(input_file);
    }

    return 0;
}