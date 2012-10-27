#include        "unpifiplus.h"
#include		"unprtt.h"
#include		<setjmp.h>
#include 	<sys/socket.h>

static struct rtt_info   rttinfo;
static int				 rttinit = 0;
static struct msghdr	 msgsend, msgrecv;	/* assumed init to 0 */

static struct hdr 
{
  uint32_t	seq;	/* sequence # */
  uint32_t	ts;		/* timestamp when sent */
} sendhdr, recvhdr;

static void	sig_alrm( int signo );
static sigjmp_buf	jmpbuf;

typedef struct 
{
	struct sockaddr_in  *ip_addr;   /* IP address bound to socket */
	int    sockfd;   /* socket descriptor */
	struct sockaddr_in  *ntmaddr;  /* netmask address for IP */
	struct sockaddr_in  *subnetaddr;  /* netmask address */

}socket_info;

static struct msghdr  *swnd; 
int send_window_size;

//typedef struct socket_info socket_info;
void mydg_echo( int sockfd, SA *servaddr, socklen_t servlen, SA *cliaddr , socklen_t clilen, char *filename );

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
	fclose( ifp );

	printf("Creating the send window array dynamically for input window size..\n");
	send_window_size = (int) atoi( configdata[1].data );
	swnd = (struct msghdr *) malloc( send_window_size*sizeof( struct msghdr ) );

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
				inet_ntop( AF_INET, &cliaddr.sin_addr, IPClient, MAXLINE );
				printf("Client address : %s\n",IPClient );
		
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
					mydg_echo( sockinfo[ i ].sockfd, (SA *) &childservaddr, sizeof(childservaddr), (SA *) &cliaddr, sizeof(cliaddr), mesg );
					exit(0);                /* never executed */
				}
              	
			}	
		}
	}


	exit(0);
}

/***************************************************************************************************************************/

ssize_t dg_send_packet( int fd, const void *outbuff, size_t outbytes, void *inbuff, size_t inbytes, const SA *destaddr, socklen_t destlen )
{
	ssize_t			n;
	struct iovec	iovsend[2], iovrecv[2];

	printf( "Entered the dg_send_packet() routine..\n" );	

	if( rttinit == 0 ) 
	{
		rtt_init( &rttinfo );
		rttinit = 1;
		rtt_d_flag = 1;
	}

	printf( "Preparing the msghdr structure for passing to sendmsg()..\n" );
	sendhdr.seq++;
	msgsend.msg_name = destaddr;
	msgsend.msg_namelen = destlen;
	msgsend.msg_iov = iovsend;
	msgsend.msg_iovlen = 2;
	iovsend[0].iov_base = &sendhdr;
	iovsend[0].iov_len = sizeof(struct hdr);
	iovsend[1].iov_base = outbuff;
	iovsend[1].iov_len = outbytes;

	printf( "Adding the packet to the send buffer..\n" );
	swnd[ (sendhdr.seq)%send_window_size ] = msgsend;

	signal(SIGALRM, sig_alrm);
	rtt_newpack( &rttinfo );		/* initialize for this packet */

sendagain:
	sendhdr.ts = rtt_ts( &rttinfo );

	printf( "Calling sendmsg() function now..\n" );	
	sendmsg( fd, &msgsend, 0 );
	printf( "Completed sending packet..\n" );	

	alarm( rtt_start( &rttinfo ) ); 	/* calc timeout value & start timer */

	if ( sigsetjmp( jmpbuf, 1 ) != 0 ) 
	{
		if ( rtt_timeout( &rttinfo ) < 0 ) 
		{
			err_msg( "dg_send_packet: no response from server, giving up" );
			rttinit = 0;	/* reinit in case we're called again */
			errno = ETIMEDOUT;
			return(-1);
		}
		goto sendagain;
	}
	alarm(0);		

	rtt_stop( &rttinfo, rtt_ts(&rttinfo) - recvhdr.ts );
	return(1);	/* return size of received datagram */
}

static void sig_alrm( int signo )
{
	siglongjmp( jmpbuf, 1 );
}

ssize_t dg_send( int fd, const void *outbuff, size_t outbytes, void *inbuff, size_t inbytes, const SA *destaddr, socklen_t destlen )
{
	ssize_t	n;

	printf( "Calling dg_send_packet() routine..\n" );
	n = dg_send_packet( fd, outbuff, outbytes, inbuff, 
						inbytes, destaddr, destlen );
	if ( n < 0 )
	{	
		err_quit("dg_send_packet error");
	}
		
	return( n );
}

void mydg_echo( int sockfd, SA *servaddr, socklen_t servlen, SA *cliaddr , socklen_t clilen, char *filename )
{
	int						n;
	char					mesg[MAXLINE];
	socklen_t				len, slen, slen1;
	int 					connfd;
	struct sockaddr_in      ss, ss1;
	char 					IPServer[20], IPClient[20];
	FILE 					*ifp;
	ssize_t					bytes;
	char					sendline[MAXLINE], recvline[MAXLINE + 1];

	printf( "Child server initiated...\n" );
	printf( "******************* Child Server *********************\n" );

	printf( "Creating Datagram...\n" );
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

	slen1 = sizeof( ss1 ); 

	if( getpeername( connfd, (SA *)&ss1, &slen1 ) < 0 )
	{
		printf( "peername error\n" );
		exit(1);
	}

	inet_ntop( AF_INET, &(ss1.sin_addr), IPClient, MAXLINE );

	printf( "******************* dg_send() *********************\n" );
 	printf( "dg_send(): Destination Address :  %s\n",IPClient );
	printf( "dg_send(): Destination Port : %d\n", ss1.sin_port );


	sprintf( mesg, "%d\n", ss.sin_port );
	printf("Sending the Ephemeral port number to client : %s..\n", mesg );

	if( sendto( sockfd, mesg, sizeof( mesg ), 0, cliaddr, clilen) < 0 ) 
	{
		printf("ERROR IN SENDTO : %d ",errno);
		exit(0);
	}	

	printf( "Reading from the newly connected socket.\n" );
	n = read( connfd, mesg, MAXLINE );
	printf("Receieved data : %s\n",mesg );
	
	if( n > 0 && strcmp( mesg, "ACK\n" ) == 0 )
	{
		printf("ACK recieved..\n");
		printf("Closing the listening socket on parent..\n");
		close( sockfd );
	}

	printf( "Picking data from the file..\n" );
	ifp = fopen( filename, 	"r" );

	printf("Sending file to the client..\n");	
//	while ( fgets( sendline, MAXLINE, ifp ) != NULL ) 
//	{
		fgets( sendline, MAXLINE, ifp );
		/* Pick the data from the file  */ 
		strcpy( sendline, "this is a test string\n");
		printf("Calling dg_send() with picked up data : %s \n", sendline );

//	struct sockaddr_in test = malloc( sizeof( struct ) )
//	*cliaddr;
		
//	printf("dg_send(): Destination Address : %s\n", inet_ntop( AF_INET, &(cliaddr->sin_addr), IPClient, MAXLINE ));
//	printf( "dg_send(): Destination Port: %d\n", &(cliaddr->sin_port) );

	//	send( connfd, sendline, strlen( sendline ), 0 );

		bytes = dg_send( connfd, sendline, strlen( sendline ), recvline, MAXLINE, cliaddr, clilen );
//	}


}
