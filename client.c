#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#define PORT 21
#define SIZE 1024
#define BUFFERSIZE 256

unsigned short controlPort;  // randomly assigned control port
int newPortForData;          // port for data transfer
char *thisIPaddr;            // max number of strings allowed in ip
char curWorkingDir[256];     // holds the current working directory information
int portDatIndex = 0;        // index for the current data transfer port
int listOfPorts[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

// function to get new port
void getNewPort() {
  // get port+n
  portDatIndex++;
  portDatIndex %= 10;

  // initializing the port numbers
  if (listOfPorts[portDatIndex] == 0) {
    listOfPorts[portDatIndex] = controlPort + portDatIndex;
  }
}

// function to initiate initial TCP connection
int initiateTCP() {
  // create a new socket = master socket for control port
  int network_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (network_socket == -1) {  // check for fail error
    printf("Socket creation failed..\n");
    exit(EXIT_FAILURE);
  }

  int value = 1;
  setsockopt(network_socket, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));

  // save the ip address in the global variable to send it to the port command
  struct sockaddr_in servAddr;
  bzero(&servAddr, sizeof(servAddr));
  servAddr.sin_family = AF_INET;
  servAddr.sin_port = htons(PORT);
  servAddr.sin_addr.s_addr = INADDR_ANY;

  // connect
  if (connect(network_socket, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0) {
    perror("Connection Problem");
    exit(EXIT_FAILURE);
  } else {
    printf("220 Service ready for new user.\n");
  }

  // getting this client's information
  struct sockaddr_in clientAddr;
  socklen_t clientSocketSz = sizeof(clientAddr);
  getsockname(network_socket, (struct sockaddr *)&clientAddr, &clientSocketSz);
  // getting client port number
  controlPort = ntohs(clientAddr.sin_port);
  // getting client ip address
  thisIPaddr = inet_ntoa(clientAddr.sin_addr);
  // printf("Client is connecting through port %d, ip:(%s)\n", controlPort, thisIPaddr);

  return network_socket;
}

// function to initiate data connection
int iniDataConnection() {
  // create new socket
  int newSocket = socket(AF_INET, SOCK_STREAM, 0);
  // check for fail error
  if (newSocket == -1) {
    printf("socket creation failed..\n");
    exit(EXIT_FAILURE);
  }
  if (setsockopt(newSocket, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {
    perror("binding failed! \n");
  }
  // define server address structure
  struct sockaddr_in transferAddress;
  bzero(&transferAddress, sizeof(transferAddress));
  transferAddress.sin_family = AF_INET;
  transferAddress.sin_port = htons(listOfPorts[portDatIndex]);
  transferAddress.sin_addr.s_addr = INADDR_ANY;

  // bind the socket to our specified IP and port
  if (bind(newSocket, (struct sockaddr *)&transferAddress, sizeof(transferAddress)) < 0) {
    printf("socket bind failed..\n");
    exit(EXIT_FAILURE);
  }

  // after it is bound, we can listen for connections
  if (listen(newSocket, 5) < 0) {
    printf("Listen failed..\n");
    close(newSocket);
    exit(EXIT_FAILURE);
  }
  return newSocket;
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
      } else if (variableCount == 2) {
        // add to command data string
        datstr[strindx] = buff[j];
      }
      strindx++;
    }
  }
}

// function to send port command
int sendPortCmd(int sockI) {
  char portCmd[256] = "PORT ";
  // get the new port
  getNewPort();
  int newPortForData = listOfPorts[portDatIndex];

  // convert ip
  int ix = 5;
  int jx = 0;
  while (thisIPaddr[jx] != '\0') {
    if (thisIPaddr[jx] == '.') {
      portCmd[ix] = ',';
    } else {
      portCmd[ix] = thisIPaddr[jx];
    }
    ix++;
    jx++;
  }
  portCmd[ix] = ',';
  ix++;

  // convert port
  int p1 = newPortForData / 256;
  int p2 = newPortForData % 256;

  char p1c[256];
  char p2c[256];
  // convert to string
  sprintf(p1c, "%d", p1);
  sprintf(p2c, "%d", p2);

  // adding p1f
  jx = 0;
  while (p1c[jx] != '\0') {
    portCmd[ix] = p1c[jx];
    ix++;
    jx++;
  }
  // adding comma
  portCmd[ix] = ',';
  ix++;

  // adding p2
  jx = 0;
  while (p2c[jx] != '\0') {
    portCmd[ix] = p2c[jx];
    ix++;
    jx++;
  }

  portCmd[ix] = '\0';
  return send(sockI, portCmd, sizeof(portCmd), 0);
}

// function to receive file over data channel
void recvFile(int i, char *filename) {
  printf("\n");
  int n;
  char buffer[BUFFERSIZE];
  bzero(buffer, sizeof(buffer));
  // receive file size
  recv(i, buffer, sizeof(buffer), 0);
  long int fsize;
  // convert to long int
  sscanf(buffer, "%ld", &fsize);
  long int readBytesCount = 0;

  FILE *fp;
  bzero(buffer, sizeof(buffer));
  fp = fopen(filename, "ab");
  while (1) {
    n = recv(i, buffer, sizeof(buffer), 0);

    readBytesCount += n;
    if ((readBytesCount >= fsize) && (readBytesCount < fsize + BUFFERSIZE)) {
      // it means buffer contains some file data and other 226 Transfer complete status. Extracting them
      int resIx = 0;
      int partDat = readBytesCount - fsize;
      fwrite(buffer, 1, BUFFERSIZE - partDat, fp);
      char resd[BUFFERSIZE];
      int limt = sizeof(buffer);
      for (int jx = BUFFERSIZE - partDat; jx < limt; jx++) {
        resd[resIx] = buffer[jx];
        resIx++;
      }
      printf("%s", resd);
    } else if (readBytesCount <= fsize) {
      // write to file in this case
      fwrite(buffer, 1, n, fp);
    } else {
      printf("%s\n", buffer);
    }
    bzero(buffer, sizeof(buffer));
    if (n <= 0) {
      break;
    }
  }

  fclose(fp);
  return;
}

// function to send file to the server
void sendFile(int i, char *filename) {
  FILE *fp = fopen(filename, "rb");
  if (fp != NULL) {
    printf("File opened.\n");

    char data[BUFFERSIZE];
    bzero(data, BUFFERSIZE);
    // read file data
    while (true) {
      int readLen = fread(data, 1, sizeof(data), fp);
      if (readLen <= 0) {
        break;
      }
      // send through the socket
      if (send(i, data, readLen, 0) == -1) {
        perror("--Error in sending file.");
        exit(1);
      }
      bzero(data, sizeof(data));
    }
    // close file
    fclose(fp);
    printf("226 Transfer completed.\n");
  }
}

// main function
int main() {
  // create a socket
  int network_socket = initiateTCP();
  char buffer[BUFFERSIZE];
  bzero(buffer, sizeof(buffer));

  while (1) {
    printf("ftp> ");

    // get input from user
    fgets(buffer, sizeof(buffer), stdin);
    buffer[strcspn(buffer, "\n")] = 0;  // remove trailing newline char from buffer, fgets does not remove it

    if (strcmp(buffer, "exit") == 0 || strcmp(buffer, "QUIT") == 0) {
      printf("221 Service closing control connection.\n");
      close(network_socket);
      break;
    } else {
      // parse command : break into command and data
      char resCmd[256];
      char resDat[256];
      bzero(resCmd, sizeof(resCmd));
      bzero(resDat, sizeof(resDat));

      sepCmdDat(buffer, resCmd, resDat);
      char allCmds[7][6] = {"RETR", "LIST", "STOR", "!PWD", "!CWD", "!LIST", "PORT"};

      // check if any of the LIST, RETR or STOR commands are input by user
      if ((strcmp(resCmd, allCmds[0]) == 0 || strcmp(resCmd, allCmds[1]) == 0 || strcmp(resCmd, allCmds[2]) == 0)) {
        //  send the port command to the server first
        if (sendPortCmd(network_socket) < 0) {
          perror("send");
          exit(EXIT_FAILURE);
        }

        else {
          // get data back
          char response[BUFFERSIZE];
          bzero(response, sizeof(response));
          recv(network_socket, &response, sizeof(response), 0);
          printf("%s\n", response);  // prints port command success

          // check if not authenticated, do not send the list command
          char checkRes[BUFFERSIZE] = "530 Not logged in.";
          // if it is store command check if the file exists
          bool dontRun = false;
          if (strcmp(resCmd, allCmds[2]) == 0 && (strcmp(checkRes, response) != 0)) {
            FILE *fp = fopen(resDat, "rb");
            if (!fp) {
              dontRun = true;
              printf("550 No such file or directory.\n");
            } else
              fclose(fp);
          }
          if ((strcmp(checkRes, response) != 0) && !dontRun) {
            // send the actual command now

            if (send(network_socket, buffer, sizeof(buffer), 0) < 0) {
              perror("send");
              exit(EXIT_FAILURE);
            }

            // fork a new process for data transfer
            int pid = fork();
            // if it is the child process
            if (pid == 0) {
              // new TCP connection and listen to the port

              close(network_socket);  // close the copy of master socket in child process
              int newSocket = iniDataConnection();
              int client_socket = accept(newSocket, 0, 0);

              char buffer2[BUFFERSIZE];
              bzero(buffer2, sizeof(buffer2));
              recv(client_socket, &buffer2, sizeof(buffer2), 0);  // receive
              // prints 150 file status okay
              printf("%s", buffer2);
              // start receiving data

              // RETR command
              if (strcmp(resCmd, allCmds[0]) == 0) {
                char errormsg[BUFFERSIZE] = "550 No such file or directory";
                if (!(strcmp(errormsg, buffer2) == 0))
                  recvFile(client_socket, resDat);
                else
                  printf("\n");
                bzero(buffer2, sizeof(buffer2));

              }

              // LIST command
              else if (strcmp(resCmd, allCmds[1]) == 0) {
                int endWhile = 1;
                // loop until we get data
                while (endWhile != 0) {
                  endWhile = recv(client_socket, &buffer2, sizeof(buffer2), 0);
                  printf("%s\n", buffer2);
                  bzero(buffer2, sizeof(buffer2));
                }
              }

              // STOR command
              else if (strcmp(resCmd, allCmds[2]) == 0) {
                sendFile(client_socket, resDat);
              }

              close(client_socket);
              exit(1);
            }

            else {  // if it is the parent process
              wait(NULL);
              // TODO: possible error close socket
            }
          }
        }

      }

      // if user tries to send port command
      else if (strcmp(resCmd, allCmds[6]) == 0)
        printf("503 Bad sequence of commands.\n");

      // !LIST command
      else if (strcmp(resCmd, allCmds[5]) == 0)
        system("ls");

      // !PWD command
      else if (strcmp(resCmd, allCmds[3]) == 0)
        system("pwd");

      // !CWD command
      else if (strcmp(resCmd, allCmds[4]) == 0) {
        // wrong input if more than 2 separate words
        if (chdir(resDat) == -1) {
          printf("No such directory.\n");
        } else {
          getcwd(curWorkingDir, sizeof(curWorkingDir));
          printf("Directory changed to %s\n", curWorkingDir);
        }
      }

      // for rest of the commands
      else {
        if (send(network_socket, buffer, strlen(buffer), 0) < 0) {
          perror("send");
          exit(EXIT_FAILURE);
        } else {
          // get data back
          char response[BUFFERSIZE];
          bzero(response, sizeof(response));

          recv(network_socket, &response, sizeof(response), 0);
          printf("%s\n", response);
        }
      }
      // end if
    }

    bzero(buffer, sizeof(buffer));
  }

  return 0;
}
