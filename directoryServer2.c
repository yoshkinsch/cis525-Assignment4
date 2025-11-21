#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <stdlib.h>
#include <unistd.h>
#include "inet.h"
#include "common.h"

struct ChatServer {
  char name[MAX];
  int port;
  int socket_id;
  struct sockaddr_in addr;
  LIST_ENTRY(ChatServer) chatServers;
};

LIST_HEAD(ChatServerList, ChatServer);

int main()
{
	struct sockaddr_in cli_addr, serv_addr;
  struct ChatServerList server_head;
  fd_set readset;
  
  LIST_INIT(&server_head);
  
	/* Create communication endpoint */
	int sockfd;			/* Listening socket */
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

	/* Bind socket to local address */
	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family		= AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port		= htons(SERV_TCP_PORT);

	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		perror("server: can't bind local address");
		return EXIT_FAILURE;
	}

	listen(sockfd, 5);

	for (;;) {

		/* TODO: Initialize and populate your readset and compute max_fd */ // - FIXED

    FD_ZERO(&readset);
		FD_SET(sockfd, &readset);
    int max_fd = sockfd;
    
    struct ChatServer *newChatServer;
		LIST_FOREACH(newChatServer, &server_head, chatServers) {
		  FD_SET(newChatServer->socket_id, &readset);
 			/* Compute max_fd as you go */
		  if (max_fd < newChatServer->socket_id) {max_fd = newChatServer->socket_id;}
		}

		/* FIXME: There should be a select call in here somewhere */ // - FIXED

    if (select(max_fd+1, &readset, NULL, NULL, NULL) > 0) {
    
      if (FD_ISSET(sockfd, &readset)) {
        
        /* Accept a new connection request */
    		socklen_t clilen = sizeof(cli_addr);
    		int newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
    		if (newsockfd < 0) {
    			perror("server: accept error");
    			close(newsockfd);
    			continue; /* We can ignore this */
    		}
    
    		fprintf(stderr, "%s:%d Accepted client connection from %s\n", __FILE__, __LINE__, inet_ntoa(cli_addr.sin_addr));
       
        struct ChatServer *temp_server_connection = malloc(sizeof(struct ChatServer));
        if (!temp_server_connection){
          perror("Unable to allocate memory");
          close(newsockfd);
          continue;
        }
        
        memset(temp_server_connection->name, 0, MAX);
        temp_server_connection->port = 0;
        temp_server_connection->socket_id = newsockfd;
        temp_server_connection->addr = cli_addr;
        
        LIST_INSERT_HEAD(&server_head, temp_server_connection, chatServers);
      }
    
  		/* TODO: Iterate through your client sockets */
      
      struct ChatServer *server_connected = LIST_FIRST(&server_head);
      while (server_connected != NULL) {
        struct ChatServer *next = LIST_NEXT(server_connected, chatServers);
      
        if(FD_ISSET(server_connected->socket_id, &readset)){
          char s[MAX] = {'\0'};
          
      		/* Read the request from the client */
      		ssize_t nread = read(server_connected->socket_id, s, MAX);
      		if (nread < 0) {
      			/* Not every error is fatal. Check the return value and act accordingly. */ // Handled in else if check below
      			fprintf(stderr, "%s:%d Error reading from client\n", __FILE__, __LINE__);  //DEBUG
      			continue;
      		}
          else if (nread == 0) {
    				// server here is disconnecting from the directory
            /* Remove client from list, free memory if needed */
            close(server_connected->socket_id);
  					LIST_REMOVE(server_connected, chatServers);
            free(server_connected);
            break;
          }
          else {
            
            /* Chat Client and Server Client reponse type
            
              c: chat Client is requesting a list of servers to connect to
              s: server client is registering with the directory
            
            */
            
        		if (s[0] == 'c') {
              char msg_to_send[MAX] = {'\0'};
        			// Client requesting list of servers
              
              struct ChatServer *server;
              
              if (LIST_EMPTY(&server_head)){
                snprintf(msg_to_send, MAX, "No servers currently running...");  
              }
              else {
                int server_number_count = 0;
                LIST_FOREACH(server, &server_head, chatServers){
                  
                  char ip[MAX] = {'\0'};
                  inet_ntop(AF_INET, &(server->addr.sin_addr), ip, MAX);
                  
                  snprintf(msg_to_send, MAX, "%d:%s:%s:%d\n", server_number_count, server->name, ip, server->port);
                  
                  if (write(server_connected->socket_id, msg_to_send, MAX) < 0){
                    perror("Error: write to socket failed...");
                  }
                  
                  server_number_count++;
                }
              }
              
              // No more servers to send to client, so we send a 'E' for end of server list
              snprintf(msg_to_send, MAX, "E");
              
              // Write to the client the server
              if (write(server_connected->socket_id, msg_to_send, MAX) < 0){
                perror("Error: write to socket failed...");
              }
              
              close(server_connected->socket_id);
              LIST_REMOVE(server_connected, chatServers);
              free(server_connected);
              break; 
        		}
        		else if (s[0] == 's') {
              char msg_to_send[MAX] = {'\0'};
              char *reg_msg = s + 1;
        			char topic[MAX];
              int serverPort;
              
              if( sscanf(reg_msg, "%[^:]:%d", topic, &serverPort) == 2) {
                // Duplicate name check
                int is_duplicate = 0;
                struct ChatServer *server;
                LIST_FOREACH(server, &server_head, chatServers){
                  if (server != server_connected && strncmp(server->name, topic, MAX) == 0) {
                    is_duplicate = 1;
                    snprintf(msg_to_send, MAX, "D"); // D is sent here for "Duplicate" to the chat Server
                    continue;
                  }
                }
                
                if (!is_duplicate) {
                  strncpy(server_connected->name, topic, MAX); // Name
                  server_connected->port = serverPort;         // Port
                  snprintf(msg_to_send, MAX, "Registration Successful!");
                }
              } // end sscanf
              else {
                snprintf(msg_to_send, MAX, "Invalid registration format");
              }
              
              if (write(server_connected->socket_id, msg_to_send, MAX) < 0) {
                perror("Error: write to socket failed...");
              }
              
              // Removing server_connected from list if duplicate
              if (strncmp(msg_to_send, "D", 1) == 0) {
                close(server_connected->socket_id);
                LIST_REMOVE(server_connected, chatServers);
                free(server_connected);
                break;
              }
        		}
        		else {
              char msg_to_send[MAX] = {'\0'};
	            snprintf(msg_to_send, MAX, "Invalid request");
              if (write(server_connected->socket_id, msg_to_send, MAX) < 0) {
                perror("Error: write to socket failed...");
              }
              close(server_connected->socket_id);
              LIST_REMOVE(server_connected, chatServers);
              free(server_connected);
              break;
        		}
          } // End Else
        } // End if(ISSET) 
        server_connected = next;
      } // End WHILE LOOP
    } // End Select
		else {
			/* Handle select errors */
      perror("Select Error");
		}
	}
}
