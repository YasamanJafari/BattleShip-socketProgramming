#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <errno.h> 

#define TCP_PORT "7758"
#define IP_ADDRESS "239.255.255.250"
#define TCP_IP_ADDRESS "127.0.0.1"
#define MAX_CLIENTS_COUNT 20
#define MAX_BUFFER_SIZE 1024
#define INTERVAL_SECONDS 1

int serverID, server_TCP_ID, clientSockets[MAX_CLIENTS_COUNT] = {0}, maxServerDesciptor;
char* clientRequests[MAX_CLIENTS_COUNT], *names[MAX_CLIENTS_COUNT], *requests[MAX_CLIENTS_COUNT], *status[MAX_CLIENTS_COUNT];
struct sockaddr_in serverAddress, serverTCPAddress;
const char *message = "127.0.0.1 7758 \0";
const char *tcpMessage = "Successfully connected to server.";
fd_set clientfds;

void sendHeartBeat() 
{
    if (sendto(serverID, message, strlen(message), 0, (struct sockaddr*) &serverAddress, sizeof(serverAddress)) < 0) 
    {
        write(2, "Sending heart beat failed.\n", 27);
        exit(1);
    }

    write(1, "Heartbeat.\n", 11);
    signal(SIGALRM, sendHeartBeat);
    alarm(INTERVAL_SECONDS);
}

void extractNames(char *info, int index)
{
    char *name, *temp, *requestedName;

    temp = strtok(info, " ");
    if(strcmp(temp, "normal") == 0)
    {
        //requests -> specified user to be played with
        status[index] = "normal";
        requests[index] = "";
    }
    else if(strcmp("specified", temp) == 0)
    {  
        status[index] = "specified";
        temp = strtok(NULL, " ");  
        requestedName = malloc(strlen(temp)+ 1);
        strcpy(requestedName, temp);
        requests[index] = requestedName;
    }
    temp = strtok(NULL, " ");
    temp = strtok(NULL, " ");
    temp = strtok(NULL, " ");
    name = malloc(strlen(temp)+ 1);
    strcpy(name, temp);
    names[index] = name;

    for(int i = 0; i < MAX_CLIENTS_COUNT; i++)
    {
        if(clientSockets[i] != 0)
        {
            write(1, "Client : ", 9);
            write(1, names[i], strlen(names[i]));
            write(1, " with status : ", 15);
            write(1, status[i], strlen(status[i]));
            write(1, " requested ", 11);
            write(1, requests[i], strlen(requests[i]));
            write(1, "\n", 1);
        }
    }
}

void createTCPPort() 
{
    int opt = 1;
    if((server_TCP_ID = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    {
        write(2, "TCP socket creation failed.\n", 28);
        exit(1);
    }  

    if(setsockopt(server_TCP_ID, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 )   
    {   
        write(2, "setsockopt for TCP server port failed.\n", 39);  
        exit(1);   
    } 

    serverTCPAddress.sin_family = AF_INET;   
    serverTCPAddress.sin_addr.s_addr = inet_addr(TCP_IP_ADDRESS);   
    serverTCPAddress.sin_port = htons(atoi(TCP_PORT)); 

    if(bind(server_TCP_ID, (struct sockaddr *)&serverTCPAddress, sizeof(serverTCPAddress)) < 0)   
    {   
        write(2, "Server TCP bind failed.\n", 24);  
        exit(1);   
    }   

    if(listen(server_TCP_ID, 5) < 0)   
    {   
        write(2, "Listening failed.\n", 18);  
        exit(1);   
    }   
}

void createHeartBeatPort(int heartBeatPort, int clientPort) 
{
    if((serverID = socket(AF_INET, SOCK_DGRAM, 0)) < 0) 
    {
        write(2, "Heartbeat socket creation failed.\n", 34); 
    	exit(1);
    }

    bzero((char *) &serverAddress, sizeof(serverAddress));

    serverAddress.sin_family = AF_INET;
	serverAddress.sin_addr.s_addr = inet_addr(IP_ADDRESS);
	serverAddress.sin_port = htons(heartBeatPort); 
}

void addRequestToList(char* message, int index)
{
    char *result = malloc(strlen(message) + 1); 
    strcpy(result, message);
    clientRequests[index] = result;
} 

int findNormal(int index)
{
    int someNormalUser = -1;
    for(int i = 0; i < MAX_CLIENTS_COUNT; i++)
    {
        if(i == index)
            continue;
        else
        {
            if(clientSockets[i] != 0 && strcmp(status[i], "specified") == 0 && strcmp(requests[i], names[index]) == 0 )
                return i;
            else if(clientSockets[i] != 0 && strcmp(status[i], "normal") == 0)
                someNormalUser = i;
        }
    }
    return someNormalUser;
}

int findSpecific(int index)
{
    for(int i = 0; i < MAX_CLIENTS_COUNT; i++)
    {
        if(i == index)
            continue;
        else
        {
            if((clientSockets[i] != 0 && strcmp(status[i], "normal") == 0)
                || (clientSockets[i] != 0 && strcmp(status[i], "specified") == 0 && strcmp(requests[i], names[index]) == 0 ) )
                return i;
        }
    }
    return -1;
}

void connectToOpponent(int index, int i)
{
    int len = strlen(clientRequests[i]);
    if(send(clientSockets[index], clientRequests[i], len, 0) < 0)
    {
        write(2, "Sending opponent data failed.\n", 30);
    }
    else
    {
        write(1, "Two clients ", 12);
        write(1, names[i], strlen(names[i]));
        write(1, " and ", 5);
        write(1, names[index], strlen(names[index]));
        write(1, " are connecting...\n", 19);
        close(clientSockets[i]); 
        close(clientSockets[index]);  
        clientSockets[i] = 0;
        clientSockets[index] = 0;
    }
}

void findOpponent(int index) 
{
    int userID = -1;
    if(strcmp(status[index], "normal") == 0)
    {
        if((userID = findNormal(index) ) != -1) 
            connectToOpponent(index, userID);  
    }

    else if(strcmp(status[index], "specified") == 0)
    {
        if((userID = findSpecific(index)) != -1)
            connectToOpponent(index, userID); 
    }
}  

int main(int argc, char const *argv[])
{
    int event, newSocketID, dataRead;
    int heartBeatPort = atoi(argv[1]);
    int clientPort = atoi(argv[2]);

    char buffer[MAX_BUFFER_SIZE];

    if (argc != 3) 
    {
       write(2, "Count of arguments is incorrect.\n", 33); 
       return 1;
    }

    int forkStatus = fork();

    if(forkStatus > 0)
    {
        createHeartBeatPort(heartBeatPort, clientPort);
        sendHeartBeat();
    }

    else if(forkStatus == 0)
    {
        createTCPPort();

        int tcpAddressLen = sizeof(serverTCPAddress);

        while(1) 
        {
            FD_ZERO(&clientfds);
            FD_SET(server_TCP_ID, &clientfds);   
            maxServerDesciptor = server_TCP_ID;

            for(int i = 0; i < MAX_CLIENTS_COUNT; i++)   
            {   
                if(clientSockets[i] > 0)   
                    FD_SET(clientSockets[i] , &clientfds);   
                     
                if(clientSockets[i] > maxServerDesciptor)   
                    maxServerDesciptor = clientSockets[i];   
            }  

            event = select(maxServerDesciptor + 1 , &clientfds , NULL , NULL , NULL); 

            if((event < 0) && (errno!=EINTR))   
            {   
                write(2, "Select failed.\n", 15);   
            }   

           if (FD_ISSET(server_TCP_ID, &clientfds))   
            {   
                write(2, "Trying to connect...\n", 21);   
                if((newSocketID = accept(server_TCP_ID, (struct sockaddr *)&serverTCPAddress, (socklen_t*)&tcpAddressLen)) < 0)   
                {   
                    write(2, "Accept failed.\n", 15);     
                    exit(1);   
                }   
                else
                    write(2, "Client connected to server successfully.\n", 41);  
                     
                for (int i = 0; i < MAX_CLIENTS_COUNT; i++)   
                {   
                    if(clientSockets[i] == 0)   
                    {   
                        clientSockets[i] = newSocketID;    
                        break;   
                    }    
                }   
            }

            for (int i = 0; i < MAX_CLIENTS_COUNT; i++)   
            {       
                //check incoming messages
                if(FD_ISSET(clientSockets[i] , &clientfds))   
                {    
                    //check if someone disconnected
                    if((dataRead = read(clientSockets[i] , buffer, MAX_BUFFER_SIZE)) <= 0)   
                    {    
   
                        write(1, "A client disconnected : ", 24); 
                        write(1, names[i], strlen(names[i])); 
                        write(1, "\n", 1);        
                        close(clientSockets[i]);   
                        clientSockets[i] = 0;   
                    }   
                         
                    else 
                    {   
                        buffer[dataRead] = '\0';
                        addRequestToList(buffer, i);
                        char *info;
                        info = malloc(strlen(buffer)+ 1);
                        strcpy(info, buffer);
                        extractNames(info, i);
                        findOpponent(i);
                    }   
                }   
            }          
        }
    }

    while(1) {}

	return 0;
}