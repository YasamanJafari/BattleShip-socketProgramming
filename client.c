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
#include <fcntl.h> 
#include <signal.h>

#define BUFFER_SIZE 1024
#define TIMEOUT 3
#define BROADCAST_TIMEOUT 0.25
#define LOCAL_HOST "127.0.0.1"
#define IP_ADDRESS "239.255.255.250"
#define INITIAL_PORT 1234
#define NAME_MAX_LEN 64
#define MAP_ROW 10
#define MAP_COLUMN 10
#define INTERVAL_SECONDS 1
#define WIN_MESSAGE "You won! Congragulations!"
#define LOSE_MESSAGE "You lost! Better luck next time!"
#define REPEATED_MOVE "You have chosen this move before! Try another one!"
#define FULL_BLOCK "Awesome! You have chosen a block with ship!"
#define EMPTY_BLOCK "Oh no! You have chosen an empty block!"
#define MAX_CLIENTS_COUNT 20

char buffer[BUFFER_SIZE] = {0}; 
int clientID, clientToServerfd, clientToClientfd, gamefd, newGamefd, broadcastFD, broadcastForkChild; 
struct sockaddr_in heartBeatAddress, dataAddress, clientAddress, gameAddress, clientBroadcastAddress;
char *ipInfo = LOCAL_HOST, *IPandPort;
char name[NAME_MAX_LEN];
char fileName[NAME_MAX_LEN];
char map[(MAP_ROW * 2) * (MAP_COLUMN + 1) + 10];
char * dataToBeSent, *friend, *currName;
int isClientNormal;

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
  out[len + 1] = '\0';
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
    IPandPort[strlen(IPandPort)] = '\0';
}

int getRemainingShipsCount()
{
    int shipCount = 0;
    for(int i = 0; i < MAP_ROW * MAP_COLUMN * 2 - 1; i++)
        if(map[i] == '1')
            shipCount++;
    return shipCount;
}

int checkIfFull(char *place)
{
    int y, x;
    x = atoi(strtok(place, " "));
    y = atoi(strtok(NULL, " "));

    // ship exists
    if(map[20 * y + x] == '1')
    {
        map[20 * y + x] = '2';
        if(getRemainingShipsCount() == 0)
            return 3;
        else
            return 1;
    }

    //already chose
    else if(map[20 * y + x] == '2')
        return 2;
     
    //empty   
    else
    {
        map[20 * y + x] = '2';
        return 0;
    }
        
}

void playGame(int playerTurn, int playerNum)
{
    if(playerTurn == 1)
    {
        int fd = newGamefd;
        //the one who asked for connection
        if(playerNum == 2)
            fd = gamefd;
        char move[NAME_MAX_LEN];
        bzero((char *) &buffer, sizeof(buffer));
        write(1, "Please enter a location.[ 0 <= x, y <= 9 ]\n", 43);
        int bytesCount = read(0, move, BUFFER_SIZE);
        move[bytesCount - 1] = '\0';
        if(send(fd, move, strlen(move), 0) < 0)
        {
            write(1, "Sending info to other player failed.\n", 37);
            exit(1);
        }
        bzero((char *) &buffer, sizeof(buffer));
        int bytes = recv(fd, buffer, BUFFER_SIZE, 0);
        if(bytes < 0)
        {
            write(1, "Receiving info from other player failed.\n", 41);
            exit(1);
        } 
        write(1, buffer, strlen(buffer));
        write(1, "\n", 1);
        if(strcmp(buffer, WIN_MESSAGE) == 0)
            exit(1);
        else if(strcmp(buffer, FULL_BLOCK) == 0 || strcmp(buffer, REPEATED_MOVE) == 0 )
            playGame(1, playerNum);
        else if(strcmp(buffer, EMPTY_BLOCK) == 0)
            playGame(0, playerNum);
    } 

    else if(playerTurn == 0)
    {
        write(1, "It's your opponent's turn. Please wait...\n", 42);
        int fd = newGamefd;
        if(playerNum == 2)
            fd = gamefd;
        bzero((char *) &buffer, sizeof(buffer));
        int bytes = recv(fd, buffer, BUFFER_SIZE, 0);
        if(bytes < 0)
        {
            write(1, "Receiving info from other player failed.\n", 41);
            exit(1);
        } 
        write(1, "Your opponent's choice: ", 24);
        write(1, buffer, strlen(buffer));
        char* moveInfo = malloc(strlen(buffer) + 1);
        strcpy(moveInfo, buffer);
        write(1, "\n", 1); 

        int check = checkIfFull(moveInfo);

        if(check == 3)
        {
            write(1, LOSE_MESSAGE, strlen(LOSE_MESSAGE));
            write(1, "\n", 1);
            if(send(fd, WIN_MESSAGE, strlen(WIN_MESSAGE), 0) < 0)
            {
                write(2, "Sending info to other player failed.\n", 37);
                exit(1);
            } 
            exit(1);
        }

        else if(check == 2)
        {
            write(1, "Your opponent repeated a move. Wait for another move...\n", 56);
            if(send(fd, REPEATED_MOVE, strlen(REPEATED_MOVE), 0) < 0)
            {
                write(2, "Sending info to other player failed.\n", 37);
                exit(1);
            }
            playGame(0, playerNum);  
        }
        else if(check == 1)
        {
            write(1, "Your opponent chose a block with ship!\n", 39);
            if(send(fd, FULL_BLOCK, strlen(FULL_BLOCK), 0) < 0)
            {
                write(2, "Sending info to other player failed.\n", 37);
                exit(1);
            }  
            playGame(0, playerNum); 
        }
        else if(check == 0)
        {
            write(1, "Your opponent chose an empty block!\n", 36);
            if(send(fd, EMPTY_BLOCK, strlen(EMPTY_BLOCK), 0) < 0)
            {
                write(2, "Sending info to other player failed.\n", 37);
                exit(1);
            }  
            playGame(1, playerNum); 
        }        
    }
}

void readMap(char* fileName)
{
    int bytesRead, fd;

    if( (fd = open(fileName, O_RDONLY) ) < 0)
    {
        write(2, "Reading map failed.\n", 20);
        exit(1);
    }

    bytesRead = read(fd, map, MAP_ROW * 2 * MAP_COLUMN);
    write(1, "This is how your map looks like: \n\n", 35);
    write(1, map, strlen(map));
    write(1, "\n", 1);
    close(fd);
}

void acceptOpponent(int isBroadcast)
{
    struct sockaddr_in addr;
    int len = sizeof(addr);

    if((newGamefd = accept(clientToClientfd, (struct sockaddr *)&addr, &len)) < 0)   
    {   
        write(2, "Accepting opponent failed.\n", 27);  
        if(isBroadcast == 1)
            kill(broadcastForkChild, SIGTERM);  
        exit(1);   
    }   
    else
    {
        if(isBroadcast == 1)
            kill(broadcastForkChild, SIGTERM);
        write(1, "\nFound an opponent! \n", 21);
        write(1, "Game began: \n", 13);
        write(1, "It's your turn: \n", 17);
        playGame(1, 1);  
    }
}

void establishGameConnection() 
{
    int connectStatus;

    if((connectStatus = connect(gamefd, (struct sockaddr *)&gameAddress,  sizeof(gameAddress))) < 0) 
    {
        write(2, "Connection to opponent failed.\n", 31);
        exit(1);       
    }
    else
    {
        write(1, "Successfully connected to opponent! \n", 37);
        bzero((char *) &buffer, sizeof(buffer));
        int bytes = recv(clientToServerfd, buffer, BUFFER_SIZE, 0); 
        if(bytes == 0)
            close(clientToServerfd);
        playGame(0, 2); 
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
    gameAddress.sin_addr.s_addr = inet_addr(gameIP); 
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

int checkServerAvailability() 
{
    int bytes = recvfrom(clientID, buffer, BUFFER_SIZE, 0, NULL, 0);
    if (bytes < 0) 
    {
        write(2, "Server is not available.\n", 25);
        return 0;
    }
    else
    {
        write(1, "Server is available.\n", 21);
        char *info = malloc(strlen(buffer)+ 1);
        strcpy(info, buffer);
        getServerPortAndIP(info);
        createAPortForClientsGame();
        return 1;
    }
}

void waitForAnswer() 
{
    bzero((char *) &buffer, sizeof(buffer));
    int bytes = recv(clientToServerfd, buffer, BUFFER_SIZE, 0);
    //printf("received count: %d\n", bytes);
    //if server closed the socket, you close it too!
    if(bytes == 0)
    {
        write(1, "Now please wait for connection...\n", 35);
        acceptOpponent(0);
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

int findNormal(char* tempMessage)
{
    char* temp = strtok(tempMessage, " ");
    char *type = malloc(strlen(tempMessage) + 1);
    strcpy(type, temp);
    tempMessage = strtok(NULL, " ");
    char* tempName = malloc(strlen(tempMessage) + 1);
    strcpy(tempName, tempMessage); 
    //2 means immediate connection
    if(strcmp(type, "specified") == 0 && strcmp(tempName, currName) == 0)
        return 2;
    //normal
    else if(strcmp(type, "normal") == 0)
        return 1;
    //fail
    return 0;
}

int findSpecific(char* tempMessage)
{
    char* temp = strtok(tempMessage, " ");
    char *type = malloc(strlen(tempMessage) + 1);
    strcpy(type, temp);
    tempMessage = strtok(NULL, " ");
    char* tempName = malloc(strlen(tempMessage) + 1);
    strcpy(tempName, tempMessage); 
    if(strcmp(type, "specified") == 0)
        strtok(NULL, " ");
    strtok(NULL, " ");
    temp = strtok(NULL, " ");
    char* otherName = malloc(strlen(temp) + 1);
    strcpy(otherName, temp);
    //connect immediately
    if(strcmp(type, "specified") == 0 && strcmp(otherName, friend) == 0)
        return 2;
    else if(strcmp(otherName, friend) == 0 && strcmp(type, "normal") == 0)
        return 2;
    return 0;
}

void askForSpecificUserBroadcast()
{
    char answer[5];

    while(1)
    {
        write(1, "Do you want to play with a specific user?(yes/no)\n", 50);
        int bytesCount = read(0, answer, BUFFER_SIZE);
        answer[bytesCount - 1] = '\0';
        if(strcmp(answer, "yes") == 0)
        {
            isClientNormal = 0;
            write(1, "Great! Now please enter the opponent's username.\n", 50);
            int bytesCount = read(0, answer, BUFFER_SIZE);
            answer[bytesCount - 1] = '\0';
            bzero((char *) &friend, sizeof(friend));
            friend = malloc(strlen(answer) + 1);
            strcpy(friend, answer);
            answer[bytesCount - 1] = ' ';
            answer[bytesCount] = '\0';
            dataToBeSent = concat(dataToBeSent, " ");
            dataToBeSent = concat(answer, dataToBeSent);
            dataToBeSent = concat("specified ", dataToBeSent);
            break;
        }   

        else if(strcmp(answer, "no") == 0)
        {
            dataToBeSent = concat("normal ", dataToBeSent);
            isClientNormal = 1;
            break;
        }

        else
        {
            bzero((char *) &answer, sizeof(answer));
            continue;
        }
    }
}

void readDataFromInput()
{
    write(1, "Please enter your username.\n", 28);
    int bytesCount = read(0, name, BUFFER_SIZE - 2);
    bzero((char *) &currName, sizeof(currName));
    currName = malloc(strlen(name) + 1);
    name[bytesCount - 1] = '\0';
    strcpy(currName, name);
    dataToBeSent = concat(IPandPort, " ");
    dataToBeSent  = concat(dataToBeSent, name);

    write(1, "Now please enter your map's file name.\n", 39);
    bytesCount = read(0, fileName, BUFFER_SIZE);
    fileName[bytesCount - 1] = '\0';
    readMap(fileName);
}

void establishConnection()
{
    readDataFromInput();

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

void createBroadcastSocket(int clientPort)
{
    struct ip_mreq mreq;
     //for broadcast socket
    if( (broadcastFD = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        write(2, "Broadcast socket creation failed.\n", 24);
        exit(1);        
    }

    //address for client broadcast
    clientBroadcastAddress.sin_family = AF_INET;
    clientBroadcastAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    clientBroadcastAddress.sin_port = htons(clientPort);

    // printf("%d \n", clientPort);

    u_int broadcastApproval = 1;
    if(setsockopt(broadcastFD, SOL_SOCKET, SO_REUSEADDR, (char*) &broadcastApproval, sizeof(broadcastApproval)) < 0)
    {
       write(2, "setsockopt for client broadcast failed.\n", 30);
       exit(1);
    }

    if( bind(broadcastFD, (struct sockaddr*) &clientBroadcastAddress, sizeof(clientBroadcastAddress)) < 0) 
    {
        write(2, "Client broadcast bind failed.\n", 30);
        exit(1);
    }

    struct timeval tv;
    tv.tv_sec = BROADCAST_TIMEOUT;
    tv.tv_usec = BROADCAST_TIMEOUT * 1000;
    if(setsockopt(broadcastFD, SOL_SOCKET, SO_RCVTIMEO, &tv,sizeof(tv)) < 0) 
    {
        write(2, "Setting timeout for client broadcast failed.\n", 24);
    }

    mreq.imr_multiaddr.s_addr = inet_addr(IP_ADDRESS);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if(setsockopt(broadcastFD, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*) &mreq, sizeof(mreq)) < 0)
    {
        write(2, "setsockopt failed. [client broadcast]\n", 38);
        exit(1);
    }
}

void sendForBroadcast()
{
    write(1, ".", 1);
    if (sendto(broadcastFD, dataToBeSent, strlen(dataToBeSent), 0, (struct sockaddr*) &clientBroadcastAddress, sizeof(clientBroadcastAddress)) < 0) 
    {
        write(2, "Sending on client broadcast socket failed.\n", 43);
        exit(1);
    }

    signal(SIGALRM, sendForBroadcast);
    alarm(INTERVAL_SECONDS);
}

void receiveForBroadcast()
{
    write(1, "Waiting...", 11);
    bzero((char *) &buffer, sizeof(buffer));
    int bytes;

    char * tempAnswer;
    int somethingFound = 0;

    char* tempInfo;

    //wait for answer
    for(int i = 0; i < MAX_CLIENTS_COUNT; i++)
    {
        bytes = recvfrom(broadcastFD, buffer, BUFFER_SIZE, 0, NULL, 0);
        if(bytes > 0)
        {
            bzero((char *) &heartBeatAddress, sizeof(heartBeatAddress));
            tempInfo = malloc(strlen(buffer) + 1);
            strcpy(tempInfo, buffer);
            int normalSearchStatus, specificSearchStatus;
            if(isClientNormal == 1)
                normalSearchStatus = findNormal(tempInfo);
                
            else if(isClientNormal == 0)
                specificSearchStatus = findSpecific(tempInfo);   
            if( (isClientNormal == 1 && normalSearchStatus == 2 ) || (isClientNormal == 0 && specificSearchStatus == 2) )
            {
                tempAnswer = malloc(strlen(buffer) + 1);
                strcpy(tempAnswer, buffer);
                break;
            }
            else if( (isClientNormal == 1 && normalSearchStatus == 0 ) || (isClientNormal == 0 && specificSearchStatus == 0) )
                bytes = -1;
            else if( (isClientNormal == 1 && normalSearchStatus == 1) )
            {
                somethingFound = 1;
                tempAnswer = malloc(strlen(buffer) + 1);
                strcpy(tempAnswer, buffer);
            }
        }
    }
    if (bytes < 0 && somethingFound == 0) 
    {
        broadcastForkChild = fork();

        //child sends info
        if(broadcastForkChild == 0)
            sendForBroadcast();

        else
        {
            acceptOpponent(1);
            // close(broadcastFD);
        }
    }
    else
    {
        char *info = malloc(strlen(tempAnswer)+ 1);
        strcpy(info, tempAnswer);
        write(1, "Opponent found.\n", 16);
        //close the child process
        // close(forkStatus);
        createFinalGamePort(info);
    }
}

void broadcast()
{
    struct ip_mreq mreq;

    createAPortForClientsGame();
    readDataFromInput();
    askForSpecificUserBroadcast();

    write(1, "You are waiting on: ", 20);
    write(1, IPandPort, strlen(IPandPort));
    write(1, " for someone to connect to you. \n", 33);
    write(1, "Searching...\n", 13);

    receiveForBroadcast();
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


    //for heartbeat socket
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

    //addressForHeartbeat
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

    int serverIsAvailable = checkServerAvailability();

    if(serverIsAvailable == 1)
        establishConnection();
    else if(serverIsAvailable == 0)
    {
        createBroadcastSocket(clientPort);
        broadcast();
    }

    while(1) {}

	return 0;
}