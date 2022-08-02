#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <netdb.h>
//#include <tcp.h>

//define 
#define SERVER_PORT 3300
#define MAXSIZE 4096
#define SA struct socketAddress

/*
void error&die(const char *fmt, ...) //print out errors
{
	int errnoSave;
	va_list ap;
	
	//save errno since all system call can set errno
	errnoSave = errno;
	
	//print out the fmt and args to standard out 
	va_start(ap, fmt);
	vfprintf(stdout, fmt,ap);
	fflush(stdout);
	
	//print out error message if errno was set 
	if(errnoSave != 0)
	{
		fprintf(stdout, "(errno = %d): %s\n", errnoSave, strerror(errnoSave));
		fprintf(stdout, "\n");
		fflush(stdout);
	}
	va_end(ap);
	
	exit(1);
}

//convert to hex representation
char *binToHex(const unsigned char *input, size_t len)
{
	char *result;
	char *hex = "0123456789ABCDEF";
	
	if(input == NULL || len <= 0)
	{
		return NULL;
		int resultLength = (len*3) + 1;
		bzero(result, resultLength);
		
		for (int i=0; i<len; i++)
		{
			result[i*3] = hex[input[i] >4];
			result[(i*3)+1] = hex[input[i] & 0x0F];
			result[(i*3)+1] = ' '; //for readability	
		}
		return result;
	}
		
	
}
*/

int main(int argc, char **argv)
{
	socklen_t clientLength;
	int listenSocketfd, connectSocketfd, readBits;
	struct socketAddressIN serverAddress, clientAddress;
	uint8_t buff[MAXSIZE + 1];
	uint8_t receiveBuffer[MAXSIZE + 1];
	
	//create socket with socket(), and error checking
	if((listenSocketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		fprintf(stderr, "error from socket\n");
	}
	
	//setup the address that for listenSocketfd 
	bzero(&serverAddress, sizeof(serverAddress));
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_addr.s_addr = htonl(INADDR_ANY); //respond to anything
	serverAddress.sin_port = htons(SERVER_PORT);
	
	//bind the listenSocketfd/welcoming socket the above address; do a error checking 
	if((bind(listenSocketfd, (struct socketAddress*) & serverAddress, sizeof(serverAddress))) < 0)
	{
		fprintf(stderr, "error from binding\n");
	}
	//After a socket has been associated with an address, listen() prepares it for incoming connections
	if((listen(listenSocketfd , 1)) < 0)
	{
		fprintf(stderr, "error from listen function\n");
	}
	
	while(1)
	{
		//struct socketAddressIN address;
		//socket_t addressLen;
		
		//accept blocks untill an incoming connection arrives. returns a file descripto to the connection
		printf("waiting for a connection on port %d\n", SERVER_PORT);
		fflush(stdout);
		//It accepts a received incoming attempt to create a new TCP connection from the remote client, 
		//and creates a new socket associated with the socket address pair of this connection.
		if((connectSocketfd = accept(listenSocketfd, (struct socketAddress*) & clientAddress, & clientLength)) < 0)
		{
			fprintf(stderr, "error from connectSocketfd/accept function\n");
		}
		
		memset(receiveBuffer, 0, MAXSIZE); //set out to receive buffer tomake sure it end up null terminated
		
		//read clinent's message
		while((readBits = read(connectSocketfd, receiveBuffer, MAXSIZE-1)) >0)
		{
			fprintf(stdout, "\n%s\n\n%s", binToHex(receiveBuffer, readBits), receiveBuffer);
			
			//hacky way to detect the end of the message
			if(receiveBuffer[readBits-1] == '\r\n'){break;}
			memset(receiveBuffer, 0, MAXSIZE);
		}
		
		//cannot read negative bits. if happens, output error message
		if (readBits<0)
			fprintf(stderr, "read negative bits from connectSocketfd\n");
		
		//send a meaningless response into buff
		sprintf((char*) buff, sizeof(buff), "HTTP/1.0 200 0k \r\n\r\nHello");
		
		//write responce into connection socket and close it
		write(connectSocketfd, (char*)buff, strlen((char*)buff));
		close(connectSocketfd);
	}
	
	
}