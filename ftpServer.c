#include <arpa/inet.h>
#include <dirent.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#define PORT 9007
#define USERMAX 1024
#define SIZE 1024

// structure for the connected client data
struct ClientStruct {
  int userIndex;
  int userCurDataPort;
  bool userName;
  bool userPass;
  char currDir[256];
};

// structure to store username and password
struct acc {
  char user[256];
  char pw[256];
};

// variable to store list of connected clients(FD_SETSIZE = max number of client connections allowed at one time)
struct ClientStruct listOfConnectedClients[FD_SETSIZE];
// variable to store accepted user accounts
static struct acc accFile[USERMAX];
// variable to hold the number of users read from file
int userCount = 0;

// function to read users from file
void loadUserFile() {
  FILE *userFile = fopen("user.txt", "r");
  int strCount = 0;
  char str;
  while (!feof(userFile)) {
    str = fgetc(userFile);
    if (str == '\n') {
      userCount += 1;
    }
  }
  rewind(userFile);
  while (strCount < userCount + 1) {
    fscanf(userFile, "%s %s", accFile[strCount].user, accFile[strCount].pw);
    strCount += 1;
  }
}

// function to intiate TCP listen
int initiateTcp() {
  int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
  if (serverSocket < 0) {  // check for fail error
    perror("socket:");
    exit(EXIT_FAILURE);
  }
  int value = 1;
  setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));  //&(int){1},sizeof(int)

  struct sockaddr_in servAddr;  // define server address structure
  bzero(&servAddr, sizeof(servAddr));
  servAddr.sin_family = AF_INET;
  servAddr.sin_port = htons(PORT);
  servAddr.sin_addr.s_addr = INADDR_ANY;
  if (bind(serverSocket, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0) {  // bind
    perror("bind failed");
    exit(EXIT_FAILURE);
  }

  if (listen(serverSocket, 5) < 0) {  // listen
    perror("listen failed");
    close(serverSocket);
    exit(EXIT_FAILURE);
  } else {
    printf("Server is listening at port: %d, ip:(%s)\n", ntohs(servAddr.sin_port), inet_ntoa(servAddr.sin_addr));
  }
  return serverSocket;
}

// function to break a string to two strings
void sepCmdDat(char *buff, char *cmdstr, char *datstr) {
  int responseSize = strlen(buff), strindx = 0;
  bool secondStr = false;
  // break into command and data

  for (int j = 0; j <= responseSize; j++) {
    if (buff[j] == ' ') {
      // command string e.g. "USER", "PASS", "STOR", etc.
      secondStr = true;
      cmdstr[strindx] = '\0';
      strindx = 0;
    } else {
      // data string (if any)
      if (!secondStr) {
        cmdstr[strindx] = buff[j];
      } else {
        datstr[strindx] = buff[j];
      }
      strindx++;
    }
  }
}

// USER command, i = socket
void ftpUserCmd(int i, char *resDat) {
  bool foundDat = false;
  for (int n = 0; n < userCount; n++) {
    if (strcmp(resDat, accFile[n].user) == 0) {
      foundDat = true;
      listOfConnectedClients[i].userIndex = n;  // found at nth pos in array
      listOfConnectedClients[i].userName = true;
      char corResponse[] = "331 Username OK, need password.";
      send(i, corResponse, sizeof(corResponse), 0);
      break;
    }
  }
  if (!foundDat) {
    char corResponse[] = "530 Not logged in.";
    send(i, corResponse, sizeof(corResponse), 0);
  }
}

// PASS command, i = socket
void ftpPassCmd(int i, char *resDat) {
  if (!listOfConnectedClients[i].userName) {
    char corResponse[] = "530 Not logged in.";
    send(i, corResponse, sizeof(corResponse), 0);
  } else {
    if (strcmp(resDat, accFile[listOfConnectedClients[i].userIndex].pw) == 0) {
      char corResponse[] = "230 User logged in, proceed.";
      listOfConnectedClients[i].userPass = true;
      send(i, corResponse, sizeof(corResponse), 0);
    } else {
      char corResponse[] = "530 Not logged in.";
      send(i, corResponse, sizeof(corResponse), 0);
    }
  }
}

// LIST command
void ftpListCmd(int i) {
  struct dirent *drty;
  DIR *direct;
  direct = opendir(".");
  if (direct) {
    char buffer2[256];
    while ((drty = readdir(direct)) != NULL) {
      // dummy read

      bzero(buffer2, sizeof(buffer2));
      recv(i, &buffer2, sizeof(buffer2), 0);  // receive dummy

      // send
      send(i, drty->d_name, sizeof(drty->d_name), 0);
    }
    closedir(direct);
  }
}

// function to initiate second TCP channel, i = current socket
int initiateDataChannel() {
  int newDataSock = socket(AF_INET, SOCK_STREAM, 0);
  if (newDataSock == -1) {
    printf("socket creation failed..\n");
    exit(EXIT_FAILURE);
  }
  int value = 1;
  setsockopt(newDataSock, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));

  struct sockaddr_in thisMachineAddr;

  // bind it to port 20
  bzero(&thisMachineAddr, sizeof(thisMachineAddr));
  thisMachineAddr.sin_family = AF_INET;
  thisMachineAddr.sin_port = htons(20);
  thisMachineAddr.sin_addr.s_addr = INADDR_ANY;  // TODO:
  if (bind(newDataSock, (struct sockaddr *)&thisMachineAddr, sizeof(struct sockaddr_in)) == 0) {
    printf("binded \n");
  }
  return newDataSock;
}

// function to send file over socket i
void sFile(FILE *fp, int i) {
  int n;
  char data[SIZE] = {0};

  while (fgets(data, SIZE, fp) != NULL) {
    printf("%s\n", data);
    if (send(i, data, sizeof(data), 0) == -1) {
      perror("[-]Error in sending file.");
      exit(1);
    }
    bzero(data, SIZE);
  }

  fclose(fp);
  char complete[] = "226 Transfer completed.";
  send(i, complete, sizeof(complete), 0);
}

// main function
int main() {
  // reading from the file
  loadUserFile();
  int serverSocket = initiateTcp();

  fd_set masterSet;
  FD_ZERO(&masterSet);               // zero out/iniitalize our set of all sockets
  FD_SET(serverSocket, &masterSet);  // adds  current socket to the fd set

  while (true) {
    fd_set copySet = masterSet;  // making a temporary copy
    // The max no. of sockets supported by select() = FD_SETSIZE (typically 1024).
    if (select(FD_SETSIZE, &copySet, NULL, NULL, NULL) < 0) {
      perror("select error");
      exit(EXIT_FAILURE);
    }

    // looping through fdset to check which ones are ready for reading
    for (int i = 0; i < FD_SETSIZE; i++) {
      // check to see if that fd is SET -> if set, there is data to read
      if (FD_ISSET(i, &copySet)) {
        // 1st case: NEW CONNECTION
        if (i == serverSocket) {
          struct sockaddr_in clientAddrs;
          socklen_t addr_size;
          // accept new connection
          int newClientSock = accept(serverSocket, (struct sockaddr *)&clientAddrs, &addr_size);
          FD_SET(newClientSock, &masterSet);
          printf("-New connection at socket: %d, ip: %s, port: %d\n", newClientSock, inet_ntoa(clientAddrs.sin_addr), ntohs(clientAddrs.sin_port));
          listOfConnectedClients[newClientSock].userName = false;
          listOfConnectedClients[newClientSock].userPass = false;
          strcpy(listOfConnectedClients[newClientSock].currDir, ".");
        }
        // 2nd case: read data
        else {
          char buffer[256];
          bzero(buffer, sizeof(buffer));
          int bytes = recv(i, buffer, sizeof(buffer), 0);  // receive
          printf("Message at: %d> %s\n", i, buffer);
          // case1: client has closed the connection
          if (bytes == 0) {
            printf("-Connection closed from client socket: %d. \n", i);
            close(i);               // we are done, close fd
            FD_CLR(i, &masterSet);  // remove the socket from the fd set
            // reset
            listOfConnectedClients[i].userName = false;
            listOfConnectedClients[i].userPass = false;

          }
          // case2: received some data from client
          else {
            char resCmd[256];
            char resDat[256];
            sepCmdDat(buffer, resCmd, resDat);

            /*
            if (resDat[0] == '\0') {
              printf("Error in command.\n");
            }
            */
            char allCmds[6][5] = {"USER", "PASS", "PORT", "LIST", "RETR", "STOR"};

            // USER command
            if (strcmp(resCmd, allCmds[0]) == 0)
              ftpUserCmd(i, resDat);

            // PASS command
            else if (strcmp(resCmd, allCmds[1]) == 0)
              ftpPassCmd(i, resDat);

            else if (!listOfConnectedClients[i].userPass || !listOfConnectedClients[i].userName) {
              char corResponse[] = "530";
              send(i, corResponse, sizeof(corResponse), 0);
              memset(resCmd, 0, strlen(resCmd));
              memset(resDat, 0, strlen(resDat));
            }
            // PORT command
            else if (strcmp(resCmd, allCmds[2]) == 0) {
              // Todo: list all the file directories in current active directory
              char corResponse[] = "200 PORT command successful.";
              listOfConnectedClients[i].userCurDataPort = atoi(resDat);
              send(i, corResponse, sizeof(corResponse), 0);
            }

            // LIST, RETR, STOR = fork a new process
            else if (strcmp(resCmd, allCmds[3]) == 0 || strcmp(resCmd, allCmds[4]) == 0) {
              int pid = fork();
              // child process
              if (pid == 0) {
                close(i);
                // TODO: do we need to reset FD set? Ask Shan.

                int newDataSock = initiateDataChannel(i);
                struct sockaddr_in clientAddrToSendData;

                bzero(&clientAddrToSendData, sizeof(clientAddrToSendData));
                clientAddrToSendData.sin_family = AF_INET;
                clientAddrToSendData.sin_port = htons(listOfConnectedClients[i].userCurDataPort);
                clientAddrToSendData.sin_addr.s_addr = INADDR_ANY;
                // connect

                int connection_status = connect(newDataSock, (struct sockaddr *)&clientAddrToSendData, sizeof(clientAddrToSendData));
                printf("New data sock: %d\n", newDataSock);

                // check for errors with the connection
                if (connection_status == -1) {
                  printf("There was an error making a connection to the remote socket \n\n");
                  exit(EXIT_FAILURE);
                } else {
                  // Todo: maybe this response is automated
                  char corResponse[] = "150 File status okay; about to open. data connection.";
                  send(newDataSock, corResponse, sizeof(corResponse), 0);

                  // LIST command
                  if (strcmp(resCmd, allCmds[3]) == 0)
                    ftpListCmd(newDataSock);

                  // RETR command
                  else if (strcmp(resCmd, allCmds[4]) == 0) {
                    // char dum[256];
                    //  recv(network_socket, &dum, sizeof(dum), 0);
                    char filename[256];
                    int i2 = 0;
                    int j2 = 0;
                    while (listOfConnectedClients[i].currDir[i2] != '\0') {
                      filename[j2] = listOfConnectedClients[i].currDir[i2];
                      i2++;
                      j2++;
                    }
                    filename[j2] = '/';
                    j2++;
                    i2 = 0;
                    while (resDat[i2] != '\0') {
                      filename[j2] = resDat[i2];
                      i2++;
                      j2++;
                    }
                    filename[j2] = '\0';
                    // send the file
                    //  text file
                    // TODO: check

                    FILE *txtile = fopen(filename, "r");
                    if (txtile != 0) {
                      printf("File opened.\n");
                      sFile(txtile, newDataSock);
                    } else {
                      char errormsg[] = "550 No such file or directory";
                      send(newDataSock, errormsg, strlen(errormsg), 0);
                    }
                    // binary file
                  }
                  // start sending the list command
                  close(newDataSock);
                  exit(1);
                }
              }
            }

            // Wrong command
            else {
              char corResponse[] = "202 Command not implemented.";
              send(i, corResponse, sizeof(corResponse), 0);
            }

            // clear the data
            memset(resCmd, 0, strlen(resCmd));
            memset(resDat, 0, strlen(resDat));
          }
          // displaying the message received
        }
      }
    }
    // end for
  }

  // close
  close(serverSocket);
  return 0;
}
