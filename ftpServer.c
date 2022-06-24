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

// list of connected clients
struct ClientStruct
{
  int userIndex;
  int userCurDataPort;
  bool userName;
  bool userPass;
  char currDir[256];
};

struct acc
{ // structure to store username and password
  char user[256];
  char pw[256];
};

struct ClientStruct listOfConnectedClients[FD_SETSIZE];
// first char = username correct?
// second char = password correct?

void sFile(FILE *fp, int i)
{
  int n;
  char data[SIZE] = {0};

  while (fgets(data, SIZE, fp) != NULL)
  {
    printf("%s\n", data);
    if (send(i, data, sizeof(data), 0) == -1)
    {
      perror("[-]Error in sending file.");
      exit(1);
    }
    bzero(data, SIZE);
  }

  fclose(fp);
  char complete[] = "226 Transfer completed.";
  send(i, complete, sizeof(complete), 0);
}

int main()
{
  // reading from the file
  static struct acc accFile[USERMAX];
  FILE *userFile = fopen("user.txt", "r");
  int userCount = 0; // holds the number of users read from file
  int strCount = 0;
  char str;
  while (!feof(userFile))
  {
    str = fgetc(userFile);
    if (str == '\n')
    {
      userCount += 1;
    }
  }
  rewind(userFile);
  while (strCount < userCount + 1)
  {
    fscanf(userFile, "%s %s", accFile[strCount].user, accFile[strCount].pw);
    strCount += 1;
  }

  // for initial TCP connection
  int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
  if (serverSocket < 0)
  { // check for fail error
    perror("socket:");
    exit(EXIT_FAILURE);
  }
  int value = 1;
  setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)); //&(int){1},sizeof(int)

  struct sockaddr_in server_address; // define server address structure
  bzero(&server_address, sizeof(server_address));
  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(PORT);
  server_address.sin_addr.s_addr = INADDR_ANY;
  if (bind(serverSocket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
  { // bind
    perror("bind failed");
    exit(EXIT_FAILURE);
  }

  if (listen(serverSocket, 5) < 0)
  { // listen
    perror("listen failed");
    close(serverSocket);
    exit(EXIT_FAILURE);
  }
  else
  {
    printf("Server is listening.\n");
  }

  fd_set masterSet;                 // DECLARE fd set (file descriptor sets : a collection of file descriptors)
  FD_ZERO(&masterSet);              // zero out/iniitalize our set of all sockets
  FD_SET(serverSocket, &masterSet); // adds one socket (the current socket) to the fd set of all sockets

  while (true)
  {
    fd_set copySet = masterSet; // select() is destructive: it's going to change the set we pass in, so we need a temporary copy
    // The maximum number of sockets supported by select() has an upper limit, represented by FD_SETSIZE (typically 1024).
    if (select(FD_SETSIZE, &copySet, NULL, NULL, NULL) < 0)
    {
      perror("select error");
      exit(EXIT_FAILURE);
    }

    // select returns the fd_set containing JUST the file descriptors ready for reading
    // to know which ones are ready, we have to loop through and check go from 0 to FD_SETSIZE
    for (int i = 0; i < FD_SETSIZE; i++)
    {
      // check to see if that fd is SET -> if set, there is data to read
      if (FD_ISSET(i, &copySet))
      {
        // 1st case: NEW CONNECTION
        if (i == serverSocket)
        {
          int client_sd = accept(serverSocket, 0, 0); // accept new connection
          FD_SET(client_sd, &masterSet);
          printf("New connection at: %d\n", client_sd);
          listOfConnectedClients[client_sd].userName = false;
          listOfConnectedClients[client_sd].userPass = false;
          strcpy(listOfConnectedClients[client_sd].currDir, ".");
        }
        // 2nd case: read data
        else
        {
          char buffer[256];
          bzero(buffer, sizeof(buffer));
          int bytes = recv(i, buffer, sizeof(buffer), 0); // receive
          printf("Message at: %d: %s\n", i, buffer);
          if (bytes == 0) // client has closed the connection
          {
            printf("Connection closed from client: %d. \n", i);
            close(i);              // we are done, close fd
            FD_CLR(i, &masterSet); // remove the socket from the list of file descriptors that we are watching
            // reset
            listOfConnectedClients[i].userName = false;
            listOfConnectedClients[i].userPass = false;
          }
          else
          {
            // TODO: stuff

            int responseSize = strlen(buffer), strindx = 0;
            bool secondStr = false;
            // break into command and data
            char resCmd[256];
            char resDat[256];
            for (int j = 0; j <= responseSize; j++)
            {
              if (buffer[j] == ' ')
              {
                // command string e.g. "USER", "PASS", "STOR", etc.
                secondStr = true;
                resCmd[strindx] = '\0';
                strindx = 0;
              }
              else
              {
                // data string (if any)
                if (!secondStr)
                {
                  resCmd[strindx] = buffer[j];
                }
                else
                {
                  resDat[strindx] = buffer[j];
                }
                strindx++;
              }
            }

            /*
            if (resCmd[0] == '\0' || resDat[0] == '\0') {
              printf("Error in command.\n");
            }
            */
            char allCmds[5][5] = {"USER", "PASS", "LIST", "PORT", "RETR"};

            // USER command
            if (strcmp(resCmd, allCmds[0]) == 0)
            {
              bool foundDat = false;
              for (int n = 0; n < userCount; n++)
              {
                if (strcmp(resDat, accFile[n].user) == 0)
                {
                  foundDat = true;
                  listOfConnectedClients[i].userIndex = n; // found at nth pos in array
                  listOfConnectedClients[i].userName = true;
                  char corResponse[] = "331 Username OK, need password.";
                  send(i, corResponse, sizeof(corResponse), 0);
                  break;
                }
              }
              if (!foundDat)
              {
                char corResponse[] = "530 Not logged in.";
                send(i, corResponse, sizeof(corResponse), 0);
              }
            }

            // PASS command
            else if (strcmp(resCmd, allCmds[1]) == 0)
            {
              if (!listOfConnectedClients[i].userName)
              {
                char corResponse[] = "530 Not logged in.";
                send(i, corResponse, sizeof(corResponse), 0);
              }
              else
              {
                if (strcmp(resDat, accFile[listOfConnectedClients[i].userIndex].pw) == 0)
                {
                  char corResponse[] = "230 User logged in, proceed.";
                  listOfConnectedClients[i].userPass = true;
                  send(i, corResponse, sizeof(corResponse), 0);
                }
                else
                {
                  char corResponse[] = "530 Not logged in.";
                  send(i, corResponse, sizeof(corResponse), 0);
                }
              }
            }

            else if (!listOfConnectedClients[i].userPass || !listOfConnectedClients[i].userName)
            {
              char corResponse[] = "530";
              send(i, corResponse, sizeof(corResponse), 0);
              memset(resCmd, 0, strlen(resCmd));
              memset(resDat, 0, strlen(resDat));
            }
            // LIST command
            else if (strcmp(resCmd, allCmds[2]) == 0 || strcmp(resCmd, allCmds[4]) == 0)
            {
              // Todo: list all the file directories in current active directory
              int pid = fork();
              if (pid == 0)
              { // child process
                close(i);
                // TODO: reset FD set
                // create a socket
                int network_socket = socket(AF_INET, SOCK_STREAM, 0);

                // check for fail error
                if (network_socket == -1)
                {
                  printf("socket creation failed..\n");
                  exit(EXIT_FAILURE);
                }

                // setsock
                int value = 1;
                setsockopt(network_socket, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)); //&(int){1},sizeof(int)

                struct sockaddr_in clientAddrToSendData, thisMachineAddr;
                bzero(&clientAddrToSendData, sizeof(clientAddrToSendData));
                clientAddrToSendData.sin_family = AF_INET;
                clientAddrToSendData.sin_port = htons(listOfConnectedClients[i].userCurDataPort);
                clientAddrToSendData.sin_addr.s_addr = INADDR_ANY; // TODO:

                // bind it to port 20

                bzero(&thisMachineAddr, sizeof(thisMachineAddr));
                thisMachineAddr.sin_family = AF_INET;
                thisMachineAddr.sin_port = htons(20);
                thisMachineAddr.sin_addr.s_addr = INADDR_ANY; // TODO:
                if (bind(network_socket, (struct sockaddr *)&thisMachineAddr, sizeof(struct sockaddr_in)) == 0)
                {
                  printf("binded \n");
                }

                // connect
                int connection_status =
                    connect(network_socket,
                            (struct sockaddr *)&clientAddrToSendData,
                            sizeof(clientAddrToSendData));
                printf("netsock: %d\n", network_socket);

                // check for errors with the connection
                if (connection_status == -1)
                {
                  printf("There was an error making a connection to the remote socket \n\n");
                  exit(EXIT_FAILURE);
                }
                else
                {
                  // Todo: maybe this response is automated
                  char corResponse[] = "150 File status okay; about to open. data connection.";
                  send(network_socket, corResponse, sizeof(corResponse), 0);

                  // LIST command
                  if (strcmp(resCmd, allCmds[2]) == 0)
                  {
                    struct dirent *drty;
                    DIR *direct;
                    direct = opendir(".");
                    if (direct)
                    {
                      char buffer2[256];
                      while ((drty = readdir(direct)) != NULL)
                      {
                        // dummy read

                        bzero(buffer2, sizeof(buffer2));
                        recv(network_socket, &buffer2, sizeof(buffer2), 0); // receive dummy

                        // send
                        send(network_socket, drty->d_name, sizeof(drty->d_name), 0);
                      }
                      closedir(direct);
                    }
                  }
                  // RETR command
                  else if (strcmp(resCmd, allCmds[4]) == 0)
                  {
                    // char dum[256];
                    //  recv(network_socket, &dum, sizeof(dum), 0);
                    char filename[256];
                    int i2 = 0;
                    int j2 = 0;
                    while (listOfConnectedClients[i].currDir[i2] != '\0')
                    {
                      filename[j2] = listOfConnectedClients[i].currDir[i2];
                      i2++;
                      j2++;
                    }
                    filename[j2] = '/';
                    j2++;
                    i2 = 0;
                    while (resDat[i2] != '\0')
                    {
                      filename[j2] = resDat[i2];
                      i2++;
                      j2++;
                    }
                    filename[j2] = '\0';
                    // send the file
                    //  text file
                    // TODO: check

                    FILE *txtile = fopen(filename, "r");
                    if (txtile != 0)
                    {
                      printf("open");
                      sFile(txtile, network_socket);
                    }
                    else
                    {
                      char errormsg[] = "550 No such file or directory";
                      send(network_socket, errormsg, strlen(errormsg), 0);
                    }
                    // binary file
                  }
                  // start sending the list command
                  close(network_socket);
                  exit(1);
                }
              }
            }

            // PORT command
            else if (strcmp(resCmd, allCmds[3]) == 0)
            {
              // Todo: list all the file directories in current active directory
              char corResponse[] = "200 PORT command successful.";
              listOfConnectedClients[i].userCurDataPort = atoi(resDat);
              send(i, corResponse, sizeof(corResponse), 0);
            }

            // Wrong command
            else
            {
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
