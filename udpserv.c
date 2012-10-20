/* include udpserv1 */
#include        "unpifiplus.h"

typedef struct 
{
  	int    				sockfd;   /* socket descriptor */
  	struct sockaddr 	*ip_addr;   /* IP address bound to socket */
  	struct sockaddr 	*ntmaddr;  /* netmask address for IP */
	struct sockaddr 	*subnetaddr;  /* netmask address */

}socket_info;

//typedef struct socket_info socket_info;
void	mydg_echo( int, SA *, socklen_t , SA * );

int main(int argc, char **argv)
{
	int					sockfd[10], sockcount = 0, countline = 0, n = 0;
	const int			on = 1;
	pid_t				pid;
	struct ifi_info		*ifi, *ifihead;
	struct sockaddr_in	*sa, cliaddr, wildaddr;
	char            	dataline[MAXLINE], ;
	char 				*configdata[2], *mode = "r";
	socket_info 		sockinfo[10];
	FILE 				*ifp;

	ifp = fopen( "server.in", mode );

	if ( ifp == NULL ) 
	{
		fprintf( stderr, "Can't open input file server.in!\n" );
		exit(1);
	}

	while ( fgets( dataline, MAXLINE, ifp ) != NULL )
	{
		n = strlen( dataline );
		dataline[n] = 0;
 
		configdata[ countline ] = dataline;
		fputs( dataline, stdout );
		printf( "reading from config %s \n", configdata[ countline ] ); 
		countline++;
	}
	printf( "read %d lines\n", countline );

	fclose( ifp );

	for ( ifihead = ifi = get_ifi_info_plus( AF_INET, 1 );
		ifi != NULL; ifi = ifi->ifi_next) 
	{

		sockfd[sockcount]=-1;
		printf("bound----- %d\n",sockcount);
		
		int x=0;	/*4bind unicast address */
		if( ( sockfd[ sockcount ] = socket( AF_INET, SOCK_DGRAM, 0 ) ) == NULL )
		{
			printf( "socket error\n" );	
			exit(1);	
		}	setsockopt( sockfd[sockcount], SOL_SOCKET, SO_REUSEADDR, &on, sizeof( on ) );

		bzero( &sa, sizeof( sa ) );
	
		sa->sin_family = AF_INET;
		sa->sin_port = htons( *configdata[0] );
		bind( sockfd[sockcount], (SA *) sa, sizeof( *sa ) );
		printf( "bound---\n" );	
		printf( "bound----- %d\n", sockcount );
		sockinfo[ sockcount ].sockfd = sockfd[ sockcount ];		
		sockinfo[ sockcount ].ip_addr = (SA *)sa;
		sockinfo[ sockcount ].ntmaddr = ifi->ifi_ntmaddr;
		//sockinfo[sockcount].subnetaddr=ifi->sockfd[sockcount];
		printf( "sockfd----- %d\n", sockinfo[ sockcount ].sockfd );

		printf("  IP addr: %s\n",
				sock_ntop_host( sockinfo[sockcount].ip_addr, sizeof( *sockinfo[sockcount].ip_addr ) ) );

		printf("ddr: %s\n",
				sock_ntop_host( sockinfo[sockcount].ntmaddr, sizeof(*sockinfo[sockcount].ntmaddr ) ) );
		sockcount++;
		//		if ( (pid = fork()) == 0) {		/* child */
		//			mydg_echo(sockfd, (SA *) &cliaddr, sizeof(cliaddr), (SA *) sa);
		//			exit(0);		/* never executed */
		//		}
		/* end udpserv1 */
	}
	exit(0);
}
/* end udpserv3 */

/* include mydg_echo */
void
mydg_echo( int sockfd, SA *pcliaddr, socklen_t clilen, SA *myaddr )
{
	int			n;
	char		mesg[MAXLINE];
	socklen_t	len;

	for ( ; ; ) 
	{
		len = clilen;
		n = recvfrom( sockfd, mesg, MAXLINE, 0, pcliaddr, &len );
		printf( "Recieved data...1\n" );

		printf( "Sending...\n" );
		sendto( sockfd, mesg, n, 0, pcliaddr, len );
		printf( "Sending...\n" );
	}
}
/* end mydg_echo */
