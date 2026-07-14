#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <sys/socket.h>
#include <string.h>
#include <netdb.h>
#include <pthread.h>
#include <ctype.h>

int population; //holds the number of users at any given moment
    //includes active and inactive 
int capacity;   //the amount we can hold, can be changed

//mutex
pthread_mutex_t usersMutex = PTHREAD_MUTEX_INITIALIZER;

//make a struct of *user to hold the population

typedef struct user{
    int sock_fd;
    int active; //1 for active connection, 0 for inactive
    char* name;
    char* status;
} User;

//array of error explanations -> helper for error()
static const char* errExplanations[] = {
    "Unreadable", //CODE 0
    "Name in use", //CODE 1
    "Unknown recipient", //CODE 2
    "Illegal character", //CODE 3
    "Too long" //CODE 4
};

 char nameMess[] = "1|NAM|";
 char whoMess[] = "1|WHO|";
 char setMess[] = "1|SET|";
 char msgMess[] = "1|MSG|";

struct user** userList = NULL; //holds all users active and inactive

#define backlog 10
#define BUFLEN 500

void freeUsers(struct user** userList);
void* handleClient(void* client);
void who(char* command, struct user* user, int commandLen);
void set(char* command, struct user* user, int commandLen);
void msg(char* command, struct user* user, int commandLen);
void error(int errCode, int client_fd);
void sendMessage(int fd, const char* msgType, const char* body); //helper function to send formatted messages

int main(int argc, char **argv)
{ 
    population = 0;
    //make sure we got a socket
    if(argc != 2){
        printf("Please provide the port number to listen to. \n");
        return EXIT_FAILURE;
    }

    struct sockaddr_in listener;
    //int opt = 1;
    socklen_t listenLen = sizeof(listener);
    listener.sin_family = AF_INET;
    listener.sin_addr.s_addr = INADDR_ANY;
    listener.sin_port = htons(atoi(argv[1])); //i think we can also j use SERVER_PORT

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
      if (sockfd == -1) {
            perror("socket failed");
            return EXIT_FAILURE;
        }

        if (bind(sockfd, (struct sockaddr*)&listener, listenLen) < 0){
            perror("bind failed");
            exit(EXIT_FAILURE);
        }

        listen(sockfd, backlog);
            //we are listening for connection requests
            //each new connection request --> new thread (multithreading)

    capacity = 10;
    userList = (struct user **)malloc(sizeof(struct user*) * capacity);

    //char buffer[1024];
    while (1) {
        //accept new connections to send/receive data 
        //each connection gives a new socket to communicate with 
        //listener will be running continously in background, monitoring for any new connections
            int new_fd = accept(sockfd, (struct sockaddr *)&listener, &listenLen);
            if(new_fd < 0){
                perror("accept");
                continue;
            }
            else{
                //run pthread_create for multithreading
                pthread_t thread_id;
                int* client = malloc(sizeof(int));
                *client = new_fd;

                if (pthread_create(&thread_id, NULL, handleClient, client) != 0) {
                    perror("Failed to create thread");
                    free(client);
                    close(new_fd);
                }
                pthread_detach(thread_id);
            }
    }

    close(sockfd);
    freeUsers(userList); //cleanup
    return 0;

}


void freeUsers(struct user** userList){
    //frees all users in the struct, and then the list itself 
    //we close the clients in handleClient() or if errCode == 0, in error()

    for(int i = 0; i < population; i++){
        if(userList[i]->name != NULL){free (userList[i]->name);}
        if(userList[i]->status != NULL){free (userList[i]->status);}
        if(userList[i] != NULL){free (userList[i]);}
    }

    population = 0;
    free(userList);

    return;
}

void* handleClient(void* client){
    int sockfd = *(int*)client;

    //1. name - setup
        //if unique name, with legal characters and within size req then continue
        //otherwise, return exit_failue 

        char *buf = (char*)malloc(sizeof(char) * BUFLEN);
        memset(buf, 0, BUFLEN);
        int bytes;
        int lineCap = 100;
        int lineIndex = 0;
        char* line = (char*)malloc(sizeof(char) * lineCap);
        int nameLen;
        char nameLenArr[6];
        int idx = 6;


        //gets the message
        while (((bytes = recv(sockfd, buf, BUFLEN, 0)) > 0)){

            for(int i = 0; i < bytes; i++){
                    if(lineIndex == lineCap){
                        line = realloc(line, lineCap*2);
                        lineCap = lineCap*2;
                    }
                    line[lineIndex] = buf[i];
                    lineIndex ++;
            }
            //null terminating
            if(lineIndex < lineCap){
                line[lineIndex] = '\0';
            } else{
                line = realloc(line, lineCap+1);
                line[lineIndex] = '\0';
            }
            if(lineIndex >= 6){

                if (strstr(line, nameMess) == NULL){
                    error(4, sockfd);
                    free(line);
                    free(buf);
                    close(sockfd);
                    return NULL;
                }
                else{
                    memset(nameLenArr, 0, 6);
                    int j = 0;
                    while(idx < lineIndex && isdigit(line[idx])){
                        nameLenArr[j++] = line[idx++];
                    }
                    //we want the number
                    //number can be up to two digits (1-32)
 
                    // if(isdigit(line[6]) != 0){
                    //      nameLenArr[0] = line[6]; 
                    //     }
                    // if(isdigit(line[7]) != 0){
                    //      nameLenArr[1] = line[7]; 
                    //     }
                    
                    nameLen = atoi(nameLenArr);
                }

                if((nameLen > 0) && (lineIndex >= nameLen + idx)){
                    bytes = -1;
                    break;
                }
            }
        } 

        if(lineIndex == lineCap){
                line = realloc(line, lineCap+1);
                lineCap = lineCap+1;
        }

        line[lineIndex] = '\0';

        char *userName = (char*)malloc(sizeof(char) * nameLen+1);

      //  printf("%i %s \n", nameLen, line);
        if((nameLen < 1) || (nameLen > 32)){
                    if(nameLen > 32){
                        //throw error 4 --> too long
                        error(4, sockfd);
                        close(sockfd);
                    }
                    else{
                        //throw error 0 --> fatal error
                        //just print error message and close client
                        error(0, sockfd);
                    }
                    //throw error - improperly formatted name
                    free(line);
                    free(buf);
                    return NULL;
        }


            for(int i = idx + 1; i < nameLen+ idx; i++){
                userName[nameLen - 1] = '\0';


                if((isalnum(line[i]) == 0) && (line[i] != '_') && (line[i] != '-')){
                        //throw error - improperly formatted name - error 3
                        error(3, sockfd);
                        free(userName);
                        free(buf);
                        free(line);
                        close(sockfd);
                        return NULL;
                    }
                    else{
                        userName[i-(idx + 1)] = line[i];
                    }
            }
            if(line[nameLen + idx] != '|'){

                error(0, sockfd);
                free(buf);
                free(userName);
                free(line);
                return NULL;
            }
            userName[nameLen] = '\0';





        //now check if name is already in use 

        for(int k = 0; k < population; k++){
            if((strcmp(userList[k]->name, userName) == 0) && (userList[k]->active == 1)){
                //throw error - name already in use - error 1
                    //this error should involve reopening the client and calling this function again
                free(userName);
                free(buf);
                free(line);
                error(1, sockfd);
                //handleClient(client);
                close(sockfd);
                return NULL;
            }
        }

        free (client);



        //name not in use, create new user & add to userList
        struct user *newUser = (struct user*)malloc(sizeof(struct user));
        newUser->sock_fd = sockfd;
        newUser->status = NULL;  
        newUser->name = userName;
        
        newUser->active = 1;
        population ++;
        if(population == capacity){
            capacity = capacity * 2;
            userList = realloc(userList, capacity);
        }
        userList[population-1] = newUser;
        //Welcome new user!!!
        char welcomeBody[64 + nameLen];
        snprintf(welcomeBody, sizeof(welcomeBody), "#all|%s|Welcome to the chat!|", userName);


        for(int m = 0; m < population; m++){
            if(userList[m]->active != 0){
                //sends message to #all
                sendMessage(userList[m]->sock_fd, "MSG", welcomeBody);            }
        }


    //2. continue listening to this socket until done -- parse each message and then respond to it 
        //appropriately (this could involve calling helper functions for each of the actions)
        //set_status, message, who, error (from the server, maybe don't need this)
        int len = 0;
        int messStartIndex = 0;
        char *messLen = (char*)malloc(5);
        line = realloc(line, 100);
        memset(line, 0, 100);


        while(1){
            memset(buf, 0, BUFLEN);
            memset(messLen, 0, 5);
            line = realloc(line, 100);
            memset(line, 0, 100);
            bytes = 0;
            lineCap = 100;
            lineIndex = 0;
            messStartIndex = 0;
            len = 0;
            int breakBytes = 0;

            //gets the message
            while ((bytes = recv(sockfd, buf, BUFLEN, 0)) > 0){
               // printf("%s \n", buf);
                for(int i = 0; i < bytes; i++){
                        if(lineIndex == lineCap){
                            line = realloc(line, lineCap*2);
                            lineCap = lineCap*2;
                        }
                        line[lineIndex] = buf[i];
                        lineIndex ++;
                }
                if(lineIndex >= 6){
                    if ((strstr(line, whoMess) == NULL) && (strstr(line, msgMess) == NULL) && (strstr(line, setMess) == NULL)){
                        error(0, sockfd);
                        newUser->active = 0;
                        free(buf);
                        free(messLen);
                        close(sockfd);
                        return NULL;
                    }
                else{
                    //we want the number
                    //number can be up to five digits 
                    for(int k = 0; k < 5; k++){
                        if(isdigit(line[6+k]) != 0){
                            messLen[k] = line[6+k];
                            //printf("k: %i \n", messLen[k]);
                        }
                        else{
                            messStartIndex = 7 + k;
                            k = 5;
                            len = atoi(messLen);
                        }
                    }

                }
                }
                if((len > 0) && (lineIndex >= len + 6)){
                    break;
                }
            }
            // check if client disconnected
            if(bytes == 0){
                newUser->active = 0;
                free(line);
                free(buf);
                return NULL;
                break;  // exit outer loop cleanly
            }
            if(messStartIndex + len >= lineCap){
                line = realloc(line, messStartIndex + len + 1);
            }
            line[messStartIndex + len] = '\0';
            lineIndex = messStartIndex + len;

            if(lineIndex == lineCap){
                    line = realloc(line, lineCap+1);
                    lineCap = lineCap+1;
            }

            if((len < 1) || (len > 80)){
                        if(len > 80){
                            //throw error 4 --> too long
                            error(4, sockfd);
                            free(line);
                            //NOT a fatal error
                        }
                        else{
                            //throw error 0 --> fatal error
                            error(0, sockfd);
                            newUser->active = 0;
                            free(buf);
                            free(line);
                            return NULL;
                        }
                    }
                    //now that we have the message length let's save the message
                    char* command = (char*) malloc(sizeof(char) * (len+1));
                    for(int k = messStartIndex; k < messStartIndex+len; k++){
                        //just save the message in command
                        //ensure all chars are legal
                            //if not --> error 3
                        if((line[k] >= 32) && (line[k] <= 126)){
                            command[k-messStartIndex] = line[k];
                        }
                        else if((line[k] == '\n') || (line[k] == ' ')){
                            continue;
                        }
                        else {
                           // printf("illegal char %c %s %i \n", line[k], line, k);
                            error(3, sockfd);
                            breakBytes = -1;
                            k = messStartIndex + len;
                            free(command);
                        }
                    }

                    if(breakBytes == -1){
                        continue;
                    }
                    
                    if((command[len-1] != '|') && (command[len] != '|') && (command[len-2] != '|')){
                        //incorrectly formatted --> error 0
                        fflush(stdout);
                        error(0, sockfd);
                        newUser->active = 0;
                        free(buf);
                        free(line);
                        free(command);
                        return NULL;
                    }
                    else{
                        command[len-1] = '\0';
                    }

                   // printf("command %s %s %i \n", command, line, len);

                    if(strstr(line, whoMess) != NULL){
                        who(command, newUser, len);
                        //run the who(), give command, newUser, len
                        //command freed in function
                    }
                    else if (strstr(line, setMess) != NULL){
                        set(command, newUser, len);
                        //run set, give command, newUser, len
                        //command freed when free(userList) is called
                    }
                    else if(strstr(line, msgMess) != NULL){
                        //run msg, give command, newUser, len
                        //command freed in function
                        msg(command, newUser, len);
                    }
              
        }

        free(buf);
        free(line);

    close (sockfd);
    //printf("closed port \n");
    //fflush(stdout);
    return NULL;
}

void who(char* command, struct user* client, int commandLen){
    //WHO Alice -> server responds with Alice: I was here first OR No status
    //WHO #all -> server responds with all users, one per line, name + status (if exists)

    char target[64];
    int i = 0;
    while (i < commandLen - 1 && command[i] != '|'){
        target[i] = command[i];
        i++; //incrementing the length :3
    }
    target[i] = '\0';
    char body[5000]; // to accomodate WHO #all responses
    char response[4096];
    memset(response, 0, sizeof(response));

    //WHO #all
    if (strcmp(target, "#all") == 0){
        //list all active users
        pthread_mutex_lock(&usersMutex);
        int offset = 0;
        for (int j = 0; j < population; j++){
            if (userList[j]->active == 0){
                continue;
            }
            if (userList[j]->status != NULL && strlen(userList[j]->status) > 0){
                offset += snprintf(response + offset, sizeof(response) - offset, "%s: %s\n", userList[j]->name, userList[j]->status );
            }else{
                offset += snprintf(response + offset, sizeof(response) - offset, "%s\n", userList[j]->name);
            }
        }
        pthread_mutex_unlock(&usersMutex);
        //remove trailing newline
        if (offset > 0 && response[offset - 1] == '\n'){
            response[offset-1] = '\0';
        } else {
            snprintf(body, sizeof(body), "#all|%s||", client->name);
        } snprintf(body, sizeof(body), "#all|%s|%s|", client->name, response);
     } else {
            //look up a specific user
            struct user* targetUser = NULL;
            pthread_mutex_lock(&usersMutex);
            for (int j = 0; j < population; j++){
                if (userList[j]->active && strcmp(userList[j]->name, target) == 0){
                    targetUser = userList[j];
                    break;
                }
            }
            pthread_mutex_unlock(&usersMutex);
            if (targetUser == NULL){
                error(2, client->sock_fd); //unknown recipient
                free(command);
                return;
            }
            if (targetUser->status != NULL && strlen(targetUser->status) > 0){
            snprintf(body, sizeof(body), "#all|%s|%s: %s|",client->name, targetUser->name, targetUser->status);
            } else {
                snprintf(body, sizeof(body), "#all|%s|No status|", client->name);
            }
        }
    sendMessage(client->sock_fd, "MSG", body);
    return;
}

void set(char* command, struct user* client, int commandLen){
    //command[commandLen] = '\0'
    //1. change the status in client field 
    client->status = command;

    //3. send message to #all if commandLen > 0
    if (commandLen < 0){
        free(command);
        return;
    }
    
    //send message to #all
    char *name = client->name;
    int msgLen = snprintf(NULL, 0, "#all|#all|%s is now \"%s\"|", name, command);
    char body[msgLen + 1];
    snprintf(body, msgLen + 1, "#all|#all|%s is now \"%s\"|", name, command);
    for (int i = 0; i < population; i++){
        if(userList[i]->active == 1){
            sendMessage(userList[i]->sock_fd, "MSG", body);
        }
    }
    
    
    return;
}

void msg (char* command, struct user* client, int commandLen){
    //sender is client->name
    //first field of command is the recipient
        //if #all, then send clientName|wholeCommand to everyone
            //make sure to incr. the length
        //if to certain recipient then send the following to everyone
            //<< 1|MSG|len|client|command
                //command will be: |recipient|private message to recipient|
    //commandLen includes |recipient|message|

    //3. send to #
    //COMMAND REFEENCE: "|receipient|message body|"
    //#all COMMAND: "|#all|Hello world|"
    //skip leading | to get to recipient
    int pos = 0;
    if (command[0] == '|'){
        pos = 1;
    }
    //parse recipient
    char recipient[64];
    int i = 0;
    while (pos < commandLen && command[pos] != '|'){
        recipient[i++] = command[pos++];
    }
    recipient[i] = '\0';
    pos++; //skip | after recipient
    //message body
    char* message_body = command + pos;
    //sender|recipient|message| for forwarded messages
    char body[4096];
    snprintf(body, sizeof(body), "%s|%s|%s|", client->name, recipient, message_body);

    //make the string that'll be sent to #all (both for all and PMs)
    if (strcmp(recipient, "#all") == 0){
        pthread_mutex_lock(&usersMutex);
        for (int j = 0; j < population; j++){
            if (userList[j]->active){
                sendMessage(userList[j]->sock_fd, "MSG", body);
            }
        }
        pthread_mutex_unlock(&usersMutex);
    }
    //figure out recipient
    else{
        struct user* targetRecipient = NULL;
        pthread_mutex_lock(&usersMutex);
        for (int j = 0; j < population; j++){
            if (userList[j]->active && strcmp(userList[j]->name, recipient) == 0){
                targetRecipient = userList[j];
                break;
            }
        } 
        pthread_mutex_unlock(&usersMutex);
        if (targetRecipient == NULL){
            error(2, client->sock_fd);
            free(command);
            return;
        }
        sendMessage(targetRecipient->sock_fd, "MSG", body);

    }
    free(command);
}

void error(int errCode, int clientFd){
    //if errCode == 0, FATAL, close(client_fd);
    //if errCode == 1-4, recoverable, connection stays open
    char body[64];
    snprintf(body, sizeof(body), "%d|%s|", errCode, errExplanations[errCode]);
    sendMessage(clientFd, "ERR", body);
    if (errCode == 0){
        close(clientFd);
    }
}

void sendMessage(int fd, const char* msgType, const char* body){
    //EXAMPLE MESSAGE: sendMessage(bobFd, "MSG", "#all|Bob|Welcome to the chat!|");
    int bodyLen = strlen(body);
    char full[bodyLen + 32];
    int fullLen = snprintf(full, sizeof(full), "1|%s|%d|%s", msgType, bodyLen, body);
    write(fd, full, fullLen);
}