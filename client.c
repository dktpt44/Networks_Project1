#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#define PORT 9007
#define SIZE 1024
#define BUFFERSIZE 256

unsigned short controlPort;
int newPortForData;
bool userAuthenticated = false;
char curWorkingDir[256];

// function to get new port
void getNewPort()
{
  newPortForData = controlPort++;
}

// function to initiate initial TCP connection
int initiateTCP()
{
  int network_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (network_socket == -1)
  { // check for fail error
    printf("Socket creation failed..\n");
    exit(EXIT_FAILURE);
  }

  int value = 1;
  setsockopt(network_socket, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));

  struct sockaddr_in servAddr;
  bzero(&servAddr, sizeof(servAddr));
  servAddr.sin_family = AF_INET;
  servAddr.sin_port = htons(PORT);
  servAddr.sin_addr.s_addr = INADDR_ANY;

  // connect
  if (connect(network_socket, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0)
  {
    perror("connect");
    exit(EXIT_FAILURE);
  }
  else
  {
    printf("220 Service ready for new user.\n");
  }

  /* for client address */
  struct sockaddr_in clientAddr;
  socklen_t clientSocketSz = sizeof(clientAddr);
  getsockname(network_socket, (struct sockaddr *)&clientAddr, &clientSocketSz);
  // getting client port number
  controlPort = ntohs(clientAddr.sin_port);
  return network_socket;
}

// function to initiate data connection
int iniDataConnection()
{
  int newSocket = socket(AF_INET, SOCK_STREAM, 0);
  // check for fail error
  if (newSocket == -1)
  {
    printf("socket creation failed..\n");
    exit(EXIT_FAILURE);
  }
  int value = 1;
  if (setsockopt(newSocket, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
  {
    perror("binding failed! \n");
  }
  // define server address structure
  struct sockaddr_in transferAddress;
  bzero(&transferAddress, sizeof(transferAddress));
  transferAddress.sin_family = AF_INET;
  transferAddress.sin_port = htons(newPortForData);
  transferAddress.sin_addr.s_addr = INADDR_ANY;

  // bind the socket to our specified IP and port
  if (bind(newSocket, (struct sockaddr *)&transferAddress, sizeof(transferAddress)) < 0)
  {
    printf("socket bind failed..\n");
    exit(EXIT_FAILURE);
  }

  // after it is bound, we can listen for connections
  if (listen(newSocket, 5) < 0)
  {
    printf("Listen failed..\n");
    close(newSocket);
    exit(EXIT_FAILURE);
  }
  return newSocket;
}

// function to break a string to two strings
void sepCmdDat(char *buff, char *cmdstr, char *datstr)
{
  int responseSize = strlen(buff), strindx = 0;
  bool secondStr = false;
  // break into command and data

  for (int j = 0; j <= responseSize; j++)
  {
    if (buff[j] == ' ')
    {
      // command string e.g. "USER", "PASS", "STOR", etc.
      secondStr = true;
      cmdstr[strindx] = '\0';
      strindx = 0;
    }
    else
    {
      // data string (if any)
      if (!secondStr)
      {
        cmdstr[strindx] = buff[j];
      }
      else
      {
        datstr[strindx] = buff[j];
      }
      strindx++;
    }
  }
}

// function to send port command
int sendPortCmd(int sockI)
{
  char portCmd[256] = "PORT ";
  char portNoString[256];
  bzero(portNoString, sizeof(portNoString));
  getNewPort();
  // TODO: convert port
  sprintf(portNoString, "%d", newPortForData);

  int i2 = 5;
  int j2 = 0;
  while (portNoString[j2] != '\0')
  {
    portCmd[i2] = portNoString[j2];
    i2++;
    j2++;
  }
  portCmd[i2] = '\0';
  return send(sockI, portCmd, strlen(portCmd), 0);
}

// function to receive file over data channel
void recvFile(int i, char *filename)
{
  int n;
  char buffer[SIZE];
  FILE *fp;
  fp = fopen(filename, "w");
  while (1)
  {
    n = recv(i, buffer, SIZE, 0);
    fprintf(fp, "%s", buffer);
    printf("p2: %s\n", buffer);

    if (n <= 0)
    {
      break;
      return;
    }
    bzero(buffer, SIZE);
  }
  fclose(fp);
  return;
}

int main()
{
  // create a socket
  int network_socket = initiateTCP();
  char buffer[256];

  while (1)
  {
    printf("ftp> ");

    // get input from user
    fgets(buffer, sizeof(buffer), stdin);
    buffer[strcspn(buffer, "\n")] = 0; // remove trailing newline char from buffer, fgets does not remove it

    if (strcmp(buffer, "exit") == 0)
    {
      printf("Connection to the server closed.\n");
      close(network_socket);
      break;
    }
    else
    {
      // parse command : break into command and data
      char resCmd[256];
      char resDat[256];
      sepCmdDat(buffer, resCmd, resDat);
      char allCmds[6][5] = {"RETR", "LIST", "STOR", "!PWD", "!CWD", "!LIST"};

      // check if any of the LIST, RETR or STOR commands are input by user
      if ((strcmp(resCmd, allCmds[0]) == 0 || strcmp(resCmd, allCmds[1]) == 0 || strcmp(resCmd, allCmds[2]) == 0) && userAuthenticated)
      {
        /* send the port command to the server */
        if (sendPortCmd(network_socket) < 0)
        {
          perror("send");
          exit(EXIT_FAILURE);
        }
        else
        {
          // get data back
          char response[1024];
          bzero(response, sizeof(response));
          recv(network_socket, &response, sizeof(response), 0);
          printf("%s\n", response);

          // send the command now
          if (send(network_socket, buffer, strlen(buffer), 0) < 0)
          {
            perror("send");
            exit(EXIT_FAILURE);
          }

          // fork a new process for data transfer
          int pid = fork();
          // if it is the child process
          if (pid == 0)
          {
            // new TCP connection and listen to the port

            close(network_socket); // close the copy of master socket in child process
            int newSocket = iniDataConnection();
            int client_socket = accept(newSocket, 0, 0);

            char buffer2[256];
            bzero(buffer2, sizeof(buffer2));
            recv(client_socket, &buffer2, sizeof(buffer2), 0); // receive

            printf("Here: %s\n", buffer2);
            // start receiving data

            // RETR command
            if (strcmp(resCmd, allCmds[0]) == 0)
            {
              char dummy2[] = "ddf";
              send(client_socket, dummy2, sizeof(dummy2), 0);
              recvFile(client_socket, resDat);
            }

            // LIST command
            else if (strcmp(resCmd, allCmds[1]) == 0)
            {
              char dummy[] = "dummy";

              int endWhile = 1;

              while (endWhile != 0)
              {
                send(client_socket, dummy, sizeof(dummy), 0); // send
                bzero(buffer2, sizeof(buffer2));
                endWhile = recv(client_socket, &buffer2, sizeof(buffer2), 0);
                printf("%s\n", buffer2);
              }
            }

            // STOR command
            else if (strcmp(resCmd, allCmds[2]) == 0)
            {
            }

            close(client_socket);
            exit(1);
          }

          else
          { // if it is the parent process
            wait(NULL);
            // TODO: possible error close socket
          }
        }
      }
      //
    
      else if (strcmp(resCmd, allCmds[5]) == 0)
      {
        system("ls");
      }

      else if (strcmp(resCmd, allCmds[3]) == 0)
      {
        system("pwd");
      }

      else if (strcmp(resCmd, allCmds[4]) == 0)
      {
        if(chdir(resDat) == -1){
          printf("550 No such directory.\n");
        } else{
          getcwd(curWorkingDir, sizeof(curWorkingDir));
          printf("200 directory changed to pathname/foldername.\n");
        }
      }
      else
      {
        printf("%d \n", strcmp(resCmd, allCmds[3]));
        // for rest of the commands
        if (send(network_socket, buffer, strlen(buffer), 0) < 0)
        {
          perror("send");
          exit(EXIT_FAILURE);
        }
        else
        {
          // get data back
          char response[1024];
          recv(network_socket, &response, sizeof(response), 0);
          printf("%s\n", response);

          char passCorrect[] = "230 User logged in, proceed.";
          if (strcmp(response, passCorrect) == 0)
            userAuthenticated = true;
        }
      }
      // end if
    }

    bzero(buffer, sizeof(buffer));
  }

  return 0;
}
