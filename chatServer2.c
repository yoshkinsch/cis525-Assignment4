#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/queue.h>
#include "inet.h"
#include "common.h"

// Make user struct to hold the socket ID, socket address, nickname, and Linked List entries of users
struct user {
    int socket_id;
    char nickname[MAX];
	  int has_nickname;
    LIST_ENTRY(user) users;
};

LIST_HEAD(user_list, user);

// Helper method to handle registration with the server
int directory_registration(char name[MAX], int port) 
{
  struct sockaddr_in dir_serv_addr;
  char register_msg[MAX];
  char s[MAX] = {'\0'};

  /* Create communication endpoint */
	int dir_sockfd;
	if ((dir_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("server: can't open stream socket");
		return -1;
	}
  
  /* Bind socket to local address */
	memset((char *) &dir_serv_addr, 0, sizeof(dir_serv_addr));
	dir_serv_addr.sin_family 		= AF_INET;
	dir_serv_addr.sin_addr.s_addr	= inet_addr(SERV_HOST_ADDR);	/* hard-coded in inet.h */
	dir_serv_addr.sin_port			= htons(SERV_TCP_PORT);			/* hard-coded in inet.h */
  
  
  // CONNECTING TO DIRECTORY SERVER

  if (connect(dir_sockfd, (struct sockaddr *) &dir_serv_addr, sizeof(dir_serv_addr)) < 0) {
		perror("server: can't connect to directory server");
		return -1;
	}
 
  /* TODO: Register with the directory server */
  
  /* Structure of message:
      s: server sending msg
      %s: Name/topic of server
      %d: port # of server
  */
  snprintf(register_msg, MAX, "s%s:%d", name, port);
  if (write(dir_sockfd, register_msg, MAX) < 0) {
    perror("Error sending registration to server...");
    close(dir_sockfd);
    return -1;
  }
  
  // MSG FROM DIRECTORY SERVER ('D' => Duplicate)
  ssize_t nread = read(dir_sockfd, s, MAX);
  if (nread < 0) {
    perror("Error reading registration response from directory server");
    close(dir_sockfd);
    return -1;
  }
  
  if (s[0] == 'D') {
    fprintf(stderr, "Registration failed: Duplicate server name\n");
    close(dir_sockfd);
    return -1;
  }
  else
  {
    fprintf(stderr, "Registration Successful\n");
  }
  
  return 0;
  
} // END directory_registration method

int main(int argc, char **argv)
{
  int sockfd, server_port;			/* Listening socket */
	struct sockaddr_in cli_addr, serv_addr;
	fd_set readset;
	struct user_list user_head;
  int user_count = 0;
  
  LIST_INIT(&user_head);
  
  /* TODO - Assignment 4 (chatServer changes)
    1. Need to register the server's name and port# with the Directory server
      1.1) If the registration goes wrong, we need to just close the connection (with a message as to why)
      1.2) If the registration is successfyl, then we continue on to what we did in assignment 3.
  */
  
  // CHECKING ARGUMENTS

  // Checking to see if the cmd args are the correct number (i.e. $./chatServer2 "Name" port#)
  if (argc != 3) {
    fprintf(stderr, "Error: Server Setup (./chatServer2 \"<topic>\" <port>)\n");
    return EXIT_FAILURE;
  }
  
  // If the second argument is not an integer (port #) then returns Error
  if (sscanf(argv[2], "%d", &server_port) != 1) {
    fprintf(stderr, "Error: Invalid port number (must be a valid integer)\n");
    return EXIT_FAILURE;
  }
  
  // COMMUNICATION ENDPOINT

  /* Create communication endpoint */
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("server: can't open stream socket");
		return EXIT_FAILURE;
	}

	/* Add SO_REUSEADDRR option to prevent address in use errors (modified from: "Hands-On Network
* Programming with C" Van Winkle, 2019. https://learning.oreilly.com/library/view/hands-on-network-programming/9781789349863/5130fe1b-5c8c-42c0-8656-4990bb7baf2e.xhtml */
	int true = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&true, sizeof(true)) < 0) {
		perror("server: can't set stream socket address reuse option");
		return EXIT_FAILURE;
	}
 
 
  // BINDING SOCKET
 
  /* Bind socket to local address */
	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family 		= AF_INET;
	serv_addr.sin_addr.s_addr 	= htonl(INADDR_ANY);
	serv_addr.sin_port			= htons((uint16_t)server_port);
 
  if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		perror("server: can't bind local address");
		return EXIT_FAILURE;
	}
 
  // REGISTERING WITH DIRECTORY
  if (directory_registration(argv[1], server_port) != 0){
    return EXIT_FAILURE;
  }
  
	listen(sockfd, 5);

// Starts going through clients
	for (;;) {

		/* Initialize and populate your readset and compute maxfd */
		FD_ZERO(&readset);
		FD_SET(sockfd, &readset);
		/* We won't write to a listening socket so no need to add it to the writeset */
		int max_fd = sockfd;

		/* FIXME: Populate readset with ALL your client sockets here,
		 * e.g., using LIST_FOREACH */
		/* clisockfd is used as an example socket -- we never populated it so it's invalid */
		//FD_SET(clisockfd, &readset); - FIXED

		// Populating the readset with all the client sockets (using linked lists foreach)
		struct user *newUser;
		LIST_FOREACH(newUser, &user_head, users) {
			FD_SET(newUser->socket_id, &readset);
			/* Compute max_fd as you go */
		  if (max_fd < newUser->socket_id) {max_fd = newUser->socket_id;}
		}

		

		if (select(max_fd+1, &readset, NULL, NULL, NULL) > 0) {

			/* Check to see if our listening socket has a pending connection */
			if (FD_ISSET(sockfd, &readset)) {
				/* Accept a new connection request */
				socklen_t clilen = sizeof(cli_addr);
				int newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
				if (newsockfd < 0) {
					perror("server: accept error");
					close (newsockfd);
					close(sockfd);
					return EXIT_FAILURE;
				}
				/* FIXME: Add newsockfd to your list of clients -- but no nickname yet */
				/* We can't immediately read(newsockfd) because we haven't asked
				* select whether it's ready for reading yet */

				struct user *new_user_connection = malloc(sizeof(struct user));
				new_user_connection->socket_id = newsockfd;
				new_user_connection->has_nickname = 0;
				memset(new_user_connection->nickname, 0, MAX);
				LIST_INSERT_HEAD(&user_head, new_user_connection, users);

				// Going to ask user for their name from the server
        char connection_msg[MAX] = {'\0'};
        snprintf(connection_msg, MAX-1, "Please enter a nickname before joining: ");
				write(new_user_connection->socket_id, connection_msg, MAX);
			}

			/* TODO: Check ALL your client sockets, e.g., using LIST_FOREACH */
      
      
			struct user *user_connected = LIST_FIRST(&user_head);
			while (user_connected != NULL) {
				struct user *next = LIST_NEXT(user_connected, users);
			/* clisockfd is used as an example socket -- we never populated it so
			* it's invalid */
 
			/* Note that this is a seperate if, not an else if -- multiple sockets
			* may become ready */
				if (FD_ISSET(user_connected->socket_id, &readset)) {
					/* FIXME: Modify the logic */

					char s[MAX] = {'\0'};

					/* Read the request from the client */
					/* FIXME: This may block forever since we haven't asked select
						whether clisockfd is ready */
					ssize_t nread = read(user_connected->socket_id, s, MAX);
          
					if (nread < 0) {
						/* Not every error is fatal. Check the return value and act accordingly. */
						fprintf(stderr, "%s:%d Error reading from client\n", __FILE__, __LINE__);
            
						return EXIT_FAILURE;
					}
					else if (nread == 0) {

						// client/user here is disconnecting from the server (need to broadcast that they left the server)
						// Also going to make sure that if they disconnected before entering a name into the server, their name won't be broadcasted to other users in the server

						char name_of_disconnected_user[MAX] = {'\0'};
						int had_nickname = 0;
						if (user_connected->has_nickname){
							snprintf(name_of_disconnected_user, MAX, "%s", user_connected->nickname);
							had_nickname = 1;
						}

						// Going to close/disconnect the user here, so I can check whether or not the list is empty after disconnecting them
            if (had_nickname) {
							
							// User has a nickname AND the list is NOT empty, so we can broadcast to all other users
							char disconnect_msg[MAX] = {'\0'};                       
              
							snprintf(disconnect_msg, MAX, "%s has left the chat", name_of_disconnected_user);
              
							struct user *already_connected_users;
							LIST_FOREACH (already_connected_users, &user_head, users){
								if (already_connected_users->has_nickname && already_connected_users != user_connected){
									write(already_connected_users->socket_id, disconnect_msg, MAX);
								}
							}
						}
            
            // Checking if the user_connected ever set a nickname
            // if they did then we decrease the user count
            if(user_connected->has_nickname){
              user_count--;
            }
            close(user_connected->socket_id);
            LIST_REMOVE(user_connected, users);
            free(user_connected);
            break;
					}
					else {

						/* Generate an appropriate reply based on the first character of the client's message */
						// Pointer to parse the contents of the message sent to the server by the client/user
						char *msg = s + 1;

						// Broadcast message to other clients/users
						char broadcast_msg[MAX] = {'\0'};

						if (s[0] == 'n' /* FIXME */) {
							/* YOUR LOGIC GOES HERE */
							// Message includes the name of the client/user - will make the first character of this 'n'
							// if they type a name that already exists then they must enter a new name

							int name_taken = 0;
							struct user *user_name;
							LIST_FOREACH (user_name, &user_head, users){
								if (strncmp(user_name->nickname, msg, MAX) == 0) {
									name_taken = 1;
                  break;
								}
							}

							if (!name_taken && s[1] != '-'){
								snprintf(user_connected->nickname, MAX, "%s", msg);	
                user_connected->has_nickname = 1;

								// Once the first client/user sets their nickname - they will either broadcast they joined or get told they are the first to join
								if(user_count == 0){
									snprintf(broadcast_msg, MAX, "You are the first user to join the chat\n");
                   
									write(user_connected->socket_id, broadcast_msg, MAX);
								}
								else{
									snprintf(broadcast_msg, MAX, "%s has joined the chat\n", user_connected->nickname);
                  
									struct user *already_connected_users;
									LIST_FOREACH (already_connected_users, &user_head, users){
										write(already_connected_users->socket_id, broadcast_msg, MAX);
									}
								}
                user_count++;
							}
							else if(s[1] == '-'){
								snprintf(broadcast_msg, MAX, "-Nickname cannot start with '-'. Choose a different nickname: ");
                write(user_connected->socket_id, broadcast_msg, MAX);
							}
							else{
								snprintf(broadcast_msg, MAX, "-Name is already taken, please enter a different nickname: ");
								write(user_connected->socket_id, broadcast_msg, MAX);
							}
							
						}
						else if (s[0] == 'm' /* FIXME */) {
							/* YOUR LOGIC GOES HERE */
							// Message is an actual message from the client/user - will make the first character of this 'm'
							// Need to broadcast the message to all of the users connected to the server (including the user/client that sent the message)
							
              if (!user_connected->has_nickname) {
                snprintf(broadcast_msg, MAX, "-You must set a nickname before sending messages.");
                write(user_connected->socket_id, broadcast_msg, MAX);
                continue;
              }
              
              snprintf(broadcast_msg, MAX, "%s: %s", user_connected->nickname, msg);
                           
							struct user *already_connected_users;
								LIST_FOREACH (already_connected_users, &user_head, users){
									if (already_connected_users->has_nickname) {
                    write(already_connected_users->socket_id, broadcast_msg, MAX);
                  }
								}
						}
						/* YOUR LOGIC GOES HERE */
						else {
							snprintf(broadcast_msg, MAX, "Invalid request");
							write(user_connected->socket_id, broadcast_msg, MAX);
						}
					}
				} // END FD_ISSET
        user_connected = next;
			} // END WHILE LOOP
		} // END IF SELECT
		else {
			/* Handle select errors */
			perror("Select Error");
		}
	}
}
