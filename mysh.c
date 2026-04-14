#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/wait.h>
#include <signal.h>
#include <limits.h>

#ifndef PATH_MAX
    #define PATH_MAX 4096
#endif

//will only work correctly with gcc mysh.c -o mysh.o and then ./mysh.o !!

typedef struct command{
    //inputName, ouputName, and commandType are all pointers to dynamically allocated char arrays

    char* inputName;
    char* outputName;
    char** argumentList;
    char* commandType;
    int redirect; //whether this command involves a pipe/redirection
        //0 for none, 1 for pipe, 2 for >, 3 for <
    int commandTypeNum;
        //0 executable, 1 built in command, 2 bare name 

} Command;

int commandParsing (char* commandString, int inputType, int isInteractive);
    //inputType options: 0 = batch, 1 = no file interactive


static void executeChild(struct command* cmd, int inputFd, int outputFd, int isInteractive);
    //run inside the child process: set up fds, execv
    //inputFd/outputFd: returns -1 -> "don't redirect this end"


static void statusOfCode(int status, int isInteractive);


int execution(struct command* commandArray, int commandCount, int isInteractive);


void wildcard (char* tempStr, char ***argList, int* argNum, int* argListCap);


int main(int argc, char **argv)
{ 
    //first determine if we are given a file or not 
        //if we have any arguments we can run in batch mode
        //need to know which arg file is though
    char fileName[100];


    if(argc > 1 /*given a file, batch mode*/){
        //we are given a file, run in batch mode and read each line from the file 
        //loop through to figure out which arg is file name 
       // printf("input given, batch mode \n");
        for(int k = 0; k < argc; k++){
            if((strcmp("./mysh.o",argv[k]) != 0) && (strcmp (argv[k], "|") != 0) && (strcmp (argv[k], "<") != 0)&& (strcmp (argv[k], ">") != 0)){
                strcpy(fileName, argv[k]);
            }
        }        
        int in_fd = open(fileName, O_RDONLY); // opens the file
        char buf[20];
        memset(buf, 0, 20);
        int bytes;
        int lineCap = 100;
        int lineIndex = 0;
        char* line = (char*)malloc(sizeof(char) * lineCap);

         while ((bytes = read(in_fd, buf, 20)) > 0){
            for(int i = 0; i < bytes; i ++){
                //go through buffer and add to line
                if(buf[i] == '\n'){
                    line[lineIndex] = '\0';
                   // printf("%s \n", line);
                    commandParsing(line, 0, 0);
                    lineCap = 100;
                    lineIndex = 0;
                    line = realloc(line, lineCap);
                }
                else{
                    if(lineIndex == lineCap){
                        line = realloc(line, lineCap*2);
                        lineCap = lineCap*2;
                    }
                    line[lineIndex] = buf[i];
                    lineIndex ++;
                }
            }
        }

        //we've finished file, check if anything left in buffer
        if(lineIndex > 0){
            line[lineIndex] = '\0';
            //printf("%s \n", line);
            commandParsing(line, 0, 0);
        }
        
        
        free(line);
        close(in_fd);

        return EXIT_SUCCESS;
    }
    else if(isatty(fileno(stdin)) == 1 /*interactive mode*/){
        //interactive mode        
        //loop through and read new commands until exit is printed 
        char* homeEnv = (char*)malloc(sizeof(char) * 200);
        homeEnv = getenv("HOME");
        
        char* workingDir = (char*)malloc(sizeof(char) * 200);
        workingDir = getcwd(workingDir, 200);

        if(strcmp(workingDir, homeEnv) == 0){
            homeEnv = "~";
        }

        if(strstr(workingDir, homeEnv) != NULL){
            char* strStart = strstr(workingDir, homeEnv);
         //   printf("string found %s \n", strStart);
            int j = 0;
            while((j < strlen(workingDir)) && (workingDir[j] == homeEnv[j])){
                j++;
            }
            char* tempStr = (char*)malloc(sizeof(char) * strlen(workingDir));

            for(int i = j; i < strlen(workingDir); i++){
                if(tempStr[i-j] == ' '){continue;}
                tempStr[i-j] = workingDir[i];
            }
           
            tempStr[strlen(workingDir) - j] = '\0';

            memcpy(workingDir, tempStr, strlen(tempStr));
            free(tempStr);
        }
        
        printf("Welcome to my shell! \n ");
       
        char buf[20];
        memset(buf, 0, 20);

        int commandLength = 0;
        int commandArrLength = 100;
        int bytes;
        char* command = (char*)malloc(sizeof(char) * 100);
        int commandExecuted = 0;
        char* pastDir = (char*)malloc(sizeof(char) * strlen(workingDir));
        memcpy(pastDir, workingDir, strlen(workingDir));

        while(1){
            printf("~%s$ ", workingDir);
            fflush(stdout);
            //printf("while loop \n");

        while ((bytes = read(fileno(stdin), buf, 20)) > 0){
           // printf("%s \n ", buf);

            for(int i = 0; i < bytes; i ++){
                //go through buffer and add to command

                if(buf[i] == '\n'){
                    //EXECUTION  
                    int result = commandParsing(command, 1, 1);
                    if((strstr(command, "cd") != NULL) && (result == 1)){
                            //printf("change directory %s \n", command);
                            //we're changing the directory 
                            if(strstr(command, "..") != NULL){
                                //want to go back a directory 
                               // printf("go back \n")
                                memset(workingDir, 0, strlen(workingDir));
                                memcpy(workingDir, pastDir, strlen(pastDir));
                            }
                            else{
                                //we have to add the new name to the end of the workingDir
                                //first let's add more space
                                int tempIndex = strlen(workingDir);
                                workingDir = realloc(workingDir, 2 * strlen(workingDir));
                                workingDir[tempIndex] = '/';
                                tempIndex ++;
                                //now lets loop through command
                                for(int l = 3; l < commandLength; l++){
                                    if(command[l] == ' '){continue;}
                                    else{
                                        workingDir[tempIndex + l - 3] = command[l];
                                        //printf("%i %c \n", l, command[l]);
                                    }
                                    if(l+1 == commandLength){
                                        tempIndex = tempIndex + l;
                                    }
                                }
                                workingDir[tempIndex] = '\0';
                            }
                        }

                    if (result == 2) {
                        printf("Exiting my shell.\n");
                        free(command);
                        return EXIT_SUCCESS;
                    }
                    commandExecuted = 1;
                    memset(command, 0, commandArrLength);
                    command = realloc(command, 100);
                    commandArrLength = 100;
                    commandLength = 0;
                    continue;
                }

                if(buf[i] == '#'){
                    if(commandLength > 0){
                        int result = commandParsing(command, 1, 1);

                        if(strstr(command, "cd") != NULL && result == 1){
                            //printf("change directory %s \n", command);
                            //we're changing the directory 
                            if(strstr(command, "..") != NULL){
                                //want to go back a directory 
                               // printf("go back \n")
                                memset(workingDir, 0, strlen(workingDir));
                                memcpy(workingDir, pastDir, strlen(pastDir));
                            }
                            else{
                                //we have to add the new name to the end of the workingDir
                                //first let's add more space
                                int tempIndex = strlen(workingDir);
                                workingDir = realloc(workingDir, 2 * strlen(workingDir));
                                workingDir[tempIndex] = '/';
                                tempIndex ++;
                                //now lets loop through command
                                for(int l = 3; l < commandLength; l++){
                                    if(command[l] == ' '){continue;}
                                    else{
                                        workingDir[tempIndex + l - 3] = command[l];
                                        //printf("%i %c \n", l, command[l]);
                                    }
                                    if(l+1 == commandLength){
                                        tempIndex = tempIndex + l;
                                    }
                                }
                                workingDir[tempIndex] = '\0';
                            }
                        }
                      
                        if (result == 2) {
                            printf("Exiting my shell.\n");
                            free(command);
                            return EXIT_SUCCESS;
                        }
                        memset(command, 0, commandArrLength);
                        command = realloc(command, 100);
                        commandArrLength = 100;
                        commandLength = 0;
                    }
                    commandExecuted = 1;
                    //just skip over the rest 
                    memset(buf, 0, 20);
                    break;
                }

                if(commandLength == commandArrLength){
                    command = realloc (command, commandArrLength * 2);
                    commandArrLength = commandArrLength * 2;
                }

                command[commandLength] = buf[i];
                commandLength ++;
            }

            if(commandExecuted == 1){
                    commandExecuted = 0;
                    commandArrLength = 100;
                    memset(command, 0, commandLength);
                    commandLength = 0;
                    command = realloc (command, commandArrLength);
                    memset(buf, 0, 20);
                    break;
                }
        memset(buf, 0, 20);

        }
    }
        if(homeEnv != NULL){free(homeEnv);}
        if(command != NULL){free(command);}
        if(pastDir != NULL){free(pastDir);}

    }
    else if (isatty(fileno(stdin)) != 1 /*no file, batch mode*/){
        char buf[20];
        memset(buf, 0, 20);
        int bytes;
        int lineCap = 100;
        int lineIndex = 0;
        char* line = (char*)malloc(sizeof(char) * lineCap);

         while ((bytes = read(fileno(stdin), buf, 20)) > 0){
            for(int i = 0; i < bytes; i ++){
                //go through buffer and add to line
                if(buf[i] == '\n'){
                    line[lineIndex] = '\0';
                    //printf("%s \n", line);
                    commandParsing(line, 0, 0);
                    lineCap = 100;
                    lineIndex = 0;
                    line = realloc(line, lineCap);
                }
                else{
                    if(lineIndex == lineCap){
                        line = realloc(line, lineCap*2);
                        lineCap = lineCap*2;
                    }
                    line[lineIndex] = buf[i];
                    lineIndex ++;
                }
            }
        }

        //we've finished file, check if anything left in buffer
        if(lineIndex > 0){
            line[lineIndex] = '\0';
           // printf("%s \n", line);
            commandParsing(line, 0,0);
        }
        free(line);
    }

    return EXIT_SUCCESS;

}

int commandParsing(char* commandString, int inputType, int isInteractive){
    //parse the string to get input, output
    //if first arg contains / it is a path to executable file 
    //printf("commandParsing \n");

    // ignore empty commands
    if (commandString == NULL || strlen(commandString) == 0) return 1;
    // skip whitespace-only lines
    int allWhitespace = 1;
    for (int k = 0; k < strlen(commandString); k++){
        if (commandString[k] != ' ' && commandString[k] != '\t'){
            allWhitespace = 0;
            break;
        }
    }
    if (allWhitespace) return 1;

    //array to hold commands (1 normally, 2+ for pipelines)
    int commandArrayCap = 4;
    int commandCount = 0;
    struct command** commandArray = malloc(sizeof(struct command*) * commandArrayCap);
    
    int commandTypeStrLen = 100;
    struct command * commandPtr = (struct command *) malloc (sizeof(struct command));
    char* commandTypeStr = (char*)malloc(sizeof(char) * commandTypeStrLen);

    char* input = (char*)malloc (sizeof(char) * 100);
    commandPtr->inputName = input;
    input[0] = '\0';

    char* output = (char*)malloc (sizeof(char) * 100);
    commandPtr->outputName = output;
    output[0] = '\0';

    int argListCap = 10;
    int argNum = 0;
    char** argList = (char**)malloc(sizeof(char*) * argListCap);
    argList[0] = NULL;

    int pipe = 0;
    int type = -1;
    int i = 0;
    int j = 0;

    while ((commandString[i] != ' ') && (i < (int)strlen(commandString))){
        if(commandString[i] == '/'){
            //this is a path to an executable 
            type = 0;
        }
       
        if(i == commandTypeStrLen){
            commandTypeStr = realloc (commandTypeStr, commandTypeStrLen * 2);
            commandTypeStrLen = commandTypeStrLen * 2;
        }
        commandTypeStr[i] = commandString[i];
        i++;
    }
    commandTypeStr[i] = '\0';

    while (i < (int)strlen(commandString)){
        //go through commandString and find any args, input/output files, and redirections 
        //redirection of input/output occurs after normal args 
        char* argTemp = (char*)malloc (sizeof(char) * 100);
        int tempIndex = 0;

        while ((commandString[i] != ' ') && (i < (int)strlen(commandString))){     
            if(commandString[i] == '<'){
                pipe = 3;
                //input 
                j = 0;
                i++;

                while((i+j < strlen(commandString)) && (commandString[i+j] != ' ')){
                    input[j] = commandString[i+j];
                    j++;
                }
                input[j] = '\0';
                i = i + j;
                continue;
            }
            else if (commandString[i] == '>'){
                pipe = 2;
                //output 
                j = 0;
                i++;

                while((i+j < (int)strlen(commandString)) && (commandString[i+j] != ' ')){
                    output[j] = commandString[i+j];
                    j++;
                }
                output[j] = '\0';
                i = i + j;
                continue;
            }
            else if (commandString[i] == '|'){
                pipe = 1;
                argList[argNum] = NULL; 
                if (type == -1){
                    if((strcmp(commandTypeStr, "cd") == 0) || (strcmp(commandTypeStr, "pwd") == 0) || 
                    (strcmp(commandTypeStr, "which") == 0) || (strcmp(commandTypeStr, "exit") == 0)){
                        type = 1;
                    } else{
                        type = 2;
                    }
                }
                commandPtr->commandTypeNum = type;
                commandPtr->commandType = commandTypeStr;
                commandPtr->argumentList = argList;
                commandPtr->redirect = 1;
                //add to array
                if (commandCount == commandArrayCap){
                    commandArrayCap*=2;
                    commandArray = realloc(commandArray, sizeof(struct command*)*commandArrayCap);
                } 
                commandArray[commandCount++] = commandPtr;

                //reset for the next command
                commandPtr = (struct command*)malloc(sizeof(struct command));
                commandTypeStr = (char*)malloc(sizeof(char)*100);
                commandTypeStrLen = 100;
                input = (char*)malloc(sizeof(char) * 100);
                output = (char*)malloc(sizeof(char) * 100);
                input[0] = '\0';
                output[0] = '\0';
                commandPtr->inputName = input;
                commandPtr->outputName = output;
                argList = (char**)malloc(sizeof(char*) * 10);
                argListCap = 10;
                argNum = 0;
                argList[0] = NULL;
                type = -1;
                pipe = 0;
                i++;

                // skip spaces after |
                while(commandString[i] == ' '){
                    i++;
                }

                // read next command name
                int ti = 0;
                while((commandString[i] != ' ') && (i < (int)strlen(commandString))){
                    if(commandString[i] == '/') type = 0;
                    commandTypeStr[ti++] = commandString[i++];
                }
                commandTypeStr[ti] = '\0';
                free(argTemp);
                argTemp = NULL;
                break; // break inner while, outer while continues
            }

            if(commandString[i] == '*'){
                //wildcard
                char* tempStr = (char*)malloc (sizeof(char) * 100);
                memcpy(tempStr, argTemp, strlen(argTemp));
                
                j = 0;
                i++;

                while((i+j < (int)strlen(commandString)) && (commandString[i+j] != ' ')){
                    tempStr[j + tempIndex] = commandString[i+j];
                    j++;
                }
                tempStr[tempIndex + j] = '\0';
                wildcard(tempStr, &argList, &argNum, &argListCap);
                i = i + j;
                free(tempStr);
                //reset argTemp
                memset(argTemp, 0, tempIndex);
                tempIndex = 0;
            }

            if(argTemp != NULL){
                argTemp[tempIndex] = commandString[i];
                tempIndex ++;
            }
        
            if((commandString[i+1] == ' ') || ((i+1) == strlen(commandString))){
                argTemp[tempIndex] = '\0';
                //this arg is done
                if (argNum == argListCap){
                    argList = realloc (argList, argListCap * 2);
                    argListCap = argListCap * 2;
                }

                argList[argNum] = argTemp;
                argNum ++;
                argTemp = NULL;
            }
            i++;
        
        }
        if(argTemp != NULL){
            free(argTemp);
        }
        i++;
    }

    argList[argNum] = NULL;

    if(type == -1){
        //not an executable 
        //check if builtin command: cd, pwd, which, exit 
        if((strcmp(commandTypeStr, "cd") == 0) || (strcmp(commandTypeStr, "pwd") == 0) || (strcmp(commandTypeStr, "which") == 0) || (strcmp(commandTypeStr, "exit") == 0)){
            type = 1;
        }
        else{type = 2;}
    }

    commandPtr -> redirect = pipe;
    //printf("argList \n");
    commandPtr->commandTypeNum = type;
    commandPtr->argumentList = argList;
    commandPtr->commandType = commandTypeStr;

    if((inputType == 0) && (pipe == 0)){
        //batchmode uses /dev/null as input stream
        memcpy(input, "/dev/null", 9);
    }

    if((inputType == 1) && (pipe == 0)){
        //child processes get stdin unless there is pipe/redirection 
        memcpy(input, "stdin", 5);
    }
    if(commandCount == commandArrayCap){
        commandArrayCap *= 2;
        commandArray = realloc(commandArray, sizeof(struct command*) * commandArrayCap);
    }
    commandArray[commandCount++] = commandPtr;

    // flatten into array for execution
    struct command* flatArray = malloc(sizeof(struct command) * commandCount);
    for(int k = 0; k < commandCount; k++){
        flatArray[k] = *commandArray[k];
        free(commandArray[k]);
    }
    free(commandArray);

    int result = execution(flatArray, commandCount, isInteractive);
    free(flatArray);
    return result;
}


static void executeChild(struct command* cmd, int inputFd, int outputFd, int isInteractive){
    //run inside the child process: set up fds, execv
    //inputFd/outputFd: returns -1 -> "don't redirect this end"
    if (inputFd != -1){ //redirect stdin
        if (dup2(inputFd, STDIN_FILENO) == -1){
            perror("dup2 stdin");
            exit(EXIT_FAILURE);
        } close (inputFd);
    } else if (cmd->inputName != NULL && cmd->inputName[0] != '\0'
               && strcmp(cmd->inputName, "stdin") != 0){//batch mode: /dev/null as the default input stream for child processes
        int fd = open(cmd->inputName, O_RDONLY);
        if (fd == -1) {
            perror(cmd->inputName);
            exit(EXIT_FAILURE);
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
    } else if (!isInteractive) {
        // batch mode with no inputName set — use /dev/null defensively
        int devnull = open("/dev/null", O_RDONLY);
        if (devnull == -1) {  
            perror("/dev/null");
            exit(EXIT_FAILURE);
        }
        dup2(devnull, STDIN_FILENO);
        close(devnull);
    }

    if (outputFd != -1){
        if (dup2(outputFd, STDOUT_FILENO) == -1){
            perror("dup2 stdout");
            exit(EXIT_FAILURE);
        } close (outputFd);
    } else if (cmd->outputName != NULL && cmd->outputName[0]!='\0'){
        //create file if it doesnt exist -> truncate if it does -> use mode 0640
        int fd = open(cmd->outputName, O_WRONLY | O_CREAT | O_TRUNC, 0640); 
        if (fd == -1){
            perror(cmd ->outputName);
            exit(EXIT_FAILURE);
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }

    //Building the argv array for int execv(const char *path, char* argv[])
    //[commandType..., argumentList, NULL]
    //count args

    int argc = 0;
    if (cmd->argumentList != NULL){
        while(cmd->argumentList[argc] != NULL) argc++;
    }
    char** argv = malloc(sizeof(char*) * (argc + 2));
    if (argv == NULL){
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    argv[0]  = cmd->commandType; //the program name
    for (int i = 0; i < argc; i++){
        argv[i+1] = cmd->argumentList[i]; //the arguments
    }
    argv[argc + 1] = NULL; //array must end with NULL

        // resolve path
    char resolvedPath[PATH_MAX];
    char* execPath = NULL;

    if (cmd->commandTypeNum == 0) {
        // already a path (contains /)
        execPath = cmd->commandType;
    } else {
        //BARE NAME: search the 3 dirs
        const char* dirs[] = {"/usr/local/bin", "/usr/bin", "/bin", NULL};
        for (int i = 0; dirs[i] != NULL; i++) {
            snprintf(resolvedPath, PATH_MAX, "%s/%s", dirs[i], cmd->commandType);
            if (access(resolvedPath, X_OK) == 0) {
                execPath = resolvedPath;
                break;
            }
        }
        if (execPath == NULL) {
            write(STDERR_FILENO, cmd->commandType, strlen(cmd->commandType));
            write(STDERR_FILENO, ": command not found\n", 20);
            free(argv);
            exit(EXIT_FAILURE);
        }
    }

    execv(execPath, argv); //if we get here, execv failed
    perror(execPath);
    free(argv);
    exit(EXIT_FAILURE);

}

//parent calls waitpid(), statusOfCode should return exit code
static void statusOfCode(int status, int isInteractive){
    //on success it should print nothing, otherwise indicate if it exited with code != 0 or
    //terminated by a signal
    if (!isInteractive) return;
    if (WIFEXITED(status)){
        int code = WEXITSTATUS(status); //returns exit code
        if (code != 0){
            fprintf(stderr, "Exited with status %d\n", code);
        }
    } else if (WIFSIGNALED(status)){
        int signum = WTERMSIG(status); //returns signal number
        psignal(signum, "Terminated by signal");
    }
}


int execution(struct command* commandArray, int commandCount, int isInteractive){
    //isInteractive returns 1 if in interactive mode, 0 if batch
    //execution returns 1 on success, 0 on failure
    if (commandCount == 0){
        return 1;
    }
    //built-in commands
    //cd, pwd, which, exit
    if (commandCount == 1 && commandArray[0].commandTypeNum == 1){
        struct command* cmd = &commandArray[0];
        char* name = cmd->commandType;
        //> redirection applies
        //save stdout, redirect if needed, restore after
        int savedStdout = -1;
        if (cmd->outputName != NULL && cmd->outputName[0] !='\0'){
            int fd = open(cmd->outputName, O_WRONLY | O_CREAT| O_TRUNC, 0640);
            if (fd == -1){
                perror(cmd->outputName);
                return 0;
            }
            savedStdout = dup(STDOUT_FILENO);
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
        int result = 1;
        if (strcmp(name, "exit") == 0){
            //indicates that mysh should stop reading commands and terminate successfully
            if (savedStdout != -1){
                dup2(savedStdout, STDOUT_FILENO);
                close(savedStdout);
            } return 2;
        } else if (strcmp(name, "cd") == 0){
            int argCount = 0;
            if (cmd->argumentList != NULL){
                while(cmd->argumentList[argCount] != NULL){
                    argCount++;
                }
            }
            //use chdir()
            if (argCount > 1){
                write(STDERR_FILENO, "cd: too many arguments\n", 23);
                result = 0;
            } else {
                char* target;
                if (argCount == 0){
                    target = getenv("HOME");
                } else {
                    target = cmd->argumentList[0];
                }
                if (chdir(target) != 0){
                    perror("cd"); //chdir failed
                    result = 0;
                }
            }
        } else if (strcmp(name, "pwd") == 0){
            //prints the current working directory to stdout using getcwd()
            char cwd[PATH_MAX];
            if (getcwd(cwd, sizeof(cwd)) == NULL){
                perror("pwd");
                result = 0;
            } else {
                write(STDOUT_FILENO, cwd, strlen(cwd));
                write(STDOUT_FILENO, "\n", 1);
            }
        } else if (strcmp(name, "which") == 0){
            //takes a single arg (name of program aka cmd->commandType)
            //prints the result of the search used for bare names
            int argCount = 0;
            if (cmd->argumentList != NULL){
                while(cmd->argumentList[argCount] != NULL){
                    argCount++;
                }
            }
            if (argCount != 1){
                result = 0;
            } else {
                char* target = cmd->argumentList[0];
                //fail if its the name of a built-in
                if ((strcmp(target, "cd") == 0) || (strcmp(target, "pwd") == 0) ||
                    (strcmp(target, "which") == 0) || (strcmp(target, "exit") == 0)){
                    result = 0;
                } else {
                    int found = 0;
                    char resolvedPath[PATH_MAX];
                    const char* dirs[] = {"/usr/local/bin", "/usr/bin", "/bin", NULL};
                    for (int i = 0; dirs[i] != NULL; i++){
                        snprintf(resolvedPath, PATH_MAX, "%s/%s", dirs[i], target);
                        if (access(resolvedPath, X_OK) == 0){
                            write(STDOUT_FILENO, resolvedPath, strlen(resolvedPath));
                            write(STDOUT_FILENO, "\n", 1);
                            found = 1;
                            break;
                        }
                    }
                    if (!found){ //program not found -> fail
                        result = 0;
                    }
                }
            }
        }
        if (savedStdout != -1){
            dup2(savedStdout, STDOUT_FILENO);
            close(savedStdout);
        }
        return result;
    }
    //PIPELINE
    int (*pipes)[2] = malloc(sizeof(int[2]) * (commandCount-1));
    if (pipes == NULL && commandCount > 1){
        perror("malloc");
        return 0;
    }
    for (int i = 0; i < commandCount - 1; i++){
    if (pipe(pipes[i]) == -1){
        perror("pipe");
        for (int j = 0; j < i; j++){
            close(pipes[j][0]);
            close(pipes[j][1]);
        }
        free(pipes);
        return 0;
    }
}

    pid_t* pids = malloc(sizeof(pid_t) * commandCount);
    if (pids == NULL){
        perror("malloc");
        free(pipes);
        return 0;
    }
    for (int i = 0; i < commandCount; i++){
        struct command* cmd = &commandArray[i];
        int inputFd = -1;
        if (i==0){
            //checking for < redirection again
            if (cmd->inputName != NULL && cmd->inputName[0] != '\0'
                && strcmp(cmd->inputName, "stdin") != 0
                && strcmp(cmd->inputName, "/dev/null") != 0) {
                inputFd = open(cmd->inputName, O_RDONLY);
                if (inputFd == -1) {
                    perror(cmd->inputName);
                    for (int j = 0; j < commandCount - 1; j++) {
                        close(pipes[j][0]); close(pipes[j][1]);
                    }
                    free(pipes); free(pids);
                    return 0;
                }
            }
        } else{
            inputFd = pipes[i-1][0];
        }
        //stdout, checking for > redirection
        int outputFd = -1;
        if (i == commandCount - 1) {
            // last process: check for > redirection
            if (cmd->outputName != NULL && cmd->outputName[0] != '\0') {
                outputFd = open(cmd->outputName, O_WRONLY | O_CREAT | O_TRUNC, 0640);
                if (outputFd == -1) {
                    perror(cmd->outputName);
                    for (int  j= 0; j < commandCount - 1; j++) {
                        close(pipes[j][0]); close(pipes[j][1]);
                    }
                    free(pipes); free(pids);
                    return 0;
                }
            }
        } else {
            outputFd = pipes[i][1];
        }
        pids[i] = fork();
        if (pids[i] == -1){
            perror("fork");
            free(pipes);
            free(pids);
            return 0;
        }
        if (pids[i] == 0){ //we are in the CHILD process
            for (int j = 0; j < commandCount - 1; j++){
                if (pipes[j][0] != inputFd){
                    close(pipes[j][0]);
                }
                if (pipes[j][1] != outputFd){
                    close(pipes[j][1]);
                }
            }
            executeChild(cmd, inputFd, outputFd, isInteractive);//never returns
        }
        //PARENT PROCESS: close the fds we handed to the child
        if (i > 0 && inputFd != -1){
            close(inputFd);
        }
        if (i < commandCount - 1 && outputFd != -1){
            close(outputFd);
        }
    }
        //the part where we call waitpid
        int finalStatusOfCode = 0;
        for (int i = 0; i < commandCount; i++){
            int status;
            waitpid(pids[i], &status, 0);
            if (i == commandCount - 1){
                finalStatusOfCode = status;
            }
        }
        statusOfCode(finalStatusOfCode, isInteractive);
        free(pipes);
        free(pids);
        if (WIFEXITED(finalStatusOfCode) && WEXITSTATUS(finalStatusOfCode) == 0){
            //success
            return 1;
        } else {
            //process exited with non-0 code or was terminated by a signal (fail)
            return 0;
        }

}


void wildcard (char* tempStr, char ***argList, int* argNum, int* argListCap){
    //searches for all files in the dir that matches the wildcard format and adds to argList
    //makes sure to update argNum and argListCap as needed 

    //open the directory we're in 
    char* prefix = (char*)malloc (sizeof(char) * (strlen(tempStr) + 1));
    char* suffix = (char*)malloc (sizeof(char) * (strlen(tempStr) + 1));
    
    int i = 0;
    int ogArgNum = *argNum;

    while(tempStr[i] != '*'){
        prefix[i] = tempStr[i];
        i++;
    }

    prefix[i] = '\0';

    int j = i + 1;
    i = 0;

    while (j < strlen(tempStr)){
        suffix[i] = tempStr[j];
        i++;
        j++;
    }

    suffix[i] = '\0';

    //now open directory and search through all files to see if they contain the prefix and suffix
        //ensure that int prefixStart < int suffixStart

    int prefixStart = 0;
    int suffixStart = 0;

    DIR *dir = opendir (".");
    struct dirent *de;

    while ((de = readdir (dir))){
          if(strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0){
                continue;
            }
        char* name = (char*)malloc (sizeof(char) * strlen(de->d_name) + 1);
        strcpy(name, de->d_name);
        

        if((strstr(name, prefix) != NULL) && ((strstr(name, suffix)) != NULL)){
            if(strstr(name, suffix) - (strstr(name, prefix)) > 0){
                //add the name to arglist 
                 if (*argNum == *argListCap){
                    *argList = realloc (*argList, sizeof(char*) * *argListCap * 2);
                    *argListCap = *argListCap * 2;
                }

                (*argList)[*argNum] = name;
                (*argNum)++;
            }
            else{free(name);}
        }
        else{free(name);}
    }

    free(prefix);
    free(suffix);

    if(ogArgNum == *argNum){
        //didn't find anything that fit pattern
        if (*argNum == *argListCap){
                    *argList = realloc (*argList, sizeof(char*) * *argListCap * 2);
                    *argListCap = *argListCap * 2;
                }

                (*argList)[*argNum] = tempStr;
            (*argNum)++;
    }

    closedir(dir);

    return;

}
