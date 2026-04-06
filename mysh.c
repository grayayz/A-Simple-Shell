#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>




int main(int argc, char **argv)
{ 
    //first determine if we are given a file or not 

    if(argc > 1){
        //we are given a file, run in batch mode and read each line from the file 
        int in_fd = open(argv[1], O_RDONLY); // opens the file
        

        close(in_fd);
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
                    //printf("done command");
                    //EXECUTION

                    //make sure to print the exit status and if it worked
                    
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
                        //EXECUTION

                        //make sure to print the exit status and if it worked
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
    else{/*batch mode*/
        //basically reads from an input file that's piped into this program I THINK
        //bit confused on this 
    }

    return EXIT_SUCCESS;

}
