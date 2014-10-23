#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/errno.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define	QLEN		  32	/* maximum connection queue length	*/
#define	BUFSIZE		4096
#define MAX_STRING_LENGTH 80
#define TABLE_SIZE 20

extern int	errno;

int errexit(const char *format, ...);
void getUDPMessage(int port);
void handleClientCommand(char *cmd, char* reply);
void start(char *arg, char *returnMesg);
void find(char *arg, char *returnMesg);
int getListeningSock(int tableEntry, in_addr_t host);
void acceptClients(int msock);
int readPrint(int sock);

struct seshInfo {
	char name[MAX_STRING_LENGTH];
	in_port_t port;
	in_addr_t addr;
};

struct seshInfo SESH[TABLE_SIZE];
//int argc, char *argv[]
int main() {
	int port = 5003;

	getUDPMessage(port);
	
	return 0;
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
void handleClientCommand(char *message, char* reply) {
	// Split by spaces. Work on duplicate message so strtok doesn't delete info.
	char *buffer = strdup(message);
//	printf("message before tokens: %s\n", buffer);
	char *command = strtok(buffer, " ");
	char *argument = strtok(NULL, " ");
	
//	printf("command: %s, argument: %s message: %s\n", command, argument, message);

	if (strcasecmp(command, "start") == 0) {
		start(argument, reply);
	} else if(strcasecmp(command, "join") == 0) {
	//	printf("recieved join command\n");
		find(argument, reply);
	} else {
		strcpy(reply,"-1");
	}
	free(buffer);
}

void find(char *arg, char *returnMesg) {
	int i;
//	printf("looking for chat session: %s\n", arg);
	for (i = 0; i < TABLE_SIZE; ++i) {
		if(strcmp(arg, SESH[i].name) == 0) {
	//		printf("Found the chat session\n");
			snprintf(returnMesg, sizeof(returnMesg), "%d", SESH[i].port);
			return;
		}
	}
	// It must not exist, return -1
	strcpy(returnMesg, "-1");
}

void start(char *arg, char *returnMesg) {
	int i = 0, child;
	// Check for duplicates.
	for (i = 0; i < TABLE_SIZE; ++i) {
		if(strcmp(arg,SESH[i].name) == 0) {
			strcpy(returnMesg, "-1");
			return;
		}
	}
	i = 0;
	// Look for an empty spot in the table
	while ((i < TABLE_SIZE) && (strcmp("",SESH[i].name) != 0)) {
		i++;
	} 
	// Guaruntee we are in the table. Or error out.
	if (i >= TABLE_SIZE) {
		strcpy(returnMesg, "-1");
		return;
	} 
	// If we got here then we found a unique name and an empty slot
	strcpy(SESH[i].name, arg);
	//printf("Just tried to copy arg = %s. Sesh[%d] = %s\n", arg, i, SESH[i].name);
	// Start a listening tcp socket on any port and launch a session server.
	int tsock = getListeningSock(i, SESH[i].addr);
	// Build return message.
	snprintf(returnMesg, sizeof(returnMesg), "%d", SESH[i].port);
	
	//---------Assuming client recieves the addr and port successfully ---------------
	
	//acceptClients(tsock);

	printf("Parent sock is %d\n", tsock);
	char str_fd[1];
	str_fd[0] = tsock;
	if ((child = fork()) == -1) {
		fprintf(stderr, "Error forking child\n");
	} else if(child == 0) {
		execl("chat_server", "chat_server", str_fd, NULL);
	}
}

int getListeningSock(int tableEntry, in_addr_t host) {
	int msock;
	struct sockaddr_in myAddr;
	int sockOption = 1;

	if ((msock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		fprintf(stderr, "fucked up\n");
		exit(1);
	}

	setsockopt(msock, SOL_SOCKET, SO_REUSEADDR, &sockOption, sizeof(int));

	memset((char *)&myAddr, 0, sizeof(myAddr));
	myAddr.sin_family = AF_INET;
	myAddr.sin_port = htons(0); // ANY PORT
	myAddr.sin_addr.s_addr = htonl(host);
	if(bind(msock, (struct sockaddr *)&myAddr, sizeof(myAddr)) < 0) {
		fprintf(stderr, "fucked up bind\n");
	}

	if(listen(msock, QLEN) < 0) {
		fprintf(stderr, "listen not working\n");
	}
	
	
	// get port num
	socklen_t len = sizeof(myAddr);
	getsockname(msock, (struct sockaddr *)&myAddr, &len);
	
	// get host ip
	// char ipAddr[MAX_STRING_LENGTH];
	// inet_ntop(myAddr.sin_family, (struct sockaddr *)&myAddr, ipAddr, sizeof(ipAddr));
	// printf("ipAddr=%s, port=%d\n", ipAddr, ntohs(myAddr.sin_port));

	// record in the table
	// strcpy(SESH[tableEntry].port, myAddr.sin_port);
	// strcpy(SESH[tableEntry].addr, myAddr.sin_addr.s_addr);
	SESH[tableEntry].port = myAddr.sin_port;
	SESH[tableEntry].addr = myAddr.sin_addr.s_addr;

	//printf("Table entry name=%s addr=%d port%d\n", 
		//SESH[tableEntry].name, SESH[tableEntry].addr, SESH[tableEntry].port);
	return msock;
}

void getUDPMessage(int port) {
	int s, n;
	struct sockaddr_in myAddr, theirAddr;
	socklen_t len;
	char message[BUFSIZE];
	char reply[BUFSIZE];

	// Allocate a socket
	s = socket(AF_INET,SOCK_DGRAM,0);
	if(s < 0) {
		errexit("Can't create socket: %s\n", strerror(errno));
	}

	memset((char *)&myAddr, 0, sizeof(myAddr));
	myAddr.sin_family = AF_INET;
	myAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	myAddr.sin_port = htons(port);
	
	if(bind(s ,(struct sockaddr *)&myAddr,sizeof(myAddr))) {
		errexit("Problem binding to port %s: %s", ntohs(myAddr.sin_port), strerror(errno));
	}
	
	len = sizeof(theirAddr);
	printf("Listening on port: %d\n", ntohs(myAddr.sin_port));
	while(1) {
		bzero(message, sizeof(message));
		bzero(reply, sizeof(reply));
		n = recvfrom(s , message, BUFSIZE, 0, (struct sockaddr *)&theirAddr, &len);
		handleClientCommand(message, reply);
	//	printf("Got message: %s Trying to send reply: %s\n", message, reply);
		sendto(s, reply, sizeof(reply), 0, (struct sockaddr *)&theirAddr, sizeof(theirAddr));
	}
	
	
	//printf("-------------------------------------------------------\n");

	close(s);
}

 

int readPrint(int sock) {
	char message[BUFSIZE];
	int cc;
	// Zero out any garbage in the buffer.
	printf("in the readprint func.\n");
	bzero(message, sizeof(message));
	cc = read(sock, message, sizeof(message));

	if(cc < 0) {
		fprintf(stderr, "%s\n", "Something messed up in read");
	}
	if(cc && write(sock, message, cc) < 0){
		fprintf(stderr, "%s\n", "Something messed up in write");
	}
	fputs(message, stdout);
	return cc;
}

