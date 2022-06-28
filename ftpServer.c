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
#define USERMAX 1024
#define SIZE 1024
#define BUFFERSIZE 256

// structure for the connected client data
struct ClientStruct
{
  int userIndex;
  int userCurDataPort;
  bool userName;
  bool userPass;
  char currDir[256];
  char *clientIPAddr;
};

// structure to store username and password
struct acc
{
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
bool isAuthenticated(int i)
{
  if (!listOfConnectedClients[i].userPass || !listOfConnectedClients[i].userName)
  {
    char corResponse[256] = "530 not authenticated.";
    send(i, corResponse, sizeof(corResponse), 0);
    return false;
  }
  return true;
}

// function to read users from file
void loadUserFile()
{
  FILE *userFile = fopen("user.txt", "r");
  int strCount = 0;
  char str = ' ';
  // get the number of users
  while (!feof(userFile))
  {
    str = fgetc(userFile);
    if (str == '\n')
    {
      userCount += 1;
    }
  }
  rewind(userFile);
  // store in the variable
  while (strCount < userCount + 1)
  {
    fscanf(userFile, "%s %s", accFile[strCount].user, accFile[strCount].pw);
    strCount += 1;
  }
}

// function to intiate TCP listen
int initiateTcp()
{
  int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
  if (serverSocket < 0)
  { // check for fail error
    perror("socket:");
    exit(EXIT_FAILURE);
  }
  int value = 1;
  setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)); //&(int){1},sizeof(int)

  struct sockaddr_in servAddr; // define server address structure
  bzero(&servAddr, sizeof(servAddr));
  servAddr.sin_family = AF_INET;
  servAddr.sin_port = htons(PORT);
  servAddr.sin_addr.s_addr = INADDR_ANY;
  if (bind(serverSocket, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0)
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
    printf("Server is listening at port: %d, ip: %s\n", ntohs(servAddr.sin_port), inet_ntoa(servAddr.sin_addr));
  }
  return serverSocket;
}

// function to break a string to two strings
int sepCmdDat(char *buff, char *cmdstr, char *datstr)
{
  int responseSize = strlen(buff), strindx = 0;
  bool secondStr = false;
  int variableCount = 1;
  // break into command and data

  for (int j = 0; j <= responseSize; j++)
  {
    if (buff[j] == ' ')
    {
      // command string e.g. "USER", "PASS", "STOR", etc.
      secondStr = true;
      cmdstr[strindx] = '\0';
      strindx = 0;
      variableCount++;
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
  return variableCount;
}

// USER command, i = socket
void ftpUserCmd(int i, char *resDat)
{
  /*
if (resDat[0] == '\0') {
  printf("Error in command.\n");
}
*/
  bool foundDat = false;
  for (int n = 0; n < userCount; n++)
  {
    if (strcmp(resDat, accFile[n].user) == 0)
    {
      foundDat = true;
      listOfConnectedClients[i].userIndex = n; // found at nth pos in array
      listOfConnectedClients[i].userName = true;
      char corResponse[256] = "331 Username OK, need password.";
      send(i, corResponse, sizeof(corResponse), 0);
      break;
    }
  }
  if (!foundDat)
  {
    char corResponse[256] = "530 Not logged in.";
    send(i, corResponse, sizeof(corResponse), 0);
  }
}

// PASS command, i = socket
void ftpPassCmd(int i, char *resDat)
{
  if (!listOfConnectedClients[i].userName)
  {
    char corResponse[256] = "530 Not logged in.";
    send(i, corResponse, sizeof(corResponse), 0);
  }
  else
  {
    if (strcmp(resDat, accFile[listOfConnectedClients[i].userIndex].pw) == 0)
    {
      char corResponse[256] = "230 User logged in, proceed.";
      listOfConnectedClients[i].userPass = true;
      send(i, corResponse, sizeof(corResponse), 0);
    }
    else
    {
      char corResponse[256] = "530 Not logged in.";
      send(i, corResponse, sizeof(corResponse), 0);
    }
  }
}

// LIST command
void ftpListCmd(int i)
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
      recv(i, &buffer2, sizeof(buffer2), 0); // receive dummy

      // send
      send(i, drty->d_name, sizeof(drty->d_name), 0);
    }
    closedir(direct);
  }
}

// function to parse port command and ip address
void ftpPortCmd(int i, char *resDat)
{
  int ix = 0, p1x = 0, p2x = 0;
  int commaindx = 0;
  char ipAdr[256];
  char p1[256];
  char p2[256];

  int ipIndx = 0;
  // parse port and ip
  while (resDat[ix] != '\0')
  {
    if (resDat[ix] == ',')
    {
      // add . instead of ,
      commaindx++;
      if (commaindx < 4)
      {
        ipAdr[ipIndx] = '.';
      }
      ipIndx++;
    }
    else if (commaindx < 4)
    {
      // extract ip
      ipAdr[ipIndx] = resDat[ix];
      ipIndx++;
    }
    else if (commaindx == 4)
    {
      // extract p1 info
      ipAdr[ipIndx] = '\0';
      p1[p1x] = resDat[ix];
      p1x++;
    }
    else if (commaindx == 5)
    {
      // extract p2 info
      p1[p1x] = '\0';
      p2[p2x] = resDat[ix];
      p2x++;
    }
    ix++;
  }
  p2[p2x] = '\0';
  int p1i, p2i;
  sscanf(p1, "%d", &p1i);
  sscanf(p2, "%d", &p2i);

  listOfConnectedClients[i].userCurDataPort = p1i * 256 + p2i;
  listOfConnectedClients[i].clientIPAddr = ipAdr;

  // do stuff
  char corResponse[256] = "200 PORT command successful.";
  send(i, corResponse, sizeof(corResponse), 0);
}

// function to initiate second TCP channel
int initiateDataChannel()
{
  int newDataSock = socket(AF_INET, SOCK_STREAM, 0);
  if (newDataSock == -1)
  {
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
  thisMachineAddr.sin_addr.s_addr = INADDR_ANY; // TODO:
  if (bind(newDataSock, (struct sockaddr *)&thisMachineAddr, sizeof(struct sockaddr_in)) == 0)
  {
    printf("binded \n");
  }
  return newDataSock;
}

// function to send file over socket i
void sFile(char *filename, int i)
{
  FILE *fp = fopen(filename, "rb");
  if (fp != NULL)
  {
    printf("File opened.\n");

    // send file size first
    fseek(fp, 0L, SEEK_END);
    // calculating the size of the file
    long int res = ftell(fp);
    char filesizemsg[256];
    printf("\nsz:%ld\n", res);
    sprintf(filesizemsg, "%ld", res);
    send(i, filesizemsg, sizeof(filesizemsg), 0);

    // read dummy
    // char dumx[256];
    // recv(i, dumx, sizeof(dumx))
  }
  else
  {
    char errormsg[256] = "550 No such file or directory";
    send(i, errormsg, sizeof(errormsg), 0);
  }

  char data[256];
  bzero(data, 256);

  // initially send the size of the file
  fseek(fp, 0L, SEEK_SET);
  while (true)
  {
    int readLen = fread(data, 1, sizeof(data), fp);
    if (readLen <= 0)
    {
      break;
    }
    // printf("-:%s\n", data);
    if (send(i, data, readLen, 0) == -1)
    {
      perror("--Error in sending file.");
      exit(1);
    }
    bzero(data, sizeof(data));
  }

  fclose(fp);

  char complete[256] = "226 Transfer completed.";
  send(i, complete, sizeof(complete), 0);
}

// main function
int main()
{
  // reading from the file
  char initDir[BUFFERSIZE];
  getcwd(initDir, BUFFERSIZE);
  loadUserFile();
  int serverSocket = initiateTcp();

  fd_set masterSet;
  FD_ZERO(&masterSet);              // zero out/iniitalize our set of all sockets
  FD_SET(serverSocket, &masterSet); // adds  current socket to the fd set

  while (true)
  {
    fd_set copySet = masterSet; // making a temporary copy
    // The max no. of sockets supported by select() = FD_SETSIZE (typically 1024).
    if (select(FD_SETSIZE, &copySet, NULL, NULL, NULL) < 0)
    {
      perror("select error");
      exit(EXIT_FAILURE);
    }

    // looping through fdset to check which ones are ready for reading
    for (int i = 0; i < FD_SETSIZE; i++)
    {
      // check to see if that fd is SET -> if set, there is data to read
      if (FD_ISSET(i, &copySet))
      {
        // 1st case: NEW CONNECTION
        if (i == serverSocket)
        {
          struct sockaddr_in clientAddrs;
          socklen_t addr_size;
          // accept new connection
          int newClientSock = accept(serverSocket, (struct sockaddr *)&clientAddrs, &addr_size);
          FD_SET(newClientSock, &masterSet);
          printf("+New connection at socket: %d, ip: %s, port: %d\n", newClientSock, inet_ntoa(clientAddrs.sin_addr), ntohs(clientAddrs.sin_port));
          listOfConnectedClients[newClientSock].userName = false;
          listOfConnectedClients[newClientSock].userPass = false;
          strcpy(listOfConnectedClients[newClientSock].currDir, initDir);
        }

        // 2nd case: read data
        else
        {
          char buffer[256];
          bzero(buffer, sizeof(buffer));
          int bytes = recv(i, buffer, sizeof(buffer), 0); // receive the first command
          printf("Message at: %d> %s\n", i, buffer);

          // case1: client has closed the connection
          if (bytes == 0)
          {
            printf("-Connection closed from client socket: %d. \n", i);
            close(i);              // we are done, close fd
            FD_CLR(i, &masterSet); // remove the socket from the fd set
            // reset
            listOfConnectedClients[i].userName = false;
            listOfConnectedClients[i].userPass = false;
          }
          // case2: received some data from client

          else
          {
            char resCmd[256];
            char resDat[256];
            int seqCheck = sepCmdDat(buffer, resCmd, resDat);

            char allCmds[8][5] = {"USER", "PASS", "PORT", "LIST", "RETR", "STOR", "PWD", "CWD"};
            chdir(listOfConnectedClients[i].currDir);
            // USER command
            if (strcmp(resCmd, allCmds[0]) == 0)
            {
              ftpUserCmd(i, resDat);
            }

            // PASS command
            else if (strcmp(resCmd, allCmds[1]) == 0)
            {
              ftpPassCmd(i, resDat);
            }

            // CHECK if authetiated or not

            // PORT command
            else if (strcmp(resCmd, allCmds[2]) == 0)
            {
              if (isAuthenticated(i))
              {
                ftpPortCmd(i, resDat);
              }
            }

            // LIST, RETR, STOR = fork a new process
            else if (strcmp(resCmd, allCmds[3]) == 0 || strcmp(resCmd, allCmds[4]) == 0)
            {
              if (isAuthenticated(i))
              {

                int pid = fork();
                // child process
                if (pid == 0)
                {
                  // TODO: do we need to reset FD set? Ask Shan.

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
                  if (connection_status == -1)
                  {
                    printf("There was an error making a connection to the remote socket \n\n");
                    exit(EXIT_FAILURE);
                  }
                  else
                  {
                    // Todo: maybe this response is automated
                    char corResponse[256] = "150 File status okay; about to open. data connection.";
                    send(newDataSock, corResponse, sizeof(corResponse), 0);

                    // LIST command
                    if (strcmp(resCmd, allCmds[3]) == 0)
                      ftpListCmd(newDataSock);

                    // RETR command
                    else if (strcmp(resCmd, allCmds[4]) == 0)
                    {
                      printf("+Sending data, using socket: %d, to client port: %d\n", newDataSock, listOfConnectedClients[i].userCurDataPort);

                      // char dum[256];
                      // recv(newDataSock, &dum, sizeof(dum), 0);
                      // printf("%s\n", dum);
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

                      // TODO: check

                      sFile(filename, newDataSock);
                    }

                    printf("-Sending done, closing sock: %d, to client port: %d\n", newDataSock, listOfConnectedClients[i].userCurDataPort);

                    close(newDataSock);
                    exit(1);
                  }
                }
              }
            }

            // PWD
            else if (strcmp(resCmd, allCmds[6]) == 0)
            {
              if (seqCheck > 1)
              {
                char invalid[] = "503 Bad sequence of commands.";
                send(i, invalid, sizeof(invalid), 0);
              }
              else if (isAuthenticated(i))
              {
                char succMsg[] = "257 pathname ";
                char returnMsg[BUFFERSIZE];
                int j2 = 0;
                int i2 = 0;
                while (succMsg[j2] != '\0')
                {
                  returnMsg[i2] = succMsg[j2];
                  i2++;
                  j2++;
                }
                j2 = 0;
                while (listOfConnectedClients[i].currDir[j2] != '\0')
                {
                  returnMsg[i2] = listOfConnectedClients[i].currDir[j2];
                  i2++;
                  j2++;
                }
                send(i, returnMsg, sizeof(returnMsg), 0);
                bzero(returnMsg, sizeof(returnMsg));
              }
            }

            // CWD command
            else if (strcmp(resCmd, allCmds[7]) == 0)
            {
              if (isAuthenticated(i))
              {
                char newDir[BUFFERSIZE];
                char responseMsg[BUFFERSIZE];
                if (resDat[0] == '.')
                {
                  strcpy(newDir, resDat);
                }
                else if (resDat[0] != '/')
                { // if resDat doesn't starts with '/' this means that this input is a folder
                  printf("r");
                  int e2 = 0;
                  int r2 = 0;
                  while (listOfConnectedClients[i].currDir[e2] != '\0')
                  {
                    newDir[r2] = listOfConnectedClients[i].currDir[e2];
                    r2++;
                    e2++;
                  }
                  e2 = 0;
                  newDir[r2] = '/';
                  r2++;
                  while (resDat[e2] != '\0')
                  {
                    newDir[r2] = resDat[e2];
                    r2++;
                    e2++;
                  }
                }
                else
                {
                  strcpy(newDir, resDat);
                }
                if (chdir(newDir) == -1)
                {
                  strcpy(responseMsg, "550 No such directory.\n");
                }
                else
                {
                  bzero(newDir, sizeof(newDir));
                  getcwd(newDir, sizeof(newDir));
                  strcpy(listOfConnectedClients[i].currDir, newDir);
                  strcpy(responseMsg, "200 directory changed to pathname/foldername.");
                }
                send(i, responseMsg, sizeof(responseMsg), 0);
                bzero(responseMsg, sizeof(responseMsg));
                bzero(newDir, sizeof(newDir));
              }
              // Wrong command
            }
            else
            {
              char corResponse[256];
              if (seqCheck > 2)
              {
                strcpy(corResponse, "503 Bad sequence of commands.");
              }
              else
              {
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
