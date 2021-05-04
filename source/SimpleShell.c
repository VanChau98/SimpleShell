#include <malloc.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#define MAXLINE 80  
#define MAXWORD 20  

int fdi, fdo;
int saved_stdin, saved_stdout;

char* args[MAXLINE / 2 + 1]; 
int nArgs; 


void stringTokenizer(char* tokens[], char* source, char* delim, int *nWords)
{
    char *ptr = strtok(source, delim);
    int count = 0;
    while (ptr != NULL)
    {
        strcpy(tokens[count], ptr);
        count++;
        ptr = strtok(NULL, delim);
    }
    *nWords = count;
    return;
}

void enterCommandLine(char command[])
{
    fgets(command, MAXLINE, stdin);
    short int len = strlen(command);
    command[len - 1] = 0;
}

void allocateMemory(char* argArray[])
{
    for (int i = 0; i < MAXLINE / 2 + 1; i++)
    {
        argArray[i] = (char *)malloc(sizeof(char) * MAXWORD);
    }
}

void freeMemory(char* argArray[])
{
    for (int i = 0; i < MAXLINE / 2 + 1; i++)
    {
        if (argArray[i] != NULL)
        {
            free(argArray[i]);
            argArray[i] = NULL;
        }
    }
}

void handle_output_redirect()
{
    saved_stdout = dup(STDOUT_FILENO);
    fdo = open(args[nArgs - 1], O_CREAT | O_WRONLY | O_TRUNC);
    dup2(fdo, STDOUT_FILENO);
    free(args[nArgs - 2]);
    args[nArgs - 2] = NULL;
}

void restore_STDOUT()
{
    close(fdo);
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);
}

void handle_input_redirect()
{
    saved_stdin = dup(STDIN_FILENO);
    fdi = open(args[nArgs - 1], O_RDONLY);
    dup2(fdi, STDIN_FILENO);
    free(args[nArgs - 2]);
    args[nArgs - 2] = NULL; 
}

void restore_STDIN()
{
    close(fdi);
    dup2(saved_stdin, STDIN_FILENO);
    close(saved_stdin);
}


void execArgsPiped(char *parsed[], char *parsedpipe[])
{
    int stat;
    int pipefd[2];
    pid_t pid1, pid2;

    pid1 = fork();
    if (pid1 < 0)
    {
        printf("\nCould not fork");
        return;
    }

    if (pid1 == 0)
    {
        if (pipe(pipefd) < 0)
        {
            printf("\nPipe could not be initialized");
            return;
        }
        pid2 = fork();

        if (pid2 < 0)
        {
            printf("\nCould not fork");
            exit(0);
        }

        
        if (pid2 == 0)
        {
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            
            if (execvp(parsed[0], parsed) < 0)
            {
                printf("\nCould not execute command 1..");
                exit(0);
            }
        }
        else
        {
            wait(NULL);
            close(pipefd[1]);
            dup2(pipefd[0], STDIN_FILENO);
            
            if (execvp(parsedpipe[0], parsedpipe) < 0)
            {
                printf("\nCould not execute command 2..");
                exit(0);
            }
        } 
    }
    else
    {
        wait(NULL);
    }

}

int parsePipe(char *cmd, char* cmdpiped[])
{
    int i;
    for (i = 0; i < 2; i++)
    {
        cmdpiped[i] = strsep(&cmd, "|");
        if (cmdpiped[i] == NULL)
            break;
    }

    if (cmdpiped[1] == NULL)
        return 0; 
    else
    {
        return 1;
    }
}

void parseSpace(char *cmd, char *parsedArg[])
{
    int i = 0;

    while (cmd != NULL)
    {
        parsedArg[i] = strsep(&cmd, " ");
        if (parsedArg[i][0] != 0)
            i++;
    }
    parsedArg[i] = NULL;
}

int processString(char *cmd, char *parsed[], char *parsedpipe[])
{
    char *cmdpiped[2];
    int piped = 0;

    piped = parsePipe(cmd, cmdpiped);

    if (piped)
    {
        parseSpace(cmdpiped[0], parsed);
        parseSpace(cmdpiped[1], parsedpipe);
    }

    return piped;
}

int main(void)
{
    char command[MAXLINE]; 
    char lastCommand[MAXLINE]; 
    strcpy(lastCommand, "\0\0");
    char* parsedArgs[MAXLINE / 2 + 1];  
    char* parsedArgsPiped[MAXLINE / 2 + 1]; 
    int pipeExec = 0;   
    pid_t pid; 
    int isRedirectOutput = 0, isRedirectInput = 0;
    
    while (1)
    {
        
        do
        {
            printf("osh>");
            fflush(stdout);
            enterCommandLine(command);
        } while (command[0] == 0);

        
        if (strcmp(command, "!!") == 0)
        {
            if (lastCommand[0] == 0)
            {
                printf("Khong co lich su lenh!\n");
                continue;
            }
            else
            {
                printf("%s\n", lastCommand);
                strcpy(command, lastCommand);
            }
        }
        else
        {
            strcpy(lastCommand, command);
        }
        
        pipeExec = processString(command, parsedArgs, parsedArgsPiped);

        if (pipeExec == 0)
        {
            allocateMemory(args);
            strcpy(command, lastCommand);
            stringTokenizer(args, command, " ", &nArgs);

            free(args[nArgs]);
            args[nArgs] = NULL;

            if (strcmp(args[0], "exit") == 0)
            {
                break;
            }
            else
            {
                if (strcmp(args[0], "~") == 0)
                {
                    char* homedir = getenv("HOME");
                    printf("Home directory : %s\n", homedir);
                }
                else 
                if (strcmp(args[0], "cd") == 0)
                {
                    char dir[MAXLINE];
                    strcpy(dir, args[1]);
                    if (strcmp(dir, "~") == 0)
                    {
                        strcpy(dir, getenv("HOME"));
                    }
                    chdir(dir);
                    printf("Current directory : ");
                    getcwd(dir, sizeof(dir));
                    printf("%s\n", dir);                    
                }
                else
                {
                    int parentWait = strcmp(args[nArgs - 1], "&");

                    if (parentWait == 0)
                    {
                        free(args[nArgs - 1]);
                        args[nArgs - 1] = NULL;
                    }

                    if ((nArgs > 1) && strcmp(args[nArgs - 2], ">") == 0)
                    {
                        handle_output_redirect();
                        isRedirectOutput = 1;
                    }
                    else
                        if ((nArgs > 1) && strcmp(args[nArgs - 2], "<") == 0)
                    {
                        handle_input_redirect();
                        isRedirectInput = 1;
                    }

                    pid = fork();

                    if (pid < 0)
                    {
                        fprintf(stderr, "FORK FAILED\n");
                        return -1;
                    }
                    if (pid == 0)
                    {

                        int ret = execvp(args[0], args);

                        if (ret == -1)
                        {
                            printf("Lenh khong hop le!\n");
                        }
                        exit(ret);
                    }
                    else
                    {
                        if (parentWait)
                        {
                            while (wait(NULL) != pid)
                                ;
                            if (isRedirectOutput)
                            {
                                restore_STDOUT();
                                isRedirectOutput = 0;
                            }
                            if (isRedirectInput)
                            {
                                restore_STDIN();
                                isRedirectInput = 0;
                            }
                        }

                        freeMemory(args);
                    }
                }
            }
        }
        else
        {
            execArgsPiped(parsedArgs, parsedArgsPiped);
        }
    }
    freeMemory(args);
    return 0;
}
