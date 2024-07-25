#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <asm-generic/socket.h>

#define BUFFER_SIZE 1024

//TODO: Make code safer by exiting if errors come up
//TODO: CLOSE ALL sockets after 
int sock_fd;

void sigint_handler(int signal){
  if (signal == SIGINT){
    printf("\nExiting... will close ports and sockets\n");
    if (close(sock_fd) < 0) {
        perror("close");
    } else {
        printf("Server socket closed successfully.\n");
    }
    exit(0);
  }
}

char* construct_http_response(char* path_name){
  char* response = malloc(512*sizeof(char));
  
  //increments the pointer by one so skips the '/'
  path_name += 1;
  
  //First we need HTTP boilerplate
  strcpy(response, "HTTP/1.1 200 OK\n");
  strcat(response, "Content-Type: text/html\n");
  strcat(response, "Connection: close\n\n");
  strcat(response, "<!DOCTYPE html>\n<html>\n<head>\n<title>");
  strcat(response, path_name);
  strcat(response, "</title>\n</head>\n<body>\n<h1>");
  strcat(response, path_name);
  strcat(response, "</h1>\n</body>\n</html>\n");
  
  return response;
}

void* handle_client(void* arg){
  printf("Handling new client!\n");
  int client_fd = *((int*) arg);
  char buffer[BUFFER_SIZE];

  ssize_t bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0);
  if(bytes_received <= 0){
    perror("no bytes received");
  }

  char method[6];
  char path[256];

  sscanf(buffer, "%4s %255s", method, path);
  printf("%s and path is %s\n", method, path);

  if(strcmp(method, "GET") != 0){
    perror("Method Not GET");
  }

  //construct http response
  char* response = construct_http_response(path);

  //printf("Response is:\n%s", response);
  int num_sent = send(client_fd, response, strlen(response), 0);
  printf("Sent %i bytes\n", num_sent);

  //Now close the client socket
  close(client_fd);
  free(response);
}

int main(void){
  printf("HEllo\n");
  if (signal(SIGINT, sigint_handler) == SIG_ERR) {
        perror("signal");
        exit(EXIT_FAILURE);
  }
  sock_fd = socket(AF_INET, SOCK_STREAM, 0);

  struct sockaddr_in serv_addr;
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(9001);
  serv_addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(sock_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
    perror("binding");
    exit(1);
  }

  int optval = 1;
  if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        perror("setsockopt SO_REUSEADDR");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }
    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)) < 0) {
        perror("setsockopt SO_REUSEPORT");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

  //Listening loop
  while (1) {
    //client info
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int* client_fd = malloc(sizeof(int));


    //accept client connection
    listen(sock_fd, 1);
    *client_fd = accept(sock_fd, (struct sockaddr*)&client_addr, &client_addr_len);
    if(*client_fd < 0){
      perror("Failed to accept client");
      continue;
    }

    printf("Client has connected!\n");
    //Create new thread to handle client request
    pthread_t thread_id;
    pthread_create(&thread_id, NULL, handle_client, (void *) client_fd);
    pthread_detach(thread_id);

  }

  close(sock_fd);
  

  return 0;
}