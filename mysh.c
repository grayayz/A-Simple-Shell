#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>


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

void commandParsing (char* commandString, int inputType);
    //inputType options: 0 = given file, 1 = no file interactive, 2 = no file batch 

void execution (struct command* commandPtr);
    //actual execution of command, only run after commandParsing
    //make sure to print the exit status and if it worked
    //make sure to free(command) at end;

void wildcard (char* tempStr, char** argList, int* argNum, int* argListCap);


int main(int argc, char **argv)
{ 
    //first determine if we are given a file or not 
        //if we have any arguments we can run in batch mode
        //need to know which arg file is though
    char fileName[100];

    if(argc > 1){
        //we are given a file, run in batch mode and read each line from the file 
        //loop through to figure out which arg is file name 
        for(int k = 0; k < argc; k++){
            if(strcmp("./mysh",argv[k]) != 0){
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

         while ((bytes = read(fileno(stdin), buf, 20)) > 0){
           // printf("%s \n ", buf);

            for(int i = 0; i < 20; i ++){
                //go through buffer and add to line
                if(buf[i] == '\n'){
                    commandParsing(line, 0);
                    free(line);
                    lineCap = 100;
                    char* line = (char*)malloc(sizeof(char) * lineCap);
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

            for(int i = 0; i < 20; i ++){
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
                pipe = 1;
                //might have to be done in the execution method? 
                //CODE 
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

                wildcard(tempStr, argList, &argNum, &argListCap);

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

void wildcard (char* tempStr, char **argList, int* argNum, int* argListCap){
    //searches for all files in the dir that matches the wildcard format and adds to argList
    //makes sure to update argNum and argListCap as needed 

    //open the directory we're in 
    char* prefix = (char*)malloc (sizeof(char) * 100);
    char* suffix = (char*)malloc (sizeof(char) * 100);
    
    int i = 0;
    int ogArgNum = *argNum;

    while(i != '*'){
        prefix[i] = tempStr[i];
        i++;
    }

    int j = i + 1;
    i = 0;

    while (j < strlen(tempStr)){
        suffix[i] = tempStr[j];
        i++;
        j++;
    }

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
        char* name = (char*)malloc (sizeof(char) * 100);
        

        if((strstr(name, prefix) != NULL) && ((strstr(name, suffix)) != NULL)){
            if(strstr(name, suffix) - (strstr(name, prefix)) > 0){
                //add the name to arglist 
                 if (*argNum == *argListCap){
                    argList = realloc (argList, *argListCap * 2);
                    *argListCap = *argListCap * 2;
                }

                argList[*argNum] = name;
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
                    argList = realloc (argList, *argListCap * 2);
                    *argListCap = *argListCap * 2;
                }

                argList[*argNum] = tempStr;
            (*argNum)++;
    }

    return;

}