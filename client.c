#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <unistd.h>
#include <stdlib.h>

#define PORT 9007


int main()
{
	bool userA = false;
	bool pwA = false;
	//create a socket
	int network_socket;
	network_socket = socket(AF_INET , SOCK_STREAM, 0);

	//check for fail error
	if (network_socket == -1) {
        printf("socket creation failed..\n");
        exit(EXIT_FAILURE);
    }

	//setsock
	int value  = 1;
	setsockopt(network_socket,SOL_SOCKET,SO_REUSEADDR,&value,sizeof(value)); //&(int){1},sizeof(int)
	
	struct sockaddr_in server_address;
	bzero(&server_address,sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(PORT);
	server_address.sin_addr.s_addr = INADDR_ANY;

	//connect
    if(connect(network_socket,(struct sockaddr*)&server_address,sizeof(server_address))<0)
    {
        perror("connect");
        exit(EXIT_FAILURE);
    } else {
    	printf("220 Service ready for new user.");
    }
	
	char buffer[256];
		// client authorization


	while(1)
	{
		printf("ftp> ");


		//get input from user
       fgets(buffer,sizeof(buffer),stdin);
       buffer[strcspn(buffer, "\n")] = 0;  //remove trailing newline char from buffer, fgets does not remove it
       if(strcmp(buffer,"exit")==0)
        {
        	printf("closing the connection to server \n");
        	close(network_socket);
            break;
        }
        
        if(send(network_socket,buffer,strlen(buffer),0)<0)
        {
            perror("send");
            exit(EXIT_FAILURE);
        }else {
        		// get data back
        	char response[1024];
	        recv(network_socket , &response, sizeof(response),0);
	        //print out whether user name exists or not
	        printf(response);

	    }
        
        bzero(buffer,sizeof(buffer));			
	}

	return 0;
}
