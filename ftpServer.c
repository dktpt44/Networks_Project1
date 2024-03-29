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

#define PORT 21
#define DATAPORT 20
#define USERMAX 1024  // max number of users that can be read from file
#define BUFFERSIZE 256

// structure for the connected client data
struct ClientStruct {
  int userIndex;
  int userCurDataPort;
  bool userName;
  bool userPass;
  char currDir[256];
  char *clientIPAddr;
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

// function to check authentication
bool isAuthenticated(int i) {
  if (!listOfConnectedClients[i].userPass || !listOfConnectedClients[i].userName) {
    // not authenticated
    char corResponse[BUFFERSIZE] = "530 Not logged in.";
    send(i, corResponse, sizeof(corResponse), 0);
    return false;
  }
  return true;
}

// function to read users from file
void loadUserFile() {
  FILE *userFile = fopen("user.txt", "r");
  if (!userFile) {
    // if user file does not exist
    userCount = 0;
    printf("User.txt file not found. \n");
    return;
  }
  int strCount = 0;
  char str = ' ';
  // get the number of users
  while (!feof(userFile)) {
    str = fgetc(userFile);
    if (str == '\n') {
      userCount += 1;
    }
  }
  rewind(userFile);
  // store in the variable
  while (strCount < userCount + 1) {
    fscanf(userFile, "%s %s", accFile[strCount].user, accFile[strCount].pw);
    strCount += 1;
  }
}

// function to intiate TCP listen
int initiateTcp() {
  // create a socket
  int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
  if (serverSocket < 0) {  // check for fail error
    perror("socket:");
    exit(EXIT_FAILURE);
  }
  int value = 1;
  setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));

  struct sockaddr_in servAddr;  // define server address structure
  bzero(&servAddr, sizeof(servAddr));
  servAddr.sin_family = AF_INET;
  servAddr.sin_port = htons(PORT);
  servAddr.sin_addr.s_addr = INADDR_ANY;
  // bind
  if (bind(serverSocket, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0) {  // bind
    perror("bind failed");
    exit(EXIT_FAILURE);
  }

  if (listen(serverSocket, 5) < 0) {  // listen
    perror("listen failed");
    close(serverSocket);
    exit(EXIT_FAILURE);
  } else {
    printf("Server is listening at port: %d, ip: %s\n", ntohs(servAddr.sin_port), inet_ntoa(servAddr.sin_addr));
  }
  return serverSocket;
}

// function to break a string to two strings
void sepCmdDat(char *buff, char *cmdstr, char *datstr) {
  int responseSize = strlen(buff), strindx = 0;
  int variableCount = 1;
  // break into command and data

  for (int j = 0; j <= responseSize; j++) {
    if (buff[j] == ' ') {
      // command string e.g. "USER", "PASS", "STOR", etc.
      if (variableCount == 1)
        cmdstr[strindx] = '\0';
      strindx = 0;
      variableCount++;
    } else {
      // data string (if any)
      if (variableCount == 1) {
        // add to command string
        cmdstr[strindx] = buff[j];
      } else {
        // add to command data string
        datstr[strindx] = buff[j];
      }
      strindx++;
    }
  }
}

// USER command, i = socket
void ftpUserCmd(int i, char *resDat) {
  bool foundDat = false;
  // loop to check if user exists
  for (int n = 0; n < userCount; n++) {
    if (strcmp(resDat, accFile[n].user) == 0) {
      // if found
      foundDat = true;
      listOfConnectedClients[i].userIndex = n;  // found at nth pos in array
      listOfConnectedClients[i].userName = true;
      char corResponse[BUFFERSIZE] = "331 Username OK, need password.";
      send(i, corResponse, sizeof(corResponse), 0);
      break;
    }
  }
  // if not found
  if (!foundDat) {
    char corResponse[BUFFERSIZE] = "530 Not logged in.";
    send(i, corResponse, sizeof(corResponse), 0);
  }
}

// PASS command, i = socket
void ftpPassCmd(int i, char *resDat) {
  // check if USER command is run first
  if (!listOfConnectedClients[i].userName) {
    char corResponse[BUFFERSIZE] = "530 Not logged in.";
    send(i, corResponse, sizeof(corResponse), 0);
  } else {
    // check if password is correct
    if (strcmp(resDat, accFile[listOfConnectedClients[i].userIndex].pw) == 0) {
      char corResponse[BUFFERSIZE] = "230 User logged in, proceed.";
      listOfConnectedClients[i].userPass = true;
      send(i, corResponse, sizeof(corResponse), 0);
    } else {
      char corResponse[BUFFERSIZE] = "530 Not logged in.";
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
    char buffer2[BUFFERSIZE];
    // loop to get all the files and folders
    while ((drty = readdir(direct)) != NULL) {
      // send
      strcpy(buffer2, drty->d_name);
      send(i, buffer2, sizeof(buffer2), 0);
      bzero(buffer2, sizeof(buffer2));
    }
    closedir(direct);
  }
}

// function to parse port command and ip address
void ftpPortCmd(int i, char *resDat) {
  // variables to parse PORT command
  int ix = 0, p1x = 0, p2x = 0;
  int commaindx = 0;
  char ipAdr[256];
  char p1[256];
  char p2[256];

  int ipIndx = 0;
  // parse port and ip
  while (resDat[ix] != '\0') {
    if (resDat[ix] == ',') {
      // add . instead of ,
      commaindx++;
      if (commaindx < 4) {
        ipAdr[ipIndx] = '.';
      }
      ipIndx++;
    } else if (commaindx < 4) {
      // extract ip
      ipAdr[ipIndx] = resDat[ix];
      ipIndx++;
    } else if (commaindx == 4) {
      // extract p1 info
      ipAdr[ipIndx] = '\0';
      p1[p1x] = resDat[ix];
      p1x++;
    } else if (commaindx == 5) {
      // extract p2 info
      p1[p1x] = '\0';
      p2[p2x] = resDat[ix];
      p2x++;
    }
    ix++;
  }
  p2[p2x] = '\0';
  int p1i, p2i;
  // convert to int
  sscanf(p1, "%d", &p1i);
  sscanf(p2, "%d", &p2i);

  // save
  listOfConnectedClients[i].userCurDataPort = p1i * 256 + p2i;
  listOfConnectedClients[i].clientIPAddr = ipAdr;

  // send response
  char corResponse[BUFFERSIZE] = "200 PORT command successful.";
  send(i, corResponse, sizeof(corResponse), 0);
}

// function to initiate second TCP channel
int initiateDataChannel() {
  // create a new data socket
  int newDataSock = socket(AF_INET, SOCK_STREAM, 0);
  if (newDataSock == -1) {
    printf("socket creation failed..\n");
    exit(EXIT_FAILURE);
  }
  int value = 1;
  setsockopt(newDataSock, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));

  // bind it to port 20
  struct sockaddr_in thisMachineAddr;
  bzero(&thisMachineAddr, sizeof(thisMachineAddr));
  thisMachineAddr.sin_family = AF_INET;
  thisMachineAddr.sin_port = htons(DATAPORT);
  thisMachineAddr.sin_addr.s_addr = INADDR_ANY;
  if (bind(newDataSock, (struct sockaddr *)&thisMachineAddr, sizeof(struct sockaddr_in)) == 0) {
  }
  return newDataSock;
}

// function to send file over socket i
void sFile(char *filename, int i) {
  // try to open file
  FILE *fp = fopen(filename, "rb");
  if (fp != NULL) {
    // if file exists
    char corResponse[BUFFERSIZE] = "150 File status okay; about to open. data connection.";
    send(i, corResponse, sizeof(corResponse), 0);
    printf("+Sending data, using socket: %d, to client port: %d\n", i, listOfConnectedClients[i].userCurDataPort);

    // send file size first
    fseek(fp, 0L, SEEK_END);
    // calculating the size of the file
    long int res = ftell(fp);
    char filesizemsg[BUFFERSIZE];
    sprintf(filesizemsg, "%ld", res);
    send(i, filesizemsg, sizeof(filesizemsg), 0);

    char data[BUFFERSIZE];
    bzero(data, BUFFERSIZE);

    // move pointer to top of the file
    fseek(fp, 0L, SEEK_SET);
    // keep sending data from the file
    while (true) {
      int readLen = fread(data, 1, sizeof(data), fp);
      if (readLen <= 0) {
        break;
      }
      // printf("-:%s\n", data);
      if (send(i, data, readLen, 0) == -1) {
        perror("--Error in sending file.");
        exit(1);
      }
      bzero(data, sizeof(data));
    }

    fclose(fp);
    // send transfer complete
    char complete[BUFFERSIZE] = "226 Transfer completed.";
    send(i, complete, sizeof(complete), 0);
  } else {
    printf("File does not exist.\n");

    char errormsg[BUFFERSIZE] = "550 No such file or directory";
    send(i, errormsg, sizeof(errormsg), 0);
  }
}

// function to receive file from the client
void recvFile(char *filename, int i) {
  int n;
  char buffer[BUFFERSIZE];
  bzero(buffer, sizeof(buffer));
  FILE *fp;
  fp = fopen(filename, "ab");
  // loop until data is received
  while (1) {
    n = recv(i, buffer, sizeof(buffer), 0);
    // write into file
    fwrite(buffer, 1, n, fp);
    bzero(buffer, sizeof(buffer));
    if (n <= 0) {
      break;
    }
  }

  printf("Transfer completed.\n");

  fclose(fp);
  return;
}

// function to check if bad sequence of command
bool checkIfBadSeq(char *resDat, char listOFcmd[8][6], int noSiz) {
  // loop through the list of commands
  for (int cx = 0; cx < noSiz; cx++) {
    if (strcmp(listOFcmd[cx], resDat) == 0)
      return true;
  }
  return false;
}

// main function
int main() {
  // reading from the file
  char initDir[BUFFERSIZE];
  getcwd(initDir, BUFFERSIZE);
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
          // accept new connection
          int newClientSock = accept(serverSocket, 0, 0);
          FD_SET(newClientSock, &masterSet);
          printf("+New connection at socket: %d\n", newClientSock);
          // reset
          listOfConnectedClients[newClientSock].userName = false;
          listOfConnectedClients[newClientSock].userPass = false;
          strcpy(listOfConnectedClients[newClientSock].currDir, initDir);
        }

        // 2nd case: read data
        else {
          char buffer[BUFFERSIZE];
          bzero(buffer, sizeof(buffer));
          int bytes = recv(i, buffer, sizeof(buffer), 0);  // receive the first command
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
            char allCmds[8][6] = {"USER", "PASS", "PORT", "LIST", "RETR", "STOR", "PWD", "CWD"};
            chdir(listOfConnectedClients[i].currDir);

            // USER command
            if (strcmp(resCmd, allCmds[0]) == 0)
              ftpUserCmd(i, resDat);

            // PASS command
            else if (strcmp(resCmd, allCmds[1]) == 0)
              ftpPassCmd(i, resDat);

            // PORT command
            else if (strcmp(resCmd, allCmds[2]) == 0) {
              if (isAuthenticated(i))
                ftpPortCmd(i, resDat);
            }

            // LIST, RETR, STOR = fork a new process
            else if ((strcmp(resCmd, allCmds[3]) == 0) || (strcmp(resCmd, allCmds[4]) == 0) || (strcmp(resCmd, allCmds[5]) == 0)) {
              if (isAuthenticated(i)) {
                int pid = fork();
                // child process
                if (pid == 0) {
                  int newDataSock = initiateDataChannel();
                  close(i);
                  struct sockaddr_in clientAddrToSendData;

                  bzero(&clientAddrToSendData, sizeof(clientAddrToSendData));
                  clientAddrToSendData.sin_family = AF_INET;
                  clientAddrToSendData.sin_port = htons(listOfConnectedClients[i].userCurDataPort);
                  clientAddrToSendData.sin_addr.s_addr = INADDR_ANY;
                  // connect

                  int connection_status = connect(newDataSock, (struct sockaddr *)&clientAddrToSendData, sizeof(clientAddrToSendData));

                  // check for errors with the connection
                  if (connection_status == -1) {
                    printf("There was an error making a connection to the remote socket \n\n");
                    exit(EXIT_FAILURE);
                  } else {
                    // LIST command
                    if (strcmp(resCmd, allCmds[3]) == 0) {
                      char corResponse[BUFFERSIZE] = "150 File status okay; about to open. data connection.";
                      send(newDataSock, corResponse, sizeof(corResponse), 0);
                      ftpListCmd(newDataSock);

                    }

                    // RETR command
                    else if (strcmp(resCmd, allCmds[4]) == 0) {
                      char filename[256];
                      int i2 = 0;
                      int j2 = 0;
                      // parsing current location
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
                      sFile(filename, newDataSock);
                      printf("-Sending done, closing sock: %d, to client port: %d\n", newDataSock, listOfConnectedClients[i].userCurDataPort);
                    }

                    // STOR command
                    else if (strcmp(resCmd, allCmds[5]) == 0) {
                      char corResponse[BUFFERSIZE] = "160 Waiting for file.";
                      send(newDataSock, corResponse, sizeof(corResponse), 0);

                      char filename[256];
                      int i2 = 0;
                      int j2 = 0;
                      // parsing current location
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

                      recvFile(filename, newDataSock);
                    }

                    close(newDataSock);
                    exit(1);
                  }
                }
              }
            }

            // PWD
            else if (strcmp(resCmd, allCmds[6]) == 0) {
              if (isAuthenticated(i)) {
                // parsing for current location
                char succMsg[] = "257 ";
                char returnMsg[BUFFERSIZE];
                int j2 = 0;
                int i2 = 0;
                while (succMsg[j2] != '\0') {
                  returnMsg[i2] = succMsg[j2];
                  i2++;
                  j2++;
                }
                j2 = 0;
                while (listOfConnectedClients[i].currDir[j2] != '\0') {
                  returnMsg[i2] = listOfConnectedClients[i].currDir[j2];
                  i2++;
                  j2++;
                }
                returnMsg[i2] = '\0';
                // send
                send(i, returnMsg, sizeof(returnMsg), 0);
                bzero(returnMsg, sizeof(returnMsg));
              }
            }

            // CWD command
            else if (strcmp(resCmd, allCmds[7]) == 0) {
              if (isAuthenticated(i)) {
                char newDir[BUFFERSIZE];
                char responseMsg[BUFFERSIZE];
                if (resDat[0] == '.') {
                  strcpy(newDir, resDat);
                } else if (resDat[0] != '/') {  // if resDat doesn't starts with '/' this means that this input is a folder
                  // parsing for current location
                  int e2 = 0;
                  int r2 = 0;
                  while (listOfConnectedClients[i].currDir[e2] != '\0') {
                    newDir[r2] = listOfConnectedClients[i].currDir[e2];
                    r2++;
                    e2++;
                  }
                  e2 = 0;
                  newDir[r2] = '/';
                  r2++;
                  while (resDat[e2] != '\0') {
                    newDir[r2] = resDat[e2];
                    r2++;
                    e2++;
                  }
                } else {
                  strcpy(newDir, resDat);
                }
                if (chdir(newDir) == -1) {
                  strcpy(responseMsg, "550 No such directory.\n");
                } else {
                  bzero(newDir, sizeof(newDir));
                  getcwd(newDir, sizeof(newDir));
                  strcpy(listOfConnectedClients[i].currDir, newDir);

                  strcpy(responseMsg, "200 directory changed to ");
                  int indx1 = strlen(responseMsg), indx2 = 0;
                  while (newDir[indx2] != '\0') {
                    responseMsg[indx1] = newDir[indx2];
                    indx1++;
                    indx2++;
                  }
                  responseMsg[indx1] = '\0';

                  // TODO: change the folder name to actual folder name
                }
                send(i, responseMsg, sizeof(responseMsg), 0);
                bzero(responseMsg, sizeof(responseMsg));
                bzero(newDir, sizeof(newDir));
              }
            }

            // else if none of the above
            else {
              char corResponse[BUFFERSIZE];
              if (checkIfBadSeq(resDat, allCmds, 8)) {
                strcpy(corResponse, "503 Bad sequence of commands.");
              } else {
                strcpy(corResponse, "202 Command not implemented.");
              }
              send(i, corResponse, sizeof(corResponse), 0);
            }

            // clear the data
            memset(resCmd, 0, sizeof(resCmd));
            memset(resDat, 0, sizeof(resDat));
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
