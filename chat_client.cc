#include <sys/types.h>
#include <sys/socket.h>

#include <sys/errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <stdarg.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define USAGE "Usage: chatClient <host> <port>"
#define HELP "Available Commands: \nstart <chat session name>\njoin <chat session name>\nsubmit <message>\ngetnext\ngetall\nleave\nexit\n"
#define	LINELEN		128
#define BUFSIZE		4096

extern int	errno;
int chat(const char *host, const char *port);
int	errexit(const char *format, ...);
int udpSocket();
int connectTCP(in_addr_t host, const char *port);
int processInput(char *input);
int start(char *message, char *response, int sock, struct sockaddr_in destAddr);
void submit(int sock, int isChatting, char *message);
void getMesg(int sock, int isChatting, char *message);
void getAllMesg(int sock, int isChatting, char *message);
void sendTCPmesg(int sock, char *message);

int main(int argc, char const *argv[])
{
	if (argc != 3) {
		fprintf(stderr, "%s\n", USAGE);
		exit(1);
	}
	const char *host = argv[1];
	const char *port = argv[2];

	printf("Connecting to chat using %s on port %s\n", host, port);
	chat(host, port);
	
	// tcp stuff

	// int sock = connectTCP(host, port);	

	// while (fgets(message, sizeof(message), stdin)) {
	// 	message[LINELEN] = '\0';
	// 	int w = write(sock, message, sizeof(message));
	// 	if(w < 0) {
	// 		fprintf(stderr, "%s\n", "Write call messed up");
	// 		exit(1);
	// 	}
	// }
	// close(sock);
	return 0;
}

/*
 * chat - prompts the user for commands and executes commands
 */
int chat(const char *host, const char *port) {
 	char	message[LINELEN+1];		/* buffer for one line of text	*/
 	char	response[BUFSIZE];
	int	udpSock;			/* socket descriptor */
	struct sockaddr_in destAddr;
	int tcpSocket;
	int isChatting = 0;

 	// Set up UDP socket and port
 	udpSock = udpSocket();

 	// Set up destination info
 	memset((char *)&destAddr, 0, sizeof(destAddr));
 	destAddr.sin_family = AF_INET;
 	destAddr.sin_port = htons((unsigned short)atoi(port));
 	destAddr.sin_addr.s_addr = inet_addr(host);

 	printf("$ ");
	while (fgets(message, sizeof(message), stdin)) {
		message[LINELEN] = '\0';	/* insure line null-terminated	*/
		
		// Generate number accorrding to command entered.
		int option = processInput(message);

		switch(option) {
			case 0: // Start
				tcpSocket = start(message, response, udpSock, destAddr);
				isChatting = 1;
				break;
			case 1: // Join: same procedure as start client side. Logic on server will differ though.
				tcpSocket = start(message, response, udpSock, destAddr);
				isChatting = 1;
				break;
			case 2: //submit
				submit(tcpSocket, isChatting, message);
				break;
			case 3: // getnext
				getMesg(tcpSocket, isChatting, message);
				break;
			case 4: // getall
				getAllMesg(tcpSocket, isChatting, message);
				break;
			case 5:
				close(tcpSocket);
				isChatting = 0;
				printf("You left your chat session.\n");
				break;
			case 6: // Exit
				close(udpSock);
				exit(0);
			case 7:
				printf("%s\n", HELP);
				break;
			default:
				printf("Invalid command. Type Help for help\n");
		}
		
		//fputs(buf, stdout);
		printf("$ ");
	}
	close(udpSock);
	return 0;
 }

// Necessary commands
 // start <name>; join <name>; submit <message>; getnext; getall; leave; exit;

int processInput(char *input) {
	const char *ops[] = {"start", "join", "submit", "getnext", "getall", "leave", "exit", "help"};
	int len = sizeof(ops) / sizeof(ops[0]);
	int i = 0; 
	int option = -1;
	char *temp = strdup(input);
	// Newline too so command w/o trailing space parses correctly.
	char *cmd = strtok(temp, " \n");
	// printf("input: %s;  cmd: %s; arg: %s; temp: %s\n",input, cmd,arg, temp);
	for (i = 0; i < len; ++i) {
		if(strcasecmp(cmd, ops[i]) == 0) {
			option = i;
		}
	}
	free(temp);
	return option;
}

int start(char *udpMessage, char *udpResponse, int sock, struct sockaddr_in destAddr) {
	struct sockaddr_in servAddr;
	// char	tcpMessage[LINELEN+1];
	socklen_t len = sizeof(servAddr);

	if(sendto(sock, udpMessage, strlen(udpMessage), 0, (struct sockaddr *)&destAddr, sizeof(destAddr)) < 0) {
		fprintf(stderr, "Failed to send UDP message: %s\n", strerror(errno));
	}
	if(recvfrom(sock, udpResponse, BUFSIZE, 0, (struct sockaddr *)&servAddr, &len) < 0) {
		fprintf(stderr, "Failed to receive UDP message: %s\n", strerror(errno));
	}
	// Expected values are <host>:<port> or -1
	if ( (strcmp("-1",udpResponse) == 0) ) {
		fprintf(stderr, "Could not start chat session. Try another name, or try to join it.\n");
		return -1;
	}
	int newsock = connectTCP(destAddr.sin_addr.s_addr, udpResponse);	
	
	return newsock;
}

void submit(int sock, int isChatting, char *message) {
	if(!isChatting) {
		fprintf(stderr, "You are not in a chat session.\n");
		return;
	}

	sendTCPmesg(sock, message);	

	// for (inchars = 0; inchars < outchars; inchars+=r ) {
	// 	r = read(newsock, &tcpMessage[inchars], outchars - inchars);
	// 	if (r < 0) {
	// 		errexit("socket read failed: %s\n",
	// 			strerror(errno));
	// 	}
	// }
	printf("Message Sent.\n");
}

void sendTCPmesg(int sock, char *message) {
	int	outchars;
	message[LINELEN] = '\0';
	outchars = strlen(message);
	int w = write(sock, message, outchars);
	if(w < 0) {
		fprintf(stderr, "%s\n", "Write call messed up");
		return;
	}	
}

void getMesg(int sock, int isChatting, char *message) {
	char serverReply[BUFSIZE];
	if(!isChatting) {
		fprintf(stderr, "You are not in a chat session.\n");
		return;
	}
	char askForNext[LINELEN+1];
	askForNext[LINELEN] = '\0';
	strcpy(askForNext, "getnext");

	write(sock, askForNext, sizeof(askForNext));
	read(sock, serverReply, sizeof(serverReply));
	printf("%s\n", serverReply);
	memset(&message, 0, sizeof(message));

}

void getAllMesg(int sock, int isChatting, char *message) {
	char serverReply[BUFSIZE];
	if(!isChatting) {
		fprintf(stderr, "You are not in a chat session.\n");
		return;
	}
	char askForAll[LINELEN+1];
	askForAll[LINELEN] = '\0';
	strcpy(askForAll, "getall");

	write(sock, askForAll, sizeof(askForAll));	
	read(sock, serverReply, sizeof(serverReply));
	
	if(strcasecmp(serverReply, "gurp") == 0) {

		printf("You are up to date\n");
		return;
	}
	printf("entering infinite loop.%s\n", serverReply);
	while(true) {
		printf("%s", serverReply);

        // Clear buffer
        memset(&serverReply, 0, sizeof(serverReply));

        // Get next reply
        printf("blocking on read\n");
        read(sock, &serverReply, sizeof(serverReply));

        if( strcmp(serverReply, "END") == 0 ){
            // Clear buffer
            printf("END\n");
            memset(&serverReply, 0, sizeof(serverReply));
            break;
        }
	}

	memset(&message, 0, sizeof(message));
}
/*
 * udpSocket - create and bind a udp socket
 */
int udpSocket() {
 	struct sockaddr_in clientAddr;

	int sock = socket(AF_INET,SOCK_DGRAM,0);
	if(sock < 0) {
		errexit("Could not get socket: %s", strerror(errno));
	}

	memset((char *)&clientAddr, 0, sizeof(clientAddr));
	clientAddr.sin_family = AF_INET;
	clientAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	clientAddr.sin_port = htons(0);

	if(bind(sock, (struct sockaddr *)&clientAddr, sizeof(clientAddr)) < 0) {
		errexit("Could not bind: %s", strerror(errno));
	}
	return sock;
 }

/*
 * connectTCP - Esablish a tcp connection to server.
 */
int connectTCP(in_addr_t servHost, const char *port) {
	struct sockaddr_in serverAddr;

	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock < 0) {
		errexit("Could not get socket: %s", strerror(errno));
	}

	memset((char *)&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = (unsigned short) atoi(port);
	serverAddr.sin_addr.s_addr = servHost;
	
	if(connect(sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
		errexit("Failed to connect: %s", strerror(errno));
	}
	return sock;
}


 /*------------------------------------------------------------------------
 * errexit - print an error message and exit
 *------------------------------------------------------------------------
 */
int errexit(const char *format, ...) {
        va_list args;
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
        exit(1);
}






