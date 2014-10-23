#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/errno.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <vector>
#include <map>


#define	BUFSIZE		4096
#define QLEN 	32
#define MAX_CLIENTS 64
#define MAX_TABLE_SIZE 125000
#define LINELEN 80

int handleMesg(int sock);
int errexit(const char *format, ...);
void acceptClients(int fd);
void addClient(int clientID);
void removeClient(int clientID);
void printChats();
void getNext(int clientID);
void getAll(int clientID);
void addMessage(struct chatStruct);
struct chatStruct
{
	int clientNo;
	char message[LINELEN+1];
	int length;
};

typedef std::vector<chatStruct> chatHistoryVect;
typedef std::map<int, std::vector<chatStruct> > clientMessageMap;

clientMessageMap cMap;


int main(int argc, char const *argv[]) {
	// const char *port = "1099";
	// int sock = getListeningSock(port, QLEN);
	int sock = argv[1][0];
	acceptClients(sock);
	return 0;
}

void acceptClients(int msock) {
	int	ssock;
	struct sockaddr_in clientAddr;
	fd_set	rfds;			/* read file descriptor set	*/
	fd_set	afds;			/* active file descriptor set	*/
	unsigned int	alen;		/* from-address length		*/
	int	fd, nfds;
	printf("Tyring to accept clients.\n");
	nfds = getdtablesize();
	FD_ZERO(&afds);
	FD_SET(msock, &afds);

	while (1) {
		memcpy(&rfds, &afds, sizeof(rfds));
		// printf("Blocking on select....\n");
		if (select(nfds, &rfds, (fd_set *)0, (fd_set *)0, (struct timeval *)0) < 0) {
			errexit("select: %s\n", strerror(errno));
		}
		// printf("Right before FD_ISSET......\n");
		if (FD_ISSET(msock, &rfds)) {
			alen = sizeof(clientAddr);
			// printf("Blocking on accept....\n");
			ssock = accept(msock, (struct sockaddr *)&clientAddr, &alen);
			// Identify client by their socket.
			addClient(ssock);
			if (ssock < 0) {
				errexit("accept: %s\n", strerror(errno));
			}
			FD_SET(ssock, &afds);
		}

			// printf("GOING IN FOR LOOP !!!!!!!\n");
		for (fd=0; fd<nfds; ++fd) {
			if (fd != msock && FD_ISSET(fd, &rfds)) {
				if (handleMesg(fd) == 0) {
					(void) close(fd);
					FD_CLR(fd, &afds);
				}
			}
		}
		// printf("LEAVING FOR LOOP !!!!!!!\n");
	}
}

void printChats() {
	// int i;
	// for( i = 0; i < 10; i++) {
	// 	printf("%d %s\n", i, CHATS[i]);
	// }

	printf("printChats got called\n");
	for(clientMessageMap::iterator iter_map = cMap.begin(); iter_map != cMap.end(); ++iter_map ) {

		chatHistoryVect &chatVector = iter_map->second;
		for(chatHistoryVect::iterator iter_vect = chatVector.begin(); iter_vect != chatVector.end(); ++iter_vect)
	    {
	        printf("In Vector: client%d  message=%s\n", iter_vect->clientNo, iter_vect->message);
	    }
	}
}

int handleMesg(int sock) {
	
	char message[BUFSIZE];
	int cc;
	// Zero out any garbage in the buffer.
	bzero(message, sizeof(message));
	cc = read(sock, message, sizeof(message));
	if(cc < 0) {
		fprintf(stderr, "%s\n", "Something messed up in read");
	}

	char *buffer = strdup(message);
	char *command = strtok(buffer, " ");
	char *contents = strtok(NULL, "\n");
	
	printf("Command: %s\n", command);
	// Parse for getNext or getAll
	if(strcasecmp(command, "getnext") == 0) {
		getNext(sock);
		return cc;
	}

	if(strcasecmp(command, "getall") == 0) {
		printf("GOING in to getall\n");
		getAll(sock);
		return cc;
	}

	struct chatStruct newChat;
	newChat.clientNo = sock;
	strcpy(newChat.message, contents);
	newChat.length = strlen(newChat.message);

	printf("NEWCHAT:   number=%d message=%s length=%d\n", newChat.clientNo, newChat.message, newChat.length);

	

	// Store the message.
	addMessage(newChat);

	printChats();

	return cc;
}

/** 
 * Add a message to the history. Only adds messages
 * from other clients so a client doesn't get his own 
 * message when he calls  getnext or getall.
 **/
void addMessage(struct chatStruct newChat) {
	int messageSubmitter = newChat.clientNo;
	
	// Iterate over the map and add chats to all 
	// clients except the one that sent it.
	for(clientMessageMap::iterator iter_map = cMap.begin(); iter_map != cMap.end(); ++iter_map ) {
		int currentKey = iter_map->first;
		if(currentKey != messageSubmitter) {
			// Problem: the struct I'm inserting has
			// the clientNo of the submitter, not the owner of this vector.
			// Change to current owner.
			newChat.clientNo = currentKey;
			iter_map->second.push_back(newChat);
		}
	}	
}

void getNext(int clientID) {
	char sendthis[LINELEN];
	bool sentChat;
	chatHistoryVect chatVector = cMap.find(clientID)->second; 
    chatHistoryVect::iterator iter_vect;
	if(chatVector.size() == 0) {
		strncpy(sendthis, "You are up to date.", sizeof(sendthis));	
		sentChat = false;
	} else {
		iter_vect = chatVector.begin();        
		strncpy(sendthis, iter_vect->message, sizeof(sendthis));
		sentChat = true;
	}

    // printf("I need to send : %s\n", sendthis);

	printf("trying to send %s\n", sendthis);
	if(send(clientID, sendthis, sizeof(sendthis), 0) < 0) {
		fprintf(stderr, "%s\n", "Something messed up in write");
	}
	printf("Trying to erase\n");
	if(sentChat) {
		cMap.find(clientID)->second.erase(cMap.find(clientID)->second.begin());   
	}

	printChats();
}

/**
 * This only works for the person who joined the chat session, not the one who created it.
**/
void getAll(int clientID) {
	char sendthis[LINELEN];
	bool sentChat;
	chatHistoryVect chatVector = cMap.find(clientID)->second; 
    chatHistoryVect::iterator iter_vect;
	if(chatVector.size() == 0) {
		strncpy(sendthis, "gurp", sizeof(sendthis));	
		sentChat = false;
		printf("trying to send %s\n", sendthis);
		if(send(clientID, sendthis, sizeof(sendthis), 0) < 0) {
			fprintf(stderr, "%s\n", "Something messed up in write");
		}
		return;
	}

	while(cMap.find(clientID)->second.size() != 0) {
		iter_vect = cMap.find(clientID)->second.begin();        
		strncpy(sendthis, iter_vect->message, sizeof(sendthis));

		printf("trying to send %s\n", sendthis);
		if(send(clientID, sendthis, sizeof(sendthis), 0) < 0) {
			fprintf(stderr, "%s\n", "Something messed up in write");
		}

		memset(&sendthis, 0, sizeof(sendthis));
		cMap.find(clientID)->second.erase(cMap.find(clientID)->second.begin());   
	}

	strncpy(sendthis, "END", sizeof(sendthis));
	// printf("Trying to send%s\n", sendthis);
	if(send(clientID, sendthis, sizeof(sendthis), 0) < 0) {
		fprintf(stderr, "%s\n", "Something messed up in write");
	}
	printf("Getall done\n");

}

void addClient(int clientID) {
	printf("Client %d has joined!\n", clientID);
	// Sanity check, shouldn't happen.
	if (clientID == 0) {
		fprintf(stderr, "Got a socket that equals 0. Invalidates my data structure. Not adding client.\n");
		return;
	}
	
	// Need to put client in map with message vector.
	// Check if the key exists in the map
	if ( cMap.find(clientID) == cMap.end() ) {
		// not found, create new vector
		cMap.insert(clientMessageMap::value_type(clientID, chatHistoryVect()));
	} else {
	  	// found
		fprintf(stderr, "Client %d is here when he shouldn't be. Map imporperly handled\n", clientID);
	}
}


void removeClient(int clientID) {
	fprintf(stderr, "Tried to remove client %d. Coudn't find him.\n", clientID);
}

int errexit(const char *format, ...) {
        va_list args;
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
        exit(1);
}