#include	"unpifiplus.h"
#include	<string.h>
#include 	<arpa/inet.h>
#include	"unprtt.h"
#include	<setjmp.h>
#include 	<sys/socket.h>

#undef  MAXLINE
#define MAXLINE 65507
typedef char* string; 

typedef struct 
{
	struct sockaddr_in  *ip_addr;   /* IP address bound to socket */
	int    sockfd;   /* socket descriptor */
	struct sockaddr_in  *ntmaddr;  /* netmask address for IP */
	struct sockaddr_in  *subnetaddr;  /* netmask address */
}socket_info;

typedef struct 
{
	char data[MAXLINE];
}config;

static struct msghdr	msgrecv;	/* assumed init to 0 */

int 		  		  global_ack_number = 0;

static struct msghdr  *rwnd; 
int 				  rwnd_start;
int 				  rwnd_end;
int 				  occupied = -1;

int 				  max_window_size;


static struct hdr 
{

  uint32_t	seq;	/* sequence # */
  uint32_t	ts;		/* timestamp when sent */
  uint32_t	ack_no;	/* sequence # */  		

}recvhdr;



void dg_cli1( FILE *fp, int sockfd, const SA *pservaddr, socklen_t servlen, config configdata[] );


int main( int argc, char **argv )
{
	int 		 			sockfd[10], n = 0, j = 0, countline = 0, x = 0, loopback = 0, socketIndex = 0;
	socklen_t				len, slen;
	struct sockaddr_in		cliaddr, servaddr;
	const int               on = 1;
	pid_t                   pid;
	struct ifi_info         *ifi, *ifihead;
	struct sockaddr_in      *sa, ss;
	config 					configdata[7]; 
	char            		dataline[MAXLINE];
	socket_info 			sockinfo[10];
	char  					IPServer[20], IPClient[20], str[INET_ADDRSTRLEN];   /* IP address bound to socket */
	int 					sockcount=0, s;
	FILE 					*ifp;
	char 					*mode = "r", *temp;
	
	ifp = fopen( "client.in", mode );

	/* Unable to opent the client input file */
	if ( ifp == NULL ) 
	{
		fprintf( stderr, "Can't open input file client.in!\n" );
		exit(1);
	}
	
	/*
	for(j=0; j<7;j++)
	{
	configdata[j].data="NA";
	printf("loop: %s\n",configdata[j].data);

	}
	dataline=malloc(MAXLINE);
	*/

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
		/*
		free(dataline);
		dataline=malloc(MAXLINE);	
		*/
	}	
	fclose( ifp );
	
	printf("Creating the recieve window array dynamically for input window size..\n");
	max_window_size = (int) atoi( configdata[3].data );
	rwnd = (struct msghdr *) malloc( max_window_size*sizeof( struct msghdr ) );

	sprintf( IPServer, "%s", configdata[0].data );
	
	printf("*************************************************************\n");
	printf("	  INTERFACE IP'S 	   						 \n");
	printf("*************************************************************\n");

	for ( ifihead = ifi = get_ifi_info_plus( AF_INET, 1 );
 		  ifi != NULL;
 		  ifi = ifi->ifi_next) 
	{
		/*
        sockfd[sockcount]=-1;

        if((    sockfd[sockcount]= socket(AF_INET, SOCK_DGRAM, 0))==NULL)
        {
			printf("socket error\n");
			exit(1);
        }
		setsockopt( sockfd[ sockcount ], SOL_SOCKET, SO_REUSEADDR, &on, sizeof( on ) );
		*/

		sa = ( struct sockaddr_in * ) ifi->ifi_addr;
		sa->sin_family = AF_INET;
		sa->sin_port = htonl( ( size_t )configdata[1].data );
		inet_pton( AF_INET, configdata[0].data, &servaddr.sin_addr );
 
		sockinfo[sockcount].sockfd=-1;
		sockinfo[sockcount].ip_addr=sa;
		sockinfo[sockcount].ntmaddr=(struct sockaddr_in *)ifi->ifi_ntmaddr;
		/* 
		sockinfo[sockcount].subnetaddr=ifi->sockfd[sockcount];
		*/
		strcpy( temp, sock_ntop_host( (SA *)sockinfo[ sockcount ].ntmaddr, sizeof( *sockinfo[ sockcount ].ntmaddr ) ) );
		printf( "IP Address : %s, Network Mask : %s\n", 
				sock_ntop_host( (SA *)sockinfo[ sockcount ].ip_addr, sizeof( *sockinfo[ sockcount ].ip_addr ) ),
				temp ); 
		
        sockcount++;
	}
	printf("*************************************************************\n");

	for ( x = 0; sockinfo[x].sockfd!=NULL; x++ )
	{
		if( strcmp( sock_ntop_host( (SA *)sockinfo[x].ip_addr, sizeof( *sockinfo[x].ip_addr ) ), configdata[0].data ) == 0 )
		{
			loopback = 1;
			socketIndex=x;		
			strcpy( IPServer, "127.0.0.1\n" );
			strcpy( IPClient, "127.0.0.1\n" );
			printf("STATUS : SAME HOST\nCLIENT ADDRESS : %s\nSERVER ADDRESS : %s\n", IPClient, IPServer );
			break;
		}
	}
	if( !loopback )
	{
		/* Longest prefix matching on output from getifinfo_plus */
		struct in_addr ip, netmask, subnet, serverip, default_ip;
		struct in_addr longest_prefix_ip, longest_prefix_netmask;
		char network1[MAXLINE], network2[MAXLINE], ip1[MAXLINE], nm1[MAXLINE];

		for ( x = 0; sockinfo[x].sockfd != NULL; x++ )
		{
			struct sockaddr_in *sd = (struct sockaddr_in *)(sockinfo[x].ip_addr);
			struct sockaddr_in *se = (struct sockaddr_in *)(sockinfo[x].ntmaddr);

			ip = sd->sin_addr;
			netmask = se->sin_addr;

			if( default_ip.s_addr == NULL )
			{
				default_ip = ip;
			}	

	 	   	subnet.s_addr = ip.s_addr & netmask.s_addr;
	    	
	    	inet_ntop( AF_INET, &subnet, network1, MAXLINE );
	    	inet_ntop( AF_INET, &ip, ip1, MAXLINE );
	   		inet_ntop( AF_INET, &netmask, nm1, MAXLINE );
	   		
//	    	printf("IP : %s\nNet Mask : %s\nNetwork : %s\n", ip1, nm1, network1 );

	    	inet_pton( AF_INET, configdata[0].data, &serverip );
	    	subnet.s_addr = serverip.s_addr & netmask.s_addr;
			    	
			inet_ntop( AF_INET, &subnet, network2, MAXLINE );

//			printf("IP : %s\nNet Mask : %s\nNetwork : %s\n", configdata[0].data, nm1, network2 );

			if( strcmp( network1, network2 ) == 0 )
			{
				if( ( longest_prefix_netmask.s_addr == NULL ) 
					|| ( netmask.s_addr > longest_prefix_netmask.s_addr ) )
				{
					longest_prefix_netmask = netmask;
					longest_prefix_ip = ip;	
					socketIndex=x;	
				}	
			}	
		}
		if( longest_prefix_netmask.s_addr != NULL )
		{
			/* Longest prefix address is set as client IP. */
			strcpy( IPClient, inet_ntop( AF_INET, &longest_prefix_ip, network2, MAXLINE ) );
			printf("STATUS : LOCAL\nCLIENT ADDRESS (IPClient) : %s\nSERVER ADDRESS (IPServer) : %s\n", IPClient, IPServer );
		}	
		else
		{
			/* Assign any arbitrary IP address to the client. */
			strcpy( IPClient, inet_ntop( AF_INET, &default_ip, network2, MAXLINE ) );
			printf("STATUS : NOT LOCAL\nCLIENT ADDRESS : %s\nSERVER ADDRESS : %s\n", IPClient, IPServer );
			socketIndex = 0;	
		}	
	}

	int sockfd1;
	
	if( ( sockfd1 = socket( AF_INET, SOCK_DGRAM, 0 ) ) == NULL )
	{
		printf( "socket error\n" );
		exit(1);
	}
			
	setsockopt( sockfd1, SOL_SOCKET, SO_REUSEADDR, &on, sizeof( on ) );
	bzero( &cliaddr, sizeof( cliaddr ) );
		 
	cliaddr.sin_family = AF_INET;
	cliaddr.sin_port = 0;
	inet_pton( AF_INET, IPClient, &cliaddr.sin_addr );

	bind( sockfd1, (SA *)&cliaddr, sizeof( cliaddr ) );
	printf( "bound....%d\n", sockfd1 );	

	printf( "IPClient123: %s\n", inet_ntop( AF_INET, &(cliaddr.sin_addr), IPClient, MAXLINE ) );
	
	slen = sizeof( ss );

	/* To obtain IPClient and assigned ephemeral port number */	
	if( getsockname( sockfd1, (SA *)&ss, &slen ) < 0 )
	{
		printf( "sockname error\n" );
		exit(1);
	}	

	inet_ntop( AF_INET, &(ss.sin_addr), IPClient, MAXLINE );	
	printf( "******************* CLIENT INFO *********************\n" );
	printf( "IPClient: %s\n", inet_ntop( AF_INET, &(ss.sin_addr), IPClient, MAXLINE ) );
	printf( "EPHEMERAL PORT: %d\n", ss.sin_port );
	printf( "socket descriptor : %d\n", sockfd1 );
	

 	bzero( &servaddr, sizeof( servaddr ) );
	servaddr.sin_family = AF_INET;
//	servaddr.sin_port = htonl(configdata[1].data); //assigning server port from client.in;

	servaddr.sin_port = htonl(12345); //assigning server port from client.in;
	inet_pton( AF_INET, IPServer, &servaddr.sin_addr );

	printf(" calling dg_cli\n");	
   	dg_cli1( stdin, sockfd1, (SA *)&servaddr, sizeof( servaddr ), configdata );

	exit(0);
}

ssize_t dg_recieve( int fd, void *inbuff, size_t inbytes )
{
	ssize_t			n;
	struct iovec	iovrecv[2];
	char 			IPClient[20];

	memset(&msgrecv, '\0', sizeof(msgrecv)); 
	
	msgrecv.msg_name = NULL;
	msgrecv.msg_namelen = 0;
	msgrecv.msg_iov = iovrecv;
	msgrecv.msg_iovlen = 2;
	iovrecv[0].iov_base = &recvhdr;
	iovrecv[0].iov_len = sizeof(struct hdr);
	iovrecv[1].iov_base = inbuff;
	iovrecv[1].iov_len = inbytes;

	if( n = recvmsg( fd, &msgrecv, 0) < 0 ) 
	{
		printf("Error in RecvMsg!!\n");
	}	

	printf( "Adding the packet to the receive buffer at %dth position..\n", (recvhdr.seq)%max_window_size );
	rwnd[ (recvhdr.seq)%max_window_size ] = msgrecv;

	printf("Updating the occupied index to that of the newly inserted datagram.. %d\n", (recvhdr.seq)%max_window_size );
	occupied = (recvhdr.seq)%max_window_size;

	printf("We just recvmsg()'ed !.. %s\n", inbuff );
	printf(" %s\n", inbuff );

//	return (n);
	return ( n- sizeof(struct hdr) );
}

void update_ack()
{
	while(1)
	{
		
		if( rwnd[ global_ack_number%max_window_size ].msg_iov != NULL )
		{
			global_ack_number++;
		}
		else
		{
			break;
		}	
	}
	printf("Current global Acknowledgement Number is %d..\n", global_ack_number );

	return;
}


ssize_t dg_send_ack( int fd )
{
	ssize_t			n;
	struct iovec	iovrecv[2];
	char 			IPClient[20];

	memset( &msgrecv, '\0', sizeof( msgrecv ) ); 
	memset( &recvhdr, '\0', sizeof( recvhdr ) ); 
	
	update_ack();

	recvhdr.ack_no = global_ack_number;
	msgrecv.msg_name = NULL;
	msgrecv.msg_namelen = 0;
	msgrecv.msg_iov = iovrecv;
	msgrecv.msg_iovlen = 1;
	iovrecv[0].iov_base = &recvhdr;
	iovrecv[0].iov_len = sizeof( struct hdr );
//	iovrecv[1].iov_base = inbuff;
//	iovrecv[1].iov_len = inbytes;

	if( n = sendmsg( fd, &msgrecv, 0) < 0 ) 
	{
		printf("Error in SendMsg!!\n");
	}	

//	printf("We just recvmsg()'ed !.. %s\n", inbuff );
//	printf(" %s\n", inbuff );

//	return (n);
	return ( n );
}

void dg_cli1( FILE *fp, int sockfd, const SA *pservaddr, socklen_t servlen, config configdata[] )
{
	int 					size;
	char 					sendline[MAXLINE], recvline[MAXLINE + 1];
	ssize_t					n;
	socklen_t 				slen;
	struct sockaddr_in      ss;
	char 					IPServer[20];	

	printf("Initializing the recieve window start and end..\n");
	rwnd_start = 0 ; 
	rwnd_end   = max_window_size - 1 ;
	
	if( connect( sockfd, pservaddr, servlen ) < 0 )
	{
		printf( "Error in connecting to server..\n" );
		exit(1);
	}	

	slen = sizeof( ss ); 

	if( getpeername( sockfd, (SA *)&ss, &slen ) < 0 )
	{
		printf( "peername error\n" );
		exit(1);
	}

	inet_ntop( AF_INET, &(ss.sin_addr), IPServer, MAXLINE );

	printf( "******************* SERVER INFO *********************\n" );
	printf( "IPServer: %s\n",IPServer);
    printf( "SERVER PORT: %d\n", ss.sin_port );
        	
	size = 70000;
	
	setsockopt( sockfd, SOL_SOCKET, SO_SNDBUF, &size, sizeof( size ) );
	setsockopt( sockfd, SOL_SOCKET, SO_RCVBUF, &size, sizeof( size ) );
	
	write( sockfd, configdata[2].data, strlen( configdata[2].data ) + 1 );
	n = read( sockfd, recvline, MAXLINE );
	
	recvline[ n ] = 0;
	fputs( recvline, stdout );
	printf( "Ephemeral Port Number Of Server Child : %s \n", recvline );
	
	/* Assigning server port from client.in */
	ss.sin_port = htonl( (uint16_t) atoi( recvline ) ); 

	printf( "Reconnecting on server child port number %d...\n", ss.sin_port );
	slen = sizeof( ss );
	if( connect( sockfd, &ss, slen ) < 0 )
	{
		printf( "Error in connecting to server..\n" );
		exit(1);
	}	

	printf( "Sending Acknowledgement to server using reconnected socket...\n" );
	write( sockfd, "ACK\n", strlen( "ACK\n" ) + 1 );

	bzero( &ss, sizeof( ss ) );
	slen = sizeof( ss );

	while( ( n = dg_recieve( sockfd, recvline, MAXLINE ) ) > 0 )
	{
		printf("%s\n", recvline );
		printf( "Received datagram from server child of %d bytes..\n", n );	
		
		/* After this send ACK */
		printf("Attempting to send an ACK..\n");
		dg_send_ack( sockfd );

		printf("Updating the start of the recieve window..\n");
		rwnd_start = global_ack_number%max_window_size - 1 ;

		printf("*****************************************\n");
		printf( "rwnd_start : %d\n", rwnd_start );
		printf( "rwnd_end : %d\n", rwnd_end );
		printf( "occupied : %d\n", occupied );
		printf("*****************************************\n");
		
		printf("Returning to recvmsg()..\n");
		}	
}
