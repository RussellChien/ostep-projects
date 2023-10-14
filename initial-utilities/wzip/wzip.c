#include <stdio.h>
#include <stdlib.h>

void wzip_files(int argc, char** argv)
{
    FILE *output_file = stdout;

    int prev_char = -1;
    int count = 0;
    int current_char = 0;

    // loop through each file
    for (int i = 1; i < argc; i++)
    {
        FILE *input_file = fopen(argv[i], "r");

        // check that fopen was successful
        if (input_file == NULL)
        {
            printf("wzip: cannot open file\n");
            exit(1);
        }

        // process each character from the file
        while ((current_char = fgetc(input_file)) != EOF)
        {
            // start counting char
            if (prev_char == -1)
            {
                prev_char = current_char;
                count = 1;
            }
            // increment char count
            else if (current_char == prev_char)
            {
                count++;
            }
            // if character changes, write current char and length to output
            else
            {
                fwrite(&count, sizeof(int), 1, output_file);
                fputc(prev_char, output_file);
                prev_char = current_char;
                count = 1;
            }
        }

        fclose(input_file);
    }
    
    // write the current_char and length to output when EOF of last file is reached
    fwrite(&count, sizeof(int), 1, output_file);
    fputc(prev_char, output_file);

    fclose(output_file);
}

int main(int argc, char *argv[])
{
    // if no files specified, exit with 1
    if (argc < 2)
    {
        printf("wzip: file1 [file2 ...]\n");
        return 1;
    }

    wzip_files(argc, argv);

    return 0;
}