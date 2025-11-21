#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/queue.h>
#include <unistd.h>
#include "inet.h"
#include "common.h"

struct chat_server {
    int list_number;
    char name[MAX];
    char ip[MAX];
    int port;
    LIST_ENTRY(chat_server) servers;
};

LIST_HEAD(server_list, chat_server);

// Used to query the directory server
int active_servers(int directory_server_sockfd, struct server_list *server_head)
{
    char s[MAX] = {'\0'};
    int server_count = 0;
    
    snprintf(s, MAX, "c");
    write(directory_server_sockfd, s, MAX);
    
    // Reading from directory server for list of chatrooms
    printf("Chatrooms active:\n");
    
    while (server_count < MAX){
      ssize_t nread = read(directory_server_sockfd, s, MAX);
      if (nread <= 0)
      {
        perror("Directory Server Request Error");
        return 0;
      }
      
      // Here we found the end of the server list from the directory server if an 'E' was sent
      if (s[0] == 'E'){
        break; 
      }
      
      char name[MAX] = {'\0'};
      char ip[MAX] = {'\0'};
      int port = 0;
      int list_number = 0;
      
      if (sscanf(s, "%d:%[^:]:%[^:]:%d", &list_number, name, ip, &port) == 4){
        struct chat_server *new_server = malloc(sizeof(struct chat_server));
        new_server->list_number = list_number;
        snprintf(new_server->name, MAX, "%s", name);
        snprintf(new_server->ip, MAX, "%s", ip);
        new_server->port = port;
        LIST_INSERT_HEAD(server_head, new_server, servers);
        
        printf("%d: (%s) %s:%d\n", new_server->list_number, new_server->name, new_server->ip, new_server->port);
        server_count++;
        
      }
    }
    
    return server_count;
}

int main()
{
	char s[MAX] = {'\0'};
  char server_host_addr[INET_ADDRSTRLEN] = {'\0'};
	int				sockfd, directory_sockfd, server_tcp_port;
	struct sockaddr_in serv_addr, directory_addr;
  struct server_list server_head;
	fd_set			readset;
  int has_nickname = 0;
  
  LIST_INIT(&server_head);

  /* TODO - Assignment 4 (chatClient changes)
    1. Need to connect with the chatDirectory and query using active_servers method above to send a request to get active servers in the directory
    2. Then we connect to the server using its Server Address and TCP Port
  */

  /* Set up the address of the directory server to be contacted. */
	memset((char *) &directory_addr, 0, sizeof(directory_addr));
	directory_addr.sin_family			= AF_INET;
	directory_addr.sin_addr.s_addr	= inet_addr(SERV_HOST_ADDR);	/* hard-coded in inet.h */
	directory_addr.sin_port			= htons(SERV_TCP_PORT);			/* hard-coded in inet.h */

  /* Create a socket (an endpoint for communication). */
	if ((directory_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("client: can't open stream socket");
		return EXIT_FAILURE;
	}
 
  /* Connect to the directory server. */
	if (connect(directory_sockfd, (struct sockaddr *) &directory_addr, sizeof(directory_addr)) < 0) {
		perror("client: can't connect to server");
		return EXIT_FAILURE;
	}

  
  // Check if the directory server is either currently running (request invalid) or contains no chatrooms
  // 0 => problem with directory server or no chatrooms active
  int server_count = active_servers(directory_sockfd, &server_head);
  if(server_count == 0)
  {
    return EXIT_FAILURE;
  }
  
  int server_found = 0;
  // Iterating through to see if the name matches any of those found from Directory server
  do {
    
    // Here we should have the list of servers, their IP addresses, and their ports
    // And we ask client to connect to the server they want to select
    int desired_server;
    char input[MAX] = {'\0'};
      
    printf("Please enter the number of which server you would like to join: ");
    if (scanf("%s", input) != 1) {
      printf("Error: reading input error.");
        
    }
      
    if (sscanf(input, "%d", &desired_server) != 1) {
      printf("Invalid input. Please enter a valid number.\n");
      continue;
    }
    struct chat_server *s;
    LIST_FOREACH(s, &server_head, servers) {
      if (s->list_number == desired_server) {
        snprintf(server_host_addr, INET_ADDRSTRLEN, "%s", s->ip);
        server_tcp_port = s->port;
        server_found = 1;
        break;
      }
    }
      
    if (!server_found) {
      printf("Server %d not found. Please try again.\n", desired_server);
    }  
      
  } while (!server_found);
    
  // If check - in case somehow some black-magic has ocurred and server_found is false
  if (!server_found){
    printf("Error: Server not found (please use a number)");
    return EXIT_FAILURE;
  }
  
  // We check to see if the connection was valid, if not we close
  memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family			= AF_INET;
	serv_addr.sin_addr.s_addr	= inet_addr(server_host_addr);	
	serv_addr.sin_port			= htons((uint16_t)server_tcp_port);	
  
	/* Create a socket (an endpoint for communication). */
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("client: can't open stream socket");
		return EXIT_FAILURE;
	}

	/* Connect to the server. */
	if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		perror("client: can't connect to server");
		return EXIT_FAILURE;
	}
	
	for(;;) {

		FD_ZERO(&readset);
		FD_SET(STDIN_FILENO, &readset);
		FD_SET(sockfd, &readset);

		if (select(sockfd+1, &readset, NULL, NULL, NULL) > 0)
		{
			/* Check whether there's user input to read */
			if (FD_ISSET(STDIN_FILENO, &readset)) {
				if (scanf(" %[^\n]", s) != -1) {
          
					/* Send the user's message to the server */
					char msg[MAX] = { '\0' };
          
					// First message will usually be the user's nickname (which will be asked by the server to start)
					// If they already have a nickname, the message will be a normal message
					if (!has_nickname){
						snprintf(msg, MAX, "n%s", s);
						has_nickname = 1;
					}
					else{
						snprintf(msg, MAX, "m%s", s);
					}
					write(sockfd, msg, MAX);
				} else {
					fprintf(stderr, "%s:%d Error reading or parsing user input\n", __FILE__, __LINE__); //DEBUG
				}
			}

			/* Check whether there's a message from the server to read */
			if (FD_ISSET(sockfd, &readset)) {
				ssize_t nread = read(sockfd, s, MAX);
        
				if (nread < 0) {
					fprintf(stderr, "%s:%d Error reading from server\n", __FILE__, __LINE__); //DEBUG
          close(sockfd);
          return EXIT_FAILURE;
				} 
        else if (nread == 0) {
          printf("Server has closed. Exiting Client...\n");
          break;
        }
        else {
				  //fprintf(stderr, "%s:%d Read %zd bytes from server: %s\n", __FILE__, __LINE__, nread, s); //DEBUG
          printf("%s\n", s);
					// This is the case in which the server rejected the nickname of our client/user
					if (s[0] == '-'){
						has_nickname = 0;
					}
				}
			}
		}
	}
	close(sockfd);
	// return or exit(0) is implied; no need to do anything because main() ends
}
