#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/wait.h>
#include <termios.h>


//Declared constants:
#define PORTNUM 5910
#define SECRET "cs591secret\n"


//Function prototypes:
char *input_matches_protocol(int infd, char *protocol_str);
void exchange_commands_with_server(int server_fd);


int main(int argc, char *argv[])
{
  char *server_ip, *sock_input;
  int sock_fd;
  struct sockaddr_in servaddr;

  char *server1 = "<rembash2>\n";
  char *server2 = "<ok>\n";

//noncanonical input mode
  struct termios tattr;
  tcgetattr (0, &tattr);
  tattr.c_lflag &= ~(ICANON|ECHO); /* Clear ICANON and ECHO. */
  tattr.c_cc[VMIN] = 1;
  tattr.c_cc[VTIME] = 0;
  tcsetattr (0, TCSAFLUSH, &tattr);

  //Check for proper number of arguments:
  if (argc != 2) {
    fprintf(stderr,"Usage: lab4-client SERVER_IP_ADDRESS\n");
    return EXIT_FAILURE;
  }

  //Get server's IP address from arguments:
  server_ip = argv[1];

  //Create socket:
  if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    perror("Client: socket call failed");
    exit(EXIT_FAILURE); }

  //Set up server address struct:
  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(PORTNUM);
  inet_aton(server_ip,&servaddr.sin_addr);
  //older method: servaddr.sin_addr.s_addr = inet_addr(server_ip);

  //Connect to server:
  if (connect(sock_fd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
    perror("Client: connect call failed");
    exit(EXIT_FAILURE); }

  //Get and check server protocol ID:
  if ((sock_input = input_matches_protocol(sock_fd,server1)) != NULL) {
    fprintf(stderr,"Client: Invalid protocol ID from server: %s\n",sock_input);
    exit(EXIT_FAILURE); }

  //Write shared secret message to server:
//  if (write(sock_fd,SECRET,strlen(SECRET)) == -1 || write(sock_fd,"\n",1) == -1) {
  if (write(sock_fd,SECRET,strlen(SECRET)) == -1) {
    perror("Client: Error writing shared secret to socket");
    exit(EXIT_FAILURE); }

  //Get and check server shared secret response:
  if ((sock_input = input_matches_protocol(sock_fd,server2)) != NULL) {
    fprintf(stderr,"Client: Invalid shared secret aknowledgement from server: %s\n",sock_input);
    exit(EXIT_FAILURE); }

  exchange_commands_with_server(sock_fd);

  printf("\nClient terminating...\n");

  //Normal termination:
  exit(EXIT_SUCCESS);
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


// Function to exchange commands read from terminal
// and send them to remote shell, and simultaneously
// read output from the shell and print it out to terminal.
void exchange_commands_with_server(int server_fd)
{
  {for (;;) {
			char c;
  			fd_set set;
			FD_ZERO(&set);
  			FD_SET(0,&set);
  			FD_SET(server_fd,&set);
  			select(server_fd+1,&set,NULL,NULL,NULL);
  			if (FD_ISSET(0,&set)) {
       				int a = read(0,&c,1);
				if (a < 1)
					break;
      		 		write(server_fd,&c,1);
  				}
  			if (FD_ISSET(server_fd,&set)) {
       				int b = read(server_fd,&c,1);
				if (b < 1)
					break;
       				write(1,&c,1);
  				}
		   }
    
 	 }
  return;
}


//EOF
