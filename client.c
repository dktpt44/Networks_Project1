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

unsigned short controlPort;
bool userAuthenticated = false;

int getNewPort()
{
  return controlPort++;
}

int main()
{
  // create a socket
  int network_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (network_socket == -1)
  { // check for fail error
    printf("socket creation failed..\n");
    exit(EXIT_FAILURE);
  }
  // setsock
  int value = 1;
  setsockopt(network_socket, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)); //&(int){1},sizeof(int)

  struct sockaddr_in server_address;
  bzero(&server_address, sizeof(server_address));
  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(PORT);
  server_address.sin_addr.s_addr = INADDR_ANY;

  // connect
  if (connect(network_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
  {
    perror("connect");
    exit(EXIT_FAILURE);
  }
  else
  {
    printf("220 Service ready for new user.\n");
  }

  /* for client address */
  struct sockaddr_in clientSocket;
  socklen_t clientSocketSz = sizeof(clientSocket);
  getsockname(network_socket, (struct sockaddr *)&clientSocket, &clientSocketSz);
  // getting client port number
  controlPort = ntohs(clientSocket.sin_port);

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
      // parse command

      // sockfd = network_socket
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

      char allCmds[3][5] = {"RETR", "STOR", "LIST"};

      // check if any of the LIST, RETR or STOR commands are input by user
      if ((strcmp(resCmd, allCmds[0]) == 0 || strcmp(resCmd, allCmds[1]) == 0 || strcmp(resCmd, allCmds[2]) == 0) && userAuthenticated)
      {
        // send port command first to the server
        char portCmd[256] = "PORT ";

        char portNoString[256];
        bzero(portNoString, sizeof(portNoString));

        int newPortForData = getNewPort();

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

        // TODO: convert port to

        /* send the port command to the server */

        if (send(network_socket, portCmd, strlen(portCmd), 0) < 0)
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
          int pid = fork(); // fork a child process
          if (pid == 0)     // if it is the child process
          {
            // new TCP connection and listen to the port

            close(network_socket); // close the copy of master socket in child process
            int newSocket = socket(AF_INET, SOCK_STREAM, 0);
            // check for fail error
            if (newSocket == -1)
            {
              printf("socket creation failed..\n");
              exit(EXIT_FAILURE);
            }

            // setsock
            int value = 1;
            setsockopt(newSocket, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)); //&(int){1},sizeof(int)

            // define server address structure
            struct sockaddr_in transferAddress;
            bzero(&transferAddress, sizeof(transferAddress));
            transferAddress.sin_family = AF_INET;
            transferAddress.sin_port = htons(newPortForData);
            transferAddress.sin_addr.s_addr = INADDR_ANY;

            // bind the socket to our specified IP and port
            if (bind(newSocket,
                     (struct sockaddr *)&transferAddress,
                     sizeof(transferAddress)) < 0)
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

            int client_socket = accept(newSocket, 0, 0);

            char buffer2[256];
            bzero(buffer2, sizeof(buffer2));
            recv(client_socket, &buffer2, sizeof(buffer2), 0); // receive

            printf("%s\n", buffer2);
            // start receiving data

            char dummy[] = "dummy";

            int endWhile = 1;

            while (endWhile != 0)
            {
              send(client_socket, dummy, sizeof(dummy), 0); // send
              bzero(buffer2, sizeof(buffer2));
              endWhile = recv(client_socket, &buffer2, sizeof(buffer2), 0);
              printf("%s\n", buffer2);
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
        bzero(portCmd, sizeof(portCmd));
      }
      else
      {
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
          // print out whether user name exists or not
          printf("%s\n", response);
          // TODO: Do stuff based on response
          char passCorrect[] = "230 User logged in, proceed.";
          if (strcmp(response, passCorrect) == 0)
            userAuthenticated = true;
        }
      }
    }

    bzero(buffer, sizeof(buffer));
  }

  return 0;
}
