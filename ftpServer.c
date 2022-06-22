#include <arpa/inet.h>
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

// list of connected clients
struct ClientStruct {
  bool userName;
  bool userPass;
};

struct acc {  // structure to store username and password
  char user[256];
  char pw[256];
};

struct ClientStruct listOfConnectedClients[FD_SETSIZE];
// first char = username correct?
// second char = password correct?

int main() {
  // reading from the file
  static struct acc accFile[USERMAX];
  FILE *userFile = fopen("user.txt", "r");
  int userCount = 0;
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

  for (int i = 0; i < 2; i++) {
    printf("%s %s", accFile[i].user, accFile[i].pw);
    printf("\n");
  }

  // for initial TCP connection

  int serverSocket = socket(AF_INET, SOCK_STREAM, 0);

  // check for fail error
  if (serverSocket < 0) {
    perror("socket:");
    exit(EXIT_FAILURE);
  }

  // setsock
  int value = 1;
  setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));  //&(int){1},sizeof(int)

  // define server address structure
  struct sockaddr_in server_address;
  bzero(&server_address, sizeof(server_address));
  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(PORT);
  server_address.sin_addr.s_addr = INADDR_ANY;

  // bind
  if (bind(serverSocket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }

  // listen
  if (listen(serverSocket, 5) < 0) {
    perror("listen failed");
    close(serverSocket);
    exit(EXIT_FAILURE);
  }

  fd_set masterSet;                  // DECLARE fd set (file descriptor sets : a collection of file descriptors)
  FD_ZERO(&masterSet);               // zero out/iniitalize our set of all sockets
  FD_SET(serverSocket, &masterSet);  // adds one socket (the current socket) to the fd set of all sockets

  printf("Server is listening.\n");

  while (true) {
    // select() is destructive: it's going to change the set we pass in, so we need a temporary copy
    fd_set copySet = masterSet;

    // The maximum number of sockets supported by select() has an upper limit, represented by FD_SETSIZE (typically 1024).
    if (select(FD_SETSIZE, &copySet, NULL, NULL, NULL) < 0) {
      perror("select error");
      exit(EXIT_FAILURE);
    }

    // select returns the fd_set containing JUST the file descriptors ready for reading
    // to know which ones are ready, we have to loop through and check go from 0 to FD_SETSIZE
    for (int i = 0; i < FD_SETSIZE; i++) {
      // check to see if that fd is SET -> if set, there is data to read
      if (FD_ISSET(i, &copySet)) {
        // 1st case: NEW CONNECTION
        if (i == serverSocket) {
          int client_sd = accept(serverSocket, 0, 0);  // accept new connection
          FD_SET(client_sd, &masterSet);
          printf("Connection at: %d\n", client_sd);
          listOfConnectedClients[client_sd].userName = false;
          listOfConnectedClients[client_sd].userPass = false;
          // ToDO: send connection success message

        }

        // 2nd case: when the socket that is ready to read from is one from the all_sockets fd_set -> we just want to read its data
        else {
          printf("message at: %d: \n", i);
          char buffer[256];
          bzero(buffer, sizeof(buffer));
          int bytes = recv(i, buffer, sizeof(buffer), 0);
          if (bytes == 0)  // client has closed the connection
          {
            printf("Connection closed from client side. \n");
            close(i);               // we are done, close fd
            FD_CLR(i, &masterSet);  // remove the socket from the list of file descriptors that we are watching
                                    // reset
            listOfConnectedClients[i].userName = false;
            listOfConnectedClients[i].userPass = false;
          } else {
            // TODO: stuff
            // Parse command

            int responseSize = strlen(buffer), ctr = 0, x = 0;
            char resCommand[2][256];  // 0 = command, 1 = data
            for (int j = 0; j <= responseSize; j++) {
              // if space or NULL found, assign NULL into newString[ctr]
              if (buffer[j] == ' ' || buffer[j] == '\0') {
                resCommand[ctr][x] = '\0';
                ctr++;  // for next word
                x = 0;  // for next word, init index to 0
              } else {
                resCommand[ctr][x] = buffer[j];
                x++;
              }
            }

            printf("hi");
            printf("here1");
            if (0 == 0) {
              printf("ok");
              for (int n = 0; n < userCount; n++) {
                if (resCommand[1] == accFile[n].user) {
                  listOfConnectedClients[i].userName = true;
                  char corResponse[] = "331 Username OK, need password.";
                  send(i, corResponse, sizeof(corResponse), 0);
                  break;
                }
              }
            }
            printf("here");

            // send something back to client
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
