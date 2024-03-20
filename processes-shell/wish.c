#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/stat.h>

#define ERROR_MSG "An error has occurred\n"
#define INITIAL_PATH "/bin"

#define NO_REDIRECTION 0
#define REDIRECTION 1
#define INTERACTIVE_MODE 1
#define BATCH_MODE 2
#define TRUE 1
#define FALSE 0

#define REDIRECTION_SYMBOL ">"
#define NULL_TERMINATOR '\0'
#define SPACE " "

typedef struct Command
{
    int len; // length of entire command 
    char **inputArray; // command split into array 
    int redirection; // 0 = no redirection, 1 = redirection
    char *output; // output file for batch mode 
} Command;

void raiseError() { write(STDERR_FILENO, ERROR_MSG, strlen(ERROR_MSG)); }

char *concat(char *a, char *b)
{
    // allocate memory for a, b, and null terminator
    char *res = malloc(strlen(a) + strlen(b) + 1);
    strcpy(stpcpy(res, a), b);
    return res;
}

char *trim(char *str)
{
    // delete leading whitespaces by incrementing *str
    // until a non whitespace character is reached
    while (isspace((unsigned char)*str) != 0)
    {
        str++;
    }
    // if end of string is reached, return str
    if (*str == NULL_TERMINATOR)
        return str;
    char *r;
    // delete trailing whitespaces
    // set r to the rightmost char before the null terminator
    r = str + strlen(str) - 1;
    while (r > str && isspace((unsigned char)*r))
    {
        r--;
    }
    // set null terminator to end of string
    r[1] = NULL_TERMINATOR;
    return str;
}

int processCommands(char *str, char *delim, char *tokens[])
{
    char *token;
    int i = 0;
    // split string into tokens with given delim
    while ((token = strsep(&str, delim)) != NULL)
    {
        if (strlen(token) == 0)
        {
            continue;
        }
        tokens[i++] = token;
    }
    // add null terminator to end of tokens array
    tokens[i] = NULL_TERMINATOR;
    // commands array is set, return length of array
    return i;
}

Command parseCommand(char *line)
{
    // instantiate command 
    Command command;
    command.redirection = NO_REDIRECTION;
    command.output = NULL;
    // check if there is redirection
    char *parsedLine = trim(line);
    char *found = strstr(parsedLine, REDIRECTION_SYMBOL);
    if (found != NULL)
    {
        // redirection is found, replace ">" with null terminator
        command.redirection = REDIRECTION;
        *found = NULL_TERMINATOR;
        // trim (if any) whitespaces
        char *output = trim(found + 1);
        // validate output file and set it to command.output
        if (strlen(output) > 0 && strstr(output, SPACE) == NULL)
        {
            command.output = malloc(strlen(output) + 1);
            strcpy(command.output, output);
        }
    }
    // add commands to command inputArray and set command len
    command.inputArray = malloc(100 * sizeof(*command.inputArray));
    command.len = processCommands(parsedLine, SPACE, command.inputArray);
    return command;
}

void executeCommand(char *filepath, char *inputArray[])
{
    int pid = fork();
    // error handling when forking
    if (pid == -1)
    {
        return;
    }
    // fork successful, execute command
    if (pid == 0)
    {
        execv(filepath, inputArray);
    }
    // parent process wait for child process to finish
    else
    {
        wait(NULL);
    }
}

void handleCommand(Command command, char *paths[], int *pathLength)
{
    // redirection handling
    int redirectionFile = -1;
    if (command.redirection)
    {
        // check that there are valid commands and output file for redirection
        if (command.len == 0 || command.output == NULL)
        {
            raiseError();
            return;
        }
        // close standard output and set output to write to redirectionFile
        redirectionFile = dup(STDOUT_FILENO);
        close(STDOUT_FILENO);
        open(command.output, O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU);
    }
    // built in exit command
    if (strcmp(command.inputArray[0], "exit") == 0)
    {
        // raise error if any args are passed
        if (command.len != 1)
        {
            raiseError();
            return;
        }
        exit(0);
    }
    // built in cd command
    else if (strcmp(command.inputArray[0], "cd") == 0)
    {
        // raise error if 0 or >1 args are passed or if chdir fails
        if (command.len != 2 || chdir(command.inputArray[1]) == -1)
        {
            raiseError();
        }
    }
    // built in path command
    else if (strcmp(command.inputArray[0], "path") == 0)
    {
        *pathLength = command.len - 1;
        for (int i = 1; i < command.len; i++)
        {
            int len = strlen(command.inputArray[i]);
            paths[i - 1] = malloc((len + 1) * sizeof(char));
            strcpy(paths[i - 1], command.inputArray[i]);
        }
    }
    // all other commands, ran using execv
    else
    {
        int fileAccessed = FALSE;
        for (int i = 0; i < *pathLength; i++)
        {
            // concat file path
            char *filepath = concat(concat(paths[i], "/"), command.inputArray[0]);
            // if file is executable, execute command
            if (access(filepath, X_OK) == 0)
            {
                executeCommand(filepath, command.inputArray);
                fileAccessed = TRUE;
                free(filepath);
                break;
            }
        }
        // raise error is file could not be accessed
        if (!fileAccessed)
        {
            raiseError();
        }
        // redirect output to redirectionFile
        if (redirectionFile != -1)
        {
            dup2(redirectionFile, STDOUT_FILENO);
        }
    }
}

void runShell(FILE *fp, int interactiveMode)
{
    // set path variable
    char *paths[100];
    paths[0] = INITIAL_PATH;
    int pathLength = 1;
    while (TRUE)
    {
        if (interactiveMode)
        {
            printf("wish> ");
        }
        // read line from fp
        char *line = NULL;
        size_t len = 0;
        ssize_t getLine = getline(&line, &len, fp);
        // exit shell if EOF is reached
        if (getLine == EOF)
        {
            exit(0);
        }
        // if there are parallel commands, split them up by "&"
        char **commandStrs = malloc(100 * sizeof(*commandStrs));
        int numCommands = processCommands(trim(line), "&", commandStrs);
        // create commands array
        Command commands[numCommands];
        // parse commands
        for (int i = 0; i < numCommands; i++)
        {
            commands[i] = parseCommand(commandStrs[i]);
        }
        // if single command, just run the command
        if (numCommands == 1)
            handleCommand(commands[0], paths, &pathLength);
        // parallel commands handling
        else
        {
            for (int i = 0; i < numCommands; i++)
            {
                int pid = fork();
                if (pid == -1)
                {
                    continue;
                }
                if (pid == 0)
                {
                    handleCommand(commands[i], paths, &pathLength);
                    exit(0);
                }
            }
            while (numCommands > 0)
            {
                wait(NULL);
                numCommands--;
            }
        }
        // free memory
        free(line);
        free(commandStrs);
        for (int i = 0; i < numCommands; i++)
        {
            free(commands[i].output);
            free(commands[i].inputArray);
        }
    }
}

int main(int argc, char *argv[])
{
    int interactiveMode = 0;
    FILE *fp;
    // interactive mode, set file pointer to stdin
    if (argc == INTERACTIVE_MODE)
    {
        interactiveMode = 1;
        fp = stdin;
    }
    // batch mode, set file pointer to given file
    else if (argc == BATCH_MODE)
    {
        fp = fopen(argv[1], "r");
        // check that file is opened successfully
        if (fp == NULL)
        {
            raiseError();
            exit(1);
        }
    }
    // invalid number of arguments given
    else
    {
        raiseError();
        exit(1);
    }
    runShell(fp, interactiveMode);
}