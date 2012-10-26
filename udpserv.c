/* include udpserv1 */
#include        "unpifiplus.h"

typedef struct 
{
	struct sockaddr_in  *ip_addr;   /* IP address bound to socket */
	int    sockfd;   /* socket descriptor */
	struct sockaddr_in  *ntmaddr;  /* netmask address for IP */
	struct sockaddr_in  *subnetaddr;  /* netmask address */

}socket_info;

//typedef struct socket_info socket_info;
void mydg_echo( int sockfd, SA *servaddr, socklen_t servlen, SA *cliaddr , socklen_t clilen);
typedef struct 
{
	char data[MAXLINE];
}config;


int main(int argc, char **argv)
{
	int					sockfd[10], sockcount = 0, countline = 0, n = 0, s, nready, maxfdp1, i;
	fd_set 				rset, allset;
	socklen_t			len, slen;
	const int			on = 1;
	pid_t				pid;
	struct ifi_info		*ifi, *ifihead;
	struct sockaddr_in	*sa, cliaddr, wildaddr, ss,childservaddr;	
	char            	dataline[MAXLINE], mesg[MAXLINE], IPClient[MAXLINE],IPServer[MAXLINE];
	config 				configdata[2]; 
	char 				*mode = "r";
	socket_info 		sockinfo[10];
	FILE 				*ifp;

	ifp = fopen( "server.in", mode );
	if ( ifp == NULL ) 
	{
		fprintf( stderr, "Can't open input file server.in!\n" );
		exit(1);
	}

	while ( fgets( dataline,MAXLINE,ifp ) != NULL )
	{
		n = strlen( dataline );
		strcpy( configdata[countline].data, dataline );
		s = strlen( configdata[ countline ].data );

    	if ( s > 0 && configdata[ countline ].data[ s-1 ] == '\n' )  /* if there's a newline */
    	{	
        	configdata[countline].data[s-1] = '\0';          /* truncate the string */
        }	

		dataline[n] = 0;
		countline++;	
	}	
	
	printf( "read %d lines\n", countline );

	fclose( ifp );

	for ( ifihead = ifi = get_ifi_info_plus( AF_INET, 1 );
		  ifi != NULL;
		  ifi = ifi->ifi_next) 
	{
		sockfd[sockcount]=-1;
		printf("bound----- %d\n",sockcount);
		
		int x = 0;	/*4bind unicast address */
		if( ( sockfd[ sockcount ] = socket( AF_INET, SOCK_DGRAM, 0 ) ) == NULL )
		{
			printf( "socket error\n" );	
			exit(1);	
		}	setsockopt( sockfd[sockcount], SOL_SOCKET, SO_REUSEADDR, &on, sizeof( on ) );

		sa = ( struct sockaddr_in * )ifi->ifi_addr;

		sa->sin_family = AF_INET;
	//	sa->sin_port = htonl( ( size_t )configdata[0].data );
		sa->sin_port = htonl( 12345 );
		
		bind( sockfd[sockcount], (SA *) sa, sizeof( struct sockaddr_in ) );

		slen = sizeof( ss );

		if( getsockname( sockfd[sockcount], (SA *)&ss, &slen ) < 0 )
		{
			printf( "sockname error\n" );
			exit(1);
		}	

		printf("BOUND SOCKET ADDRESSES : %s\n", inet_ntop( AF_INET, &(ss.sin_addr), IPClient, MAXLINE ));	
		printf( "SERVER PORT: %d\n", ss.sin_port );

		sockinfo[ sockcount ].sockfd = sockfd[ sockcount ];		
		sockinfo[ sockcount ].ip_addr = (SA *)sa;
		sockinfo[ sockcount ].ntmaddr = (struct sockaddr_in *)ifi->ifi_ntmaddr;
		//sockinfo[sockcount].subnetaddr=ifi->sockfd[sockcount];

		printf( "sockfd----- %d\n", sockinfo[ sockcount ].sockfd );
		printf("  IP addr: %s\n",
				sock_ntop_host( (SA *)sockinfo[sockcount].ip_addr, sizeof( *sockinfo[sockcount].ip_addr ) ) );

		printf("ddr: %s\n",
				sock_ntop_host( (SA *)sockinfo[sockcount].ntmaddr, sizeof(*sockinfo[sockcount].ntmaddr ) ) );
		sockcount++;
		//		if ( (pid = fork()) == 0) {		/* child */
		//			mydg_echo(sockfd, (SA *) &cliaddr, sizeof(cliaddr), (SA *) sa);
		//			exit(0);		/* never executed */
		//		}
		/* end udpserv1 */
	}

	maxfdp1 = -1;
	FD_ZERO( &allset );
	for( i = 0; i<sockcount; i++ )
	{
		FD_SET( sockinfo[ i ].sockfd, &allset );	
		if( sockinfo[ i ].sockfd > maxfdp1 )
		{
			maxfdp1 = sockinfo[ i ].sockfd;
		}	
	}	

	for ( ; ; ) 
	{
		rset = allset;
		if ( ( nready = select( maxfdp1 + 1, &rset, NULL, NULL, NULL ) ) < 0 ) 
		{
			if ( errno == EINTR )
				continue;		/* back to for() */
			else
				err_sys("select error");
		}

		for( i = 0; i<sockcount; i++ )
		{
			if( FD_ISSET( sockinfo[ i ].sockfd, &rset ) )
			{
				len = sizeof( cliaddr );
				n = recvfrom( sockinfo[ i ].sockfd, mesg, MAXLINE, 0, (SA *) &cliaddr, &len );
		
				printf("Recieved message from client : %s\n", mesg );
				inet_ntop( AF_INET, &cliaddr.sin_addr, mesg, MAXLINE );
				printf("Client address : %s\n",mesg );
		
				/* Returns which interface onto which the client has connected. */ 
				if( getsockname( sockinfo[ i ].sockfd, (SA *)&ss, &slen ) < 0 )
				{
					printf( "sockname error\n" );
					exit(1);
				}      
				
				printf("Connection recieved on :\n ");	
				printf("BOUND SOCKET ADDRESSES : %s\n", inet_ntop( AF_INET, &(ss.sin_addr), IPServer, MAXLINE ));
				printf( "SERVER PORT: %d\n", ss.sin_port );
				printf( "sockfd----- %d\n", sockinfo[ i ].sockfd );
		
				/* Filling up the child server address structure. */
				childservaddr.sin_family = AF_INET;
       			childservaddr.sin_port = htonl( 0 );
				inet_pton( AF_INET, IPServer, &childservaddr.sin_addr );
				slen = sizeof( ss );
				inet_pton( AF_INET, IPServer, &childservaddr.sin_addr );	
	
				//sendto( sockinfo[ i ].sockfd, mesg, sizeof( mesg ), 0, (SA *) &cliaddr, len);
				if ( ( pid = fork() ) == 0 ) 
				{             /* child */
					mydg_echo( sockfd, (SA *) &childservaddr, sizeof(childservaddr), (SA *) &cliaddr, sizeof(cliaddr) );
					exit(0);                /* never executed */
				}
              	
			}	
		}
	}


	exit(0);
}
/* end udpserv3 */

/* include mydg_echo */
void mydg_echo( int sockfd, SA *servaddr, socklen_t servlen, SA *cliaddr , socklen_t clilen)
{
	int						n;
	char					mesg[MAXLINE];
	socklen_t				len, slen;
	int 					connfd;
	struct sockaddr_in      ss;
	char 					IPServer[20];

	printf( "child server initiated...\n" );

	
	if( ( connfd = socket( AF_INET, SOCK_DGRAM, 0 ) ) == NULL )
	{
		printf( "socket error\n" );
		exit(1);
	}

	/* Bind to IPServer and EPHEMERAL PORT and return EPHEMERAL PORT */
	bind( connfd, (SA *) servaddr, sizeof( struct sockaddr_in ) );
	slen = sizeof( ss );

	if( getsockname( connfd, (SA *)&ss, &slen ) < 0 )
	{
		printf( "sockname error\n" );
		exit(1);
	}      

	printf("BOUND SOCKET ADDRESSES : %s\n", inet_ntop( AF_INET, &(ss.sin_addr), IPServer, MAXLINE ));
	printf( "SERVER PORT: %d\n", ss.sin_port );

	/* Connect to IPClient */
	if( connect( connfd, cliaddr, clilen ) < 0 )
	{
		printf( "Error in connecting to server..\n" );
		exit(1);
	}	

	if( getpeername( connfd, (SA *)&ss, &slen ) < 0 )
	{
		printf( "peername error\n" );
		exit(1);
	}

	inet_ntop( AF_INET, &(ss.sin_addr), IPServer, MAXLINE );

	printf( "******************* SERVER INFO *********************\n" );
        printf( "IPClient: %s\n",IPServer);
        printf( "Client PORT: %d\n", ss.sin_port );
        	


	sprintf( mesg, "%d\n", ss.sin_port );
	
	//sendto( sockfd, mesg, sizeof( mesg ), 0, cliaddr, clilen);

	
	if( sendto( sockfd, mesg, sizeof( mesg ), 0, cliaddr, clilen) < 0 ) 
	{
		printf("ERROR IN SENDTO : %d ",errno);
		exit(0);
	}	
	
	/*	for ( ; ; ) 
	{
		len = clilen;
		n = recvfrom( sockfd, mesg, MAXLINE, 0, pcliaddr, &len );
		printf( "Recieved data...1\n" );

		printf( "Sending...\n" );
		sendto( sockfd, mesg, n, 0, pcliaddr, len );
		printf( "Sending...\n" );
	}
*/
}
/* end mydg_echo */
