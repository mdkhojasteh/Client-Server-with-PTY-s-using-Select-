#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <wait.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pty.h>
#include <termios.h>


//Declared constants:
#define PORTNUM 5910
#define SECRET "cs591secret\n"


//Function prototypes:
void handle_client(int connect_fd);
char *input_matches_protocol(int infd, char *protocol_str);
void print_id_info(char *message);


int main()
{
  int listen_fd, connect_fd, fork_pid;
  struct sockaddr_in servaddr;

  //Set SIGCHLD signals to be ignored, which causes child process
  //results to be automatically discarded when they terminate:
  signal(SIGCHLD,SIG_IGN);

  //Create socket for server to listen on:
  if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    perror("Server: socket call failed");
    exit(EXIT_FAILURE); }

  //Set up socket so port can be immediately reused:
  int i=1;
  setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i));

  //Set up server address struct (simplified vs. text):
  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(PORTNUM);
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);  //Means accept all local interfaces.
  //As an alternative for setting up address, could have had for declaration:
  //struct sockaddr_in servaddr = {AF_INET, htons(PORTNUM), htonl(INADDR_ANY)};

  //Give socket a name/address by binding to a port:
  if (bind(listen_fd, (struct sockaddr *) &servaddr, sizeof(servaddr)) == -1) {
    perror("Server: bind call failed");
    exit(EXIT_FAILURE); }

  //Start socket listening:
  if (listen(listen_fd, 10) == -1) {
    perror("Server: listen call failed");
    exit(EXIT_FAILURE); }

  #ifdef DEBUG
  print_id_info("Server starting: ");
  #endif

  //Main server loop to wait for a new connection request and then fork off
  //child process to handle connection, with server continuing forever:
  while(1) {

    //Accept a new connection and get socket to use for client:
    if ((connect_fd = accept(listen_fd, (struct sockaddr *) NULL, NULL)) != -1) {

      //Make child process to handle this client:
      switch (fork_pid = fork()) {
      case -1:  //Error:
        perror("Server: fork failed");
        return EXIT_FAILURE;
      case 0:  //In child process:
        #ifdef DEBUG
        print_id_info("handle_client() subprocess: ");
        #endif
        //Eliminate inherited SIGCHLD handler since system call
        //creates child process that would trigger it:
        signal(SIGCHLD,SIG_DFL);
        //Call function to handle new client connection:
        handle_client(connect_fd);
        //Guarantee that child terminates after handling connection:
        exit(EXIT_SUCCESS);
      default:  //In parent process:
          //Main server loop will just continue to wait for connections,
          //but first, close un-needed connection fd to conserve fd's:
          close(connect_fd); } } }

  //Should never end up here, but just in case:
  return EXIT_FAILURE;
}



//Function to handle a client connection.  Implements the rembash
//protocol, and then runs bash in subprocess.
//stdin/stdout/stderr redirected from shell session back to client.
//Note that because client connection could close at any time, read errors
//are not reported as errors, but simply cause connection to be closed.
void handle_client(int connect_fd)
{
  char *sock_input;
  int stdin_fd, stdout_fd, stderr_fd;

  char *server1 = "<rembash2>\n";
  char *server2 = "<ok>\n";

  //Save stdin, stdout, and stderr fd's to restore after redirection for system():
  if((stdin_fd = dup(0)) == -1 || (stdout_fd = dup(1)) == -1 || (stderr_fd = dup(2)) == -1) {
    perror("Server: dup calls to save stdin and stderr failed");
    exit(EXIT_FAILURE); }

  //Write initial protocol ID to client:
  if (write(connect_fd,server1,strlen(server1)) == -1) {
    perror("Server: Error writing to socket");
    exit(EXIT_FAILURE); }

  //Check if correct shared secret was received: 
  if ((sock_input = input_matches_protocol(connect_fd,SECRET)) != NULL) {
    fprintf(stderr,"Client: Invalid shared secret received from client: %s\n",sock_input);
    exit(EXIT_FAILURE); }

  //Send OK response to client:
  if (write(connect_fd, server2, strlen(server2)) == -1) {
    perror("Server: Error writing OK to connection socket");
    exit(EXIT_FAILURE); }

  //Make child to run bash in:
  switch (fork()) {
  case -1:  //fork() error:
    perror("fork() failed");
    exit(EXIT_FAILURE);
  case 0:  //In new child process:
    #ifdef DEBUG
    print_id_info("Subprocess to exec bash, before setsid(): ");
    #endif

    //Create a new session for the new process:
    if (setsid() == -1) {
      perror("Server: setsid call failed");
      exit(EXIT_FAILURE); }

    #ifdef DEBUG
    print_id_info("Subprocess to exec bash, after setsid(): ");
    #endif

    //Setup stdin, stdout, and stderr redirection:
     	int masterfd, pid;
	if ((pid = forkpty(&masterfd, NULL, NULL, NULL)) < 0)
    		perror("FORK");
	
	else if (pid == 0)
	{
		
    		if (execlp("bash","bash","--noediting","-i",NULL)<0)
		{
       		 perror("execvp");
        	 exit(2);
    		}
	}

	else
	{
			for (;;) {
			char c;
  			fd_set set;
			FD_ZERO(&set);
  			FD_SET(masterfd,&set);
  			FD_SET(connect_fd,&set);
			int max;
			if (masterfd > connect_fd)
				{ max = masterfd;}
			else
				{ max = connect_fd;}
  			select(max+1,&set,NULL,NULL,NULL);
  			if (FD_ISSET(masterfd,&set)) {
       				int a = read(masterfd,&c,1);
				if (a < 1)
					break;
      		 		write(connect_fd,&c,1);
  				}
  			if (FD_ISSET(connect_fd,&set)) {
       				int b = read(connect_fd,&c,1);
				if (b < 1)
					break;
       				write(masterfd,&c,1);
  				}
		   }
    
 	 }

 
}
}
// Function to test if next input on socket matches
// what is supposed to be sent per protocol.
// Note:  returns NULL if matches, else returns string
// read from socket, to support printing error message.
char *input_matches_protocol(int infd, char *protocol_str)
{
  static char buff[513];  //513 to handle extra '\0'
  int nread;

  if ((nread = read(infd,buff,512)) == -1) {
      perror("Server: Error reading from socket");
      exit(EXIT_FAILURE); }

  buff[nread] = '\0';

  if (strcmp(buff,protocol_str) == 0)
    return NULL;
  else
    return buff;
}



// Function to print out detailed info about a process.
void print_id_info(char *message)
{
  printf("%sPID=%ld, PGID=%ld, SID=%ld\n", message, (long) getpid(), (long) getpgrp(), (long) getsid(0));
}


// EOF
