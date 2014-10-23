CC = g++
CFLAGS = -Wall -Wextra

all: chatClientMake chatCoordinatorMake chatServerMake

chatClientMake: chat_client.cc
	$(CC) $(CFLAGS) -o chat_client chat_client.cc -I.

chatCoordinatorMake: chat_coordinator.cc
	$(CC) $(CFLAGS) -o chat_coordinator chat_coordinator.cc -I.

chatServerMake: chat_server.cc
	$(CC) $(CFLAGS) -o chat_server chat_server.cc -I.

clean: 
	rm -f chat_client chat_coordinator chat_server
