#include        "unpifiplus.h"
#include		"unprtt.h"
#include		<setjmp.h>
#include 		<sys/socket.h>

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define INT_MAX  +32767
#define PACKET_SIZE 50

static struct rtt_info   rttinfo;
static int				 rttinit = 0;
static struct msghdr	 msgsend, msgrecv, *tmp;	/* assumed init to 0 */

static struct hdr 
{
  uint32_t	seq;	/* sequence # */
  uint32_t	ts;		/* timestamp when sent */
  uint32_t	ack_no;	/* sequence # */  		
  uint32_t	recv_window_advertisement;	/* sequence # */  		

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

typedef struct 
{
	uint32_t	ts;
	char data[MAXLINE];
}send_buffer;

send_buffer *swnd1;

static struct msghdr  *swnd; 

/* Sliding window data */
int 	sender_window_size;
int 	recv_advertisement;
int 	na = 0;
int 	nt =  0;
int 	global_sequence_number = -1; 
int prev_ack=-1, current_ack = -1, dup_ack = 0;
int cwnd = 1, slowstart = 1, ssthresh = 5;

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
	sender_window_size = (int) atoi( configdata[1].data );
	swnd = (struct msghdr *) malloc( sender_window_size*sizeof( struct msghdr ) );

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

int dg_send_packet( int fd, const void *outbuff, size_t outbytes )
{
	ssize_t			n;
	struct iovec	iovsend[2], iovrecv[2];

	printf( "Entered the dg_send_packet() routine..\n" );	

	printf( "Preparing the msghdr structure for passing to sendmsg()..\n" );
	memset( &msgsend, '\0', sizeof( msgsend ) ); 
	memset( &sendhdr, '\0', sizeof( sendhdr ) ); 

	sendhdr.seq = global_sequence_number;
	msgsend.msg_name = NULL;
	msgsend.msg_namelen = 0;
	msgsend.msg_iov = iovsend;
	msgsend.msg_iovlen = 2;
	iovsend[0].iov_base = &sendhdr;
	iovsend[0].iov_len = sizeof(struct hdr);
	iovsend[1].iov_base = outbuff;
	iovsend[1].iov_len = outbytes;

	printf( "Adding the packet to the send buffer at %dth position..\n", (sendhdr.seq)%sender_window_size );
//	swnd[ (sendhdr.seq)%sender_window_size ] = msgsend;

	strcpy( swnd1[ (sendhdr.seq)%sender_window_size ].data, outbuff );
	swnd1[ (sendhdr.seq)%sender_window_size ].ts = sendhdr.ts;

	printf( "Calling sendmsg() function now..\n" );	
	int n1;

	sendhdr.ts = rtt_ts(&rttinfo);
	n1 = sendmsg( fd, &msgsend, 0 );
	alarm(rtt_start(&rttinfo));	/* calc timeout value & start timer */
	
	if( n1 > 0 )
	{	
		printf( "Completed sending packet.. with %d bytes...\n", n1 );	
	}
	else
	{
		printf("Failed to send..\n");
	}	
	return( (sendhdr.seq)%sender_window_size );	
}

static void sig_alrm( int signo )
{
	siglongjmp( jmpbuf, 1 );
}

void update_nt()
{
	nt = global_sequence_number + 1;
}

void update_na_first_packet()
{
	if( global_sequence_number == 0 )
	{
		na = 0;
	}	
}

int dg_send( int fd, const void *outbuff, size_t outbytes )
{
	int	n;
	/* Incrementing the sequence number counter to assign seq_no to packet being sent. */
	global_sequence_number++;

	/* Check if swnd is full || recv_advertisement == 0  */
//	if( (swnd[ sender_window_size - 1 ] != NULL) || recv_advertisement == 0 )
//	{
//		/* Recieve an ACK */
//		dg_recieve_ack( fd );
//	}	

	n = dg_send_packet( fd, outbuff, outbytes );
	if ( n < 0 )
	{	
		err_quit("dg_send_packet error");
	}
	
	/* Update nt = global_sequence_number + 1  */
	update_nt();

	/* If first packet.. Increment na */
//	update_na_first_packet();

	return( n );
}

void delete_datasegment( int na )
{
//	swnd[ na%sender_window_size ] = NULL;
	printf("Deleting the ACK'ed segment after updating na..\n");
//	tmp = &( swnd[ na%sender_window_size ] );
//	memset( tmp, '\0', sizeof( struct msghdr ) ); 

	swnd1[ na%sender_window_size ].data[0]='\0' ;

}

/* acknowledgement number = seq# of highest acknowledged packet + 1 */
void update_na( int acknowledgment_no )
{
	if( na < acknowledgment_no )
	{
		while( na != acknowledgment_no )
		{
			delete_datasegment( na );
			na++;
		}	
	}	
}

/* Recieves the ack's that are sent after the buffer is full... */
/* RETURNS ACK_NO */
int dg_recieve_ack( int fd )
{
	printf("Recieving the ACK's...\n");
	ssize_t			n;
	struct iovec	iovrecv[2];
	char 			IPClient[20];

	memset( &msgrecv, '\0', sizeof(msgrecv) ); 
	memset( &recvhdr, '\0', sizeof(recvhdr) ); 
	
	msgrecv.msg_name = NULL;
	msgrecv.msg_namelen = 0;
	msgrecv.msg_iov = iovrecv;
	msgrecv.msg_iovlen = 2;
	iovrecv[0].iov_base = &recvhdr;
	iovrecv[0].iov_len = sizeof(struct hdr);
	
	if( n = Recvmsg( fd, &msgrecv, 0) < 0 ) 
	{
		printf("Error in RecvMsg!!\n");
		printf("Error no : %d\n", errno );
		exit(0);
	}
	
//	printf( "Adding the packet to the receive buffer at %dth position..\n", (recvhdr.seq)%reciever_window_size );
//	rwnd[ (recvhdr.seq)%reciever_window_size ] = msgrecv;

	//THIS WILL BE USED IF recvhdr.BLAH fails 
//	printf( "Updating the reciever advertisement global variable with %d..\n", msgrecv.msg_iov[0].iov_base.recv_window_advertisement );
//	recv_advertisement = msgrecv.msg_iov[0].iov_base.recv_window_advertisement;

	printf( "Updating the reciever advertisement global variable with %d..\n", recvhdr.recv_window_advertisement );
	recv_advertisement = recvhdr.recv_window_advertisement;

//	printf("Updating the occupied index to that of the newly inserted datagram.. %d\n", (recvhdr.seq)%reciever_window_size );
//	occupied = (recvhdr.seq)%reciever_window_size;

//	printf("We just recvmsg()'ed !.. %s\n", inbuff );
//	printf(" %s\n", inbuff );

	update_na( recvhdr.ack_no );


	current_ack = recvhdr.ack_no;
 	
 	if( prev_ack == current_ack  )
	{
		/* We got 3 DUP's */
		dup_ack++;
	} 
	else
	{
		dup_ack = 0;
	}

	/* swap fpr next ACK */
	prev_ack = current_ack;

	return recvhdr.ack_no;
}

void status_report()
{
	printf("***********************************************************\n");
	printf("STATUS PRINT\n");
	printf("***********************************************************\n");
	printf("Recv advertisement : %d\n",recv_advertisement );
	printf("First unacked packet - Na  : %d\n",na );
	printf("Packet to be transmitted next : %d\n",nt );
	printf("Global Sequence Number : %d\n",global_sequence_number );
	printf("***********************************************************\n");

}

int dg_retransmit( int fd, int ack_recieved )
{
//	memset( tmp, '\0', sizeof( struct msghdr ) ); 
	
	ssize_t			n;
	struct iovec	iovsend[2], iovrecv[2];
	char 			outbuff[MAXLINE];

	memset( &msgsend, '\0', sizeof( msgsend ) ); 
	memset( &sendhdr, '\0', sizeof( sendhdr ) ); 

	strcpy( outbuff, swnd1[ ack_recieved%sender_window_size ].data );

	sendhdr.seq = ack_recieved;
	msgsend.msg_name = NULL;
	msgsend.msg_namelen = 0;
	msgsend.msg_iov = iovsend;
	msgsend.msg_iovlen = 2;
	iovsend[0].iov_base = &sendhdr;
	iovsend[0].iov_len = sizeof(struct hdr);
	iovsend[1].iov_base = (void *)outbuff;
	iovsend[1].iov_len = sizeof(outbuff);

	int n1;
	sendhdr.ts = rtt_ts(&rttinfo);
	n1 = sendmsg( fd, &msgsend, 0 );
	alarm(rtt_start(&rttinfo));	/* calc timeout value & start timer */

	
	if( n1 > 0 )
	{	
		printf( "Completed sending packet.. with %d bytes...\n", n1 );	
	}
	else
	{
		printf("Failed to send..\n");
	}	
	return( ack_recieved );	
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

	swnd1 = malloc( sender_window_size*sizeof(send_buffer) );
	int i;
	for( i = 0 ; i<sender_window_size; i++)
	{
			swnd1[i].data[0] = '\0';
	}	

	int buffer_position, send_counter = 0;

	memset( sendline, '\0', sizeof( sendline ) );

	printf("Sending file to the client..\n");	
//	while ( fgets( sendline, MAXLINE, ifp ) != NULL )
	int j, sender_usable_window, ack_recieved ;
	recv_advertisement = INT_MAX;
	while(1)
	{	
		//sender_usable_window = sender_window_size - ( nt - na ) ;	
		sender_usable_window = cwnd - ( nt - na ) ;	

		printf("Sender usable window : %d\n recv_advertisement : %d\nSend Counter : %d\n",sender_usable_window, recv_advertisement, send_counter );
		
		/* Check for if recv_adv == 0 i.e. no more packets to be sent. */
		j = MIN( sender_usable_window , recv_advertisement );
	
		if( (j == 0) || (send_counter == recv_advertisement) )
		{
			while(1)
			{	
				ack_recieved = dg_recieve_ack( connfd );

				if( dup_ack == 3 )
				{
					/* DUP ACK's case */
					//dup_ack = 0;
					printf("We got 3 dup acks !!\n");
					printf("Retransmit the packet %d.. )\n", ack_recieved );
					
					ssthresh = MIN( cwnd, recv_advertisement );
					ssthresh = ( MIN( ssthresh, 2 ) )/2;
					dg_retransmit( connfd, ack_recieved );	
				}	

				if( ack_recieved == nt )
				{	
					/* All Acks have been recieved.. */
					printf("All ACK's have been recieved for buffer..\n");
					alarm(0);
					rtt_stop(&rttinfo, rtt_ts(&rttinfo) - swnd1[ (ack_recieved-1)%sender_window_size ].ts);
					
					send_counter = 0;
					break;
				}	
				//printf("After j==0 dg_recieve_ack\n");
				//status_report();
			}
			if(  slowstart )
			{
				cwnd *= 2;
				if( cwnd > ssthresh )
				{
					cwnd = ssthresh;
					slowstart = 0;
				}		
			}
			else
			{
				cwnd += 1;
			}
		}	
		else if( fread( sendline, 1, PACKET_SIZE, ifp ) != NULL )
		{
			printf("***************************************************************************************\n");
			printf("Calling dg_send() with picked up data : %s of size %d\n", sendline, strlen(sendline) );
			printf("***************************************************************************************\n");

			printf("Starting RTT timer..\n");
			if( rttinit == 0 ) 
			{
				rtt_init( &rttinfo );		/* first time we're called */
				rttinit = 1;
				rtt_d_flag = 1;
			}


			signal(SIGALRM, sig_alrm);
			rtt_newpack( &rttinfo );		/* initialize for this packet and sets retransmission counter to 0*/
	
			buffer_position = dg_send( connfd, sendline, strlen( sendline ) );
			
			if ( sigsetjmp( jmpbuf, 1 ) != 0 ) 
			{
				if ( rtt_timeout( &rttinfo ) < 0 ) 
				{
					err_msg( "dg_send_recv: no response from server, giving up" );
					rttinit = 0;	/* reinit in case we're called again */
					errno = ETIMEDOUT;
					return(-1);
				}
				goto sendagain;
			}

			send_counter++;
			printf("After dg_send\n");
			status_report();

			printf("Buffer position : %d\n", buffer_position );	

			memset( sendline, '\0', sizeof( sendline ) );
		}
		else
		{
			/* the last packets */
			while( na != nt )
			{	
				ack_recieved = dg_recieve_ack( connfd );
				if( dup_ack == 3 )
				{
					/* DUP ACK's case */
					printf("We got 3 dup acks !!\n");
					printf("Retransmit the packet %d.. And set the ssthresh to 1/2( min(cwnd, recv_window_advertisement,2) ) )\n", ack_recieved);
					ssthresh = MIN( cwnd, recv_advertisement );
					ssthresh = ( MIN( ssthresh, 2 ) )/2;
					//RETRANSMIT
					dg_retransmit( connfd, ack_recieved );
				}	
				printf("After na!=nt dg_recieve_ack\n");
				status_report();
			}	
			break;
		}	
		continue;
		sendagain : 
					while( na != nt )
					{
						printf("TIMEOUT EXPERIENCED..\n");	
						printf("Retransmitting the packet %d.. )\n", na);
						dg_retransmit( connfd, na );
					}
					ssthresh = MIN( cwnd, recv_advertisement );
					ssthresh = ( MIN( ssthresh, 2 ) )/2;
					cwnd = 1;
					slowstart = 1;

	}	

/*
	while( fread( sendline, 1, MAXLINE, ifp ) != NULL )
	{
		printf("***************************************************************************************\n");
		printf("Calling dg_send() with picked up data : %s of size %d\n", sendline, strlen(sendline) );
		printf("***************************************************************************************\n");

		buffer_position = dg_send( connfd, sendline, strlen( sendline ) );

		printf("Buffer position : %d\n", buffer_position );

		memset( sendline, '\0', sizeof( sendline ) );

	}

*/
}
