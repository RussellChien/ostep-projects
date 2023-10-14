#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void wgrep_stdin(char *search_term)
{
    // getline will allocate memory for line and len
    char *line = NULL;
    size_t len = 0;

    // read from stdin and search for the given search_term using strstr
    while (getline(&line, &len, stdin) != -1)
    {
        if (strstr(line, search_term) != NULL)
        {
            printf("%s", line);
        }
    }

    free(line); // free memory allocated by getline
}

void wgrep_file(FILE *fp, char *search_term)
{
    // check that fopen was successful
    if (fp == NULL)
    {
        printf("wgrep: cannot open file\n");
        exit(1);
    }

    // getline will allocate memory for line and len
    char *line = NULL;
    size_t len = 0;

    // read and search each line from the file for given search_term using strstr
    while (getline(&line, &len, fp) != -1)
    {
        if (strstr(line, search_term) != NULL)
        {
            printf("%s", line);
        }
    }

    free(line); // free memory allocated by getline
    fclose(fp); 
}

int main(int argc, char *argv[])
{
    // if no files specified, exit with 1
    if (argc < 2)
    {
        printf("wgrep: searchterm [file ...]\n");
        return 1;
    }

    char *search_term = argv[1];

    // if no files given, read from stdin
    if (argc == 2)
    {
        wgrep_stdin(search_term);
    }
    else
    {
        // loop through each file
        for (int i = 2; i < argc; i++)
        {
            FILE *fp = fopen(argv[i], "r");

            wgrep_file(fp, search_term);
        }
    }

    return 0;
}