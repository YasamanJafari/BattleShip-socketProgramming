#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define BUFFER_SIZE 1024
#define TIMEOUT 4
#define IP_ADDRESS "239.255.255.250"
#define INITIAL_PORT 1234
#define NAME_MAX_LEN 64

char buffer[BUFFER_SIZE] = {0}; 
int clientID, clientToServerfd, clientToClientfd, gamefd, newGamefd; 
struct sockaddr_in heartBeatAddress, dataAddress, clientAddress, gameAddress;
char *ipInfo, *IPandPort;
char name[NAME_MAX_LEN];

char* concat(const char *s1, const char *s2)
{
    char *result = malloc(strlen(s1) + strlen(s2) + 1);
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}

void intToString(int in, char *out)
{
  int len = 0, temp;
  temp = in;
  for (; temp > 0; temp /= 10, len++);
  for (int i = len - 1; in > 0; in /= 10, i--)
    out[i] = '0' + (in % 10);
}

//the port which clients listens on for other players
void createAPortForClientsGame()
{
    if((clientToClientfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    {
        write(2, "Socket creation from client to client failed.\n", 46);
        exit(1);
    }

    clientAddress.sin_family = AF_INET;   
    clientAddress.sin_addr.s_addr = inet_addr(ipInfo); 

    int randomPort;

    while(1)  
    {
        randomPort = rand() % 3000 + 4001;
        clientAddress.sin_port = htons(randomPort); 

        if(bind(clientToClientfd, (struct sockaddr *)&clientAddress, sizeof(clientAddress)) < 0)      
            continue;
        else
           break;  

    } 

    if(listen(clientToClientfd, 1) < 0)   
    {   
        write(2, "Listening on client side failed.\n", 33);  
        exit(1);   
    }  

    char finalAdr[10];
    intToString(randomPort, finalAdr);
    ipInfo = concat(ipInfo, " ");
    IPandPort = concat(ipInfo, finalAdr);
    IPandPort = concat(IPandPort, " ");
    puts(IPandPort);
}

void acceptOpponent()
{
    struct sockaddr_in addr;
    int len = sizeof(addr);
    if((newGamefd = accept(clientToClientfd, (struct sockaddr *)&addr, &len)) < 0)   
    {   
        write(2, "Accepting opponent failed.\n", 27);  
        perror("ACCEPT");   
        exit(1);   
    }   
    else
        write(2, "Now wait for the game to begin.\n", 32); 
}

void establishGameConnection() 
{
    int connectStatus;

    if((connectStatus = connect(gamefd, (struct sockaddr *)&gameAddress,  sizeof(gameAddress))) < 0) 
    {
        write(2, "Connection to opponent failed.\n", 31);
        perror("CONNECTION");
        exit(1);       
    }
    else
    {
        write(1, "Successfully connected to opponent! Now wait for the game to begin.\n", 68);
    }    
}

//after finding opponent, the second user creates a new port for connection
void createFinalGamePort(char* gameInfo) 
{
    char* temp, *gamePort, *gameIP;

    temp = strtok(gameInfo, " ");

    //because of one extra argument in specified mode
    if(strcmp(temp, "specified") == 0)
        temp = strtok(NULL, " ");

    temp = strtok(NULL, " ");
    gameIP = malloc(strlen(temp) + 1);
    strcpy(gameIP, temp);
    temp = strtok(NULL, " ");
    gamePort = malloc(strlen(temp) + 1);
    strcpy(gamePort, temp);

    if((gamefd = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    {
        write(2, "Final game socket creation failed.\n", 36);
        exit(1);
    }

    gameAddress.sin_family = AF_INET; 
    puts(gameIP);  
    gameAddress.sin_addr.s_addr = inet_addr(gameIP); 
    puts(gamePort);
    gameAddress.sin_port = htons(atoi(gamePort)); 

    establishGameConnection();    
}

void getServerPortAndIP(char *info) 
{
    char *serverPort, *serverIP;

    serverIP = strtok(info, " ");
    ipInfo = malloc(strlen(serverIP)+ 1);
    strcpy(ipInfo, serverIP);
    serverPort = strtok(NULL, " ");

    bzero((char *) &dataAddress, sizeof(dataAddress));

    dataAddress.sin_family = AF_INET;
    dataAddress.sin_addr.s_addr = inet_addr(serverIP);
    dataAddress.sin_port = htons(atoi(serverPort)); 
}

void sendDataToServer(char *name)
{
    if((clientToServerfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    {
        write(2, "Socket creation from client to server failed.\n", 24);
        exit(1);
    }

    int connectStatus;

    if((connectStatus = connect(clientToServerfd, (struct sockaddr *)&dataAddress,  sizeof(dataAddress))) < 0) 
    {
        write(2, "Connection of client and server failed.\n", 40);
        exit(1);       
    }
    else
    {
        write(1, "Sending Username...\n", 20);
        send(clientToServerfd, name, strlen(name), 0);
        write(1, "Username successfully sent. Now please wait for opponent...\n", 60);
    }
}

void checkServerAvailability() 
{
    int receivedBytes;
    int bytes = recvfrom(clientID, buffer, BUFFER_SIZE, 0, NULL, 0);
    if (bytes < 0) 
    {
        write(2, "Server is not available.\n", 25);
    }
    else
    {
        write(1, "Server is available.\n", 21);
        char *info = malloc(strlen(buffer)+ 1);
        strcpy(info, buffer);
        getServerPortAndIP(info);
        createAPortForClientsGame();
    }
}

void waitForAnswer() 
{
    bzero((char *) &buffer, sizeof(buffer));
    int bytes = recv(clientToServerfd, buffer, BUFFER_SIZE, 0);
    printf("received count: %d\n", bytes);
    //if server closed the socket, you close it too!
    if(bytes == 0)
    {
        write(1, "Now please wait for connection...\n", 35);
        acceptOpponent();
        close(clientToServerfd);
    }
    else if (bytes < 0) 
    {
        write(2, "Receiving opponent data failed.\n", 32);
    }
    else
    {
        write(1, "Your opponenet IP and Port are:\n", 32);
        write(1, buffer, sizeof(buffer));
        write(1, "\n", 1);
        char* gameInfo = malloc(strlen(buffer) + 1);
        strcpy(gameInfo, buffer);
        createFinalGamePort(gameInfo);
    }
}

void establishConnection()
{
    write(1, "Please enter your username.\n", 28);
    int bytesCount = read(0, name, NAME_MAX_LEN - 2);
    name[bytesCount - 1] = '\0';
    char* dataToBeSent  = concat(IPandPort, name);

    char answer[5];

    while(1)
    {
        write(1, "Do you want to play with a specific user?(yes/no)\n", 50);
        int bytesCount = read(0, answer, BUFFER_SIZE);
        answer[bytesCount - 1] = '\0';
        if(strcmp(answer, "yes") == 0)
        {
            write(1, "Great! Now please enter the opponent's username.\n", 50);
            int bytesCount = read(0, answer, BUFFER_SIZE);
            answer[bytesCount - 1] = ' ';
            answer[bytesCount] = '\0';
            dataToBeSent = concat(dataToBeSent, " ");
            dataToBeSent = concat(answer, dataToBeSent);
            dataToBeSent = concat("specified ", dataToBeSent);
            sendDataToServer(dataToBeSent);
            waitForAnswer();
            break;
        }   

        else if(strcmp(answer, "no") == 0)
        {
            dataToBeSent = concat("normal ", dataToBeSent);
            puts(dataToBeSent);
            sendDataToServer(dataToBeSent);
            waitForAnswer();
            break;
        }

        else
        {
            bzero((char *) &answer, sizeof(answer));
            continue;
        }
    }
}

int main(int argc, char const *argv[])
{
	struct ip_mreq mreq;
    int addrlen = sizeof(heartBeatAddress); 
    int heartBeatPort = atoi(argv[1]);
    int clientPort = atoi(argv[2]);

    if(argc != 3) 
    {
       write(2, "Count of arguments is incorrect.\n", 33);
       return 1;
    }

    if((clientID = socket(AF_INET, SOCK_DGRAM, 0)) < 0) 
    {
        write(2, "Socket creation failed.\n", 24);
    	exit(1);
    }

    u_int broadcastApproval = 1;
    if(setsockopt(clientID, SOL_SOCKET, SO_REUSEADDR, (char*) &broadcastApproval, sizeof(broadcastApproval)) < 0)
    {
       write(2, "setsockopt for client failed.\n", 30);
       return 1;
    }

    bzero((char *) &heartBeatAddress, sizeof(heartBeatAddress));

    heartBeatAddress.sin_family = AF_INET;
	heartBeatAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	heartBeatAddress.sin_port = htons(heartBeatPort);

	if(bind(clientID, (struct sockaddr*) &heartBeatAddress, sizeof(heartBeatAddress)) < 0) 
    {
        write(2, "Bind failed.\n", 13);
        return 1;
    }

    struct timeval tv;
    tv.tv_sec = TIMEOUT;
    tv.tv_usec = TIMEOUT * 1000;
    if(setsockopt(clientID, SOL_SOCKET, SO_RCVTIMEO, &tv,sizeof(tv)) < 0) 
    {
        write(2, "Setting timeout failed.\n", 24);
    }

    mreq.imr_multiaddr.s_addr = inet_addr(IP_ADDRESS);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if(setsockopt(clientID, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*) &mreq, sizeof(mreq)) < 0)
    {
        write(2, "setsockopt failed.\n", 19);
        return 1;
    }

    checkServerAvailability();
    establishConnection();

    while(1) {}

	return 0;
}