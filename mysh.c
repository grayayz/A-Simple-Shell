#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>

//will only work correctly with gcc mysh.c -o mysh.o and then ./mysh.o !!!

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

typedef struct pipeCommand{
    int in_fd;
    int out_fd;
    char** argList;
    char* commandType;
    int commandTypeNum;
} pipeCommand;

void commandParsing (char* commandString, int inputType);
    //inputType options: 0 = batch, 1 = no file interactive

void execution (void *);
    //actual execution of command, only run after commandParsing
    //make sure to print the exit status and if it worked
    //make sure to free(command) at end;

void parsePipe(char* commandStr);


void wildcard (char* tempStr, char*** argList, int* argNum, int* argListCap);


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
                    commandParsing(line, 0);
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
            commandParsing(line, 0);
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

        while(1){
            printf("~%s $ ", workingDir);
            fflush(stdout);
            //printf("while loop \n");

        while ((bytes = read(fileno(stdin), buf, 20)) > 0){
           // printf("%s \n ", buf);

            for(int i = 0; i < bytes; i ++){
                //go through buffer and add to command

                if(buf[i] == '\n'){
                    //EXECUTION  
                    commandParsing(command, 2);                  
                    commandExecuted = 1;

                    if(strcmp(command, "exit") == 0){
                        printf("exiting mysh now \n");
                        return EXIT_SUCCESS;
                    }

                    memset(command, 0, commandArrLength);
                    command = realloc(command, 100);
                    commandArrLength = 100;
                    commandLength = 0;
                    continue;
                }

                if(buf[i] == '#'){
                    if(commandLength > 0){
                        commandParsing(command, 2);                  
                        commandExecuted = 1;

                        memset(command, 0, commandArrLength);
                        command = realloc(command, 100);
                        commandArrLength = 100;
                        commandLength = 0;
                    }
                    else{commandExecuted = 1;}
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
                    commandParsing(line, 0);
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
            commandParsing(line, 0);
        }
        free(line);
    }

    return EXIT_SUCCESS;

}

void commandParsing (char* commandString, int inputType){
    //parse the string to get input, output
    //if first arg contains / it is a path to executable file 
    //printf("commandParsing \n");

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

    while ((commandString[i] != ' ') && (i < strlen(commandString))){
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

    while (i < strlen(commandString)){
        //go through commandString and find any args, input/output files, and redirections 
            //redirection of input/output occurs after normal args 
        char* argTemp = (char*)malloc (sizeof(char) * 100);
        int tempIndex = 0;

        while ((commandString[i] != ' ') && (i < strlen(commandString)))
       {     
        if(commandString[i] == '<'){
                pipe = 3;
                //input 
                j = 0;
                i++;

                while((j < strlen(commandString)) && (j != ' ')){
                    input[j] = commandString[i+j];
                }

                i = i + j;

                continue;
            }
            else if (commandString[i] == '>'){
                pipe = 2;
                //output 
                j = 0;
                i++;

                while((j < strlen(commandString)) && (j != ' ')){
                    output[j] = commandString[i+j];
                }

                i = i + j;

                continue;
            }
            else if (commandString[i] == '|'){

                 // printf("did we get here? \n");
                pipe = 1;
                parsePipe(commandString);
                return;
                continue;
            }

            if(commandString[i] == '*'){
                //wildcard
                char* tempStr = (char*)malloc (sizeof(char) * 100);
                memcpy(tempStr, argTemp, strlen(argTemp));
                
                j = 0;
                i++;

                while((j < strlen(commandString)) && (j != ' ')){
                    tempStr[j + tempIndex] = commandString[i+j];
                }

                wildcard(tempStr, &argList, &argNum, &argListCap);

                i = i + j;

                free(tempStr);
                //reset argTemp
                memset(argTemp, 0, tempIndex);
                tempIndex = 0;
            }

            argTemp[tempIndex] = commandString[i];
            tempIndex ++;
        
            if((commandString[i+1] == ' ') || ((i+1) == strlen(commandString))){
                //this arg is done
                if (argNum == argListCap){
                    argList = realloc (argList, argListCap * 2);
                    argListCap = argListCap * 2;
                }

                argList[argNum] = argTemp;
                argNum ++;

                //printf("%s \n", argTemp);
            }

            i++;
        }
        i++;
    }

    commandPtr -> redirect = pipe;
    //printf("argList \n");

    if(type == -1){
        //not an executable 
        //check if builtin command: cd, pwd, which, exit 
        if((strcmp(commandTypeStr, "cd") == 0) || (strcmp(commandTypeStr, "pwd") == 0) || (strcmp(commandTypeStr, "which") == 0) || (strcmp(commandTypeStr, "exit") == 0)){
            type = 1;
        }
        else{type = 2;}
    }

    commandPtr->commandTypeNum = type;
    commandPtr->commandType = commandTypeStr;

    if((inputType == 2) && (pipe == 0)){
        //batchmode uses /dev/null as input stream
        memcpy(input, "/dev/null", 9);
    }

    if((inputType == 1) && (pipe == 0)){
        //child processes get stdin unless there is pipe/redirection 
        memcpy(input, "stdin", 5);
    }

    return;

}

void parsePipe(char* commandStr){

    //printf("pipe parsing \n");
    //we know we have n pipes that will connect n+1 processes
        //uses pipe() and dup2() to connect input/output
    int arrLen = 10;
    int commandNum = 0;
    struct pipeCommand** commandArr = (struct pipeCommand**)malloc(sizeof(pipeCommand*) * arrLen);
    struct pipeCommand* commandPtr = (struct pipeCommand*)malloc(sizeof(pipeCommand));
    commandArr[0] = commandPtr;
    commandNum ++;

    int i = 0;
    int j = 0;
        //while j = 0 input, while j = 1 output

    //add every word to argList until you reach the pipe
        //once you finish the first word it is the command
            //commandPtr -> command = argList[0];
            //create a new char arr for command, then memcpy to get the command
        //once argNum = 1, you are just propogating the argList 
            //which is a single char[] 
            //once you've hit the pipe and strlen(argList) > 0
                //commandPtr -> arglist = argList;
    int type = -1;
    int argCap = 100;
    char* argListPtr = (char*)malloc(sizeof(char) * argCap);   

    while (i < strlen(commandStr)){
        argCap = 100;
        int argLen = 0;
        int argNum = 0;
        char* argListPtr = (char*)malloc(sizeof(char) * argCap);   
       
        if(commandStr[i] == '/'){
            //this is a path to an executable 
            type = 0;
        }

        if(commandStr[i] == '|'){
            if(j == 0){j = 1;}
            else if (j == 1){//we are working on a new command in the commandArr
                j = 0;
                struct pipeCommand* commandPtr = (struct pipeCommand*)malloc(sizeof(pipeCommand));
                if (commandNum < arrLen){
                    commandArr = realloc(commandArr, arrLen * 2);
                    arrLen = arrLen * 2;
                }

                commandArr[commandNum] = commandPtr;
                commandNum++;
            }
            if(strlen(argListPtr) > 0){
                commandPtr->argList = &argListPtr;
            }

            free (argListPtr);
            char* argListPtr = (char*)malloc(sizeof(char) * argCap);   

        }
        if(commandStr[i] == ' '){
            if(argNum == 0){
                char* command = (char*)malloc(sizeof(char)*strlen(argListPtr));
                memcpy(command, argListPtr, strlen(argListPtr));
                commandPtr->commandType = command;
                memset(argListPtr, 0, strlen(argListPtr));
                argNum ++;
            }
            else{
                continue;
            }
        }

        if(argLen == argCap){
            argListPtr = realloc(argListPtr, argCap * 2);
            argCap = argCap * 2;
        }

        argListPtr[argLen] = commandStr[i];
        argLen ++;

        i++;
    }
    //parse the command to isolate smaller command blocks such that 
        //command arg | command arg --> each one of these is a "process"
        //can have no args or multiple args 
        //propogate a struct that sequentially stores these processes so that we can execute

    //we have propogated the struct, we need to figure out what kind of command it is now 
    if(type == -1){
        //not an executable 
        //check if builtin command: cd, pwd, which, exit 
        if((strcmp(commandPtr->commandType, "cd") == 0) || (strcmp(commandPtr->commandType, "pwd") == 0) || (strcmp(commandPtr->commandType, "which") == 0) || (strcmp(commandPtr->commandType, "exit") == 0)){
            type = 1;
        }
        else{type = 2;}
    }

    commandPtr->commandTypeNum = type;

    //after we have parsed and propogated, call the execute function
    //free each commandPtr after calling the execute function 
    i = 0;
    int nextInput = STDIN_FILENO;
    
    while (i < commandNum-1){
        int fd[2];
        pipe(fd);

        commandArr[i] -> in_fd = nextInput;
        commandArr[i] -> out_fd = fd[1];

        nextInput = fd[0];
        i++;
    }

    commandArr[commandNum-1] -> in_fd = nextInput;
    commandArr[commandNum-1] -> out_fd = STDOUT_FILENO;

    //okay now we have to call the execute function 
    i = 0;

    while (i < commandNum){
        pid_t pid = fork();

        if (pid == 0){
            //this is a child command
            execution(commandArr[i]);
        }
        else{
            //this is a parent 
            if (commandArr[i]->in_fd != STDIN_FILENO) {
                    close(commandArr[i]->in_fd);
                }
                if (commandArr[i]->out_fd != STDOUT_FILENO) {
                    close(commandArr[i]->out_fd);
                }
        }

        i++;
    }


     for (int i = 0; i < commandNum; i++) {
            wait(NULL);
    }

    //now that we've called all the commands we have to go through and free everything
    for (int i = 0; i < commandNum; i++){
        //commandArr has commandNum commandPtr's that have up to one argListPtr
        if(commandArr[i]->argList != NULL){
            free(commandArr[i]->argList);
        }
        free(commandArr[i]);
    }
    //free everything 
    free(commandArr);
    return;
}

void execution (void * ptr){
    
    
    
    return;
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