#include	"unpifiplus.h"
#include	<string.h>
#include 	<arpa/inet.h>
#include	"unprtt.h"
#include	<setjmp.h>
#include 	<sys/socket.h>
#include 	<unpthread.h>
#include 	<math.h>

#undef  MAXLINE
#define MAXLINE 65507
#define PACKET_SIZE 496

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

typedef struct 
{
	uint32_t	ts;
	char data[MAXLINE];
}receive_buffer;

receive_buffer *rwnd1;

static struct msghdr	msgrecv, *tmp;

int 		  		  global_ack_number = 0,n;

static struct msghdr  *rwnd; 

/* Sliding window protocol */
int 	reciever_window_size;
int 	ns = 0;
int 	nr = 0;

/* Producer consumer Mutex  */
pthread_mutex_t	nr_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_t		tid;
int				val;


/* Consumed sequence numbers / datagrams */
int consumed = -1;	

/* Client.in parameters */
double mu;

/* Packet Drop  */
int packet_drop = 0;


static struct hdr 
{

  uint32_t	seq;	/* sequence # */
  uint32_t	ts;		/* timestamp when sent */
  uint32_t	ack_no;	/* sequence # */  		
  uint32_t	recv_window_advertisement;	/* sequence # */  		

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
	printf("***************************************************\n");
        printf("         DATA FROM CLIENT.IN\n");
        printf("***************************************************\n");
	
	while ( fgets( dataline,MAXLINE,ifp ) != NULL )
	{
		n = strlen( dataline );
		strcpy( configdata[countline].data, dataline );
		s = strlen( configdata[ countline ].data );

    	if ( s > 0 && configdata[ countline ].data[ s-1 ] == '\n' )  /* if there's a newline */
    	{	
        	configdata[countline].data[s-1] = '\0';          /* truncate the string */
        }	
	printf("%s\n",configdata[countline].data);

		dataline[n] = 0;
		countline++;	
	}	
	fclose( ifp );
	
	printf("Creating the recieve window array dynamically for input window size..\n");
	reciever_window_size = (int) atoi( configdata[3].data );

	rwnd = (struct msghdr *) malloc( reciever_window_size*sizeof( struct msghdr ) );
	memset( rwnd, '\0', reciever_window_size*sizeof( struct msghdr ) ); 


	sprintf( IPServer, "%s", configdata[0].data );
	
	printf("*************************************************************\n");
	printf("	  INTERFACE IP'S 	   						 \n");
	printf("*************************************************************\n");

	for ( ifihead = ifi = get_ifi_info_plus( AF_INET, 1 );
 		  ifi != NULL;
 		  ifi = ifi->ifi_next) 
	{
		sa = ( struct sockaddr_in * ) ifi->ifi_addr;
		sa->sin_family = AF_INET;
		sa->sin_port = htonl( ( size_t )configdata[1].data );
		inet_pton( AF_INET, configdata[0].data, &servaddr.sin_addr );
 
		sockinfo[sockcount].sockfd=-1;
		sockinfo[sockcount].ip_addr=sa;
		sockinfo[sockcount].ntmaddr=(struct sockaddr_in *)ifi->ifi_ntmaddr;

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
	cliaddr.sin_port = htons(0);
	inet_pton( AF_INET, IPClient, &cliaddr.sin_addr );

	bind( sockfd1, (SA *)&cliaddr, sizeof( cliaddr ) );
	//printf( "bound....%d\n", sockfd1 );	

	printf( "IPClient: %s\n", inet_ntop( AF_INET, &(cliaddr.sin_addr), IPClient, MAXLINE ) );
	
	slen = sizeof( ss );

	/* To obtain IPClient and assigned ephemeral port number */	
	if( getsockname( sockfd1, (SA *)&ss, &slen ) < 0 )
	{
		printf( "sockname error\n" );
		exit(1);
	}	

	inet_ntop( AF_INET, &(ss.sin_addr), IPClient, MAXLINE );	
	printf("\n**********************************************\n");
	printf( "       CLIENT INFO \n" );
	printf("**********************************************\n");	
	printf( "IP : %s\n", inet_ntop( AF_INET, &(ss.sin_addr), IPClient, MAXLINE ) );
	printf( "EPHEMERAL PORT: %d\n", ntohl(ss.sin_port) );
	//printf( "socket descriptor : %d\n", sockfd1 );
	

 	bzero( &servaddr, sizeof( servaddr ) );
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htonl(((int)atoi(configdata[1].data))); //assigning server port from client.in;

//	servaddr.sin_port = htonl(12345); //assigning server port from client.in;
	inet_pton( AF_INET, IPServer, &servaddr.sin_addr );

//	printf(" calling dg_cli\n");	
   	dg_cli1( stdin, sockfd1, (SA *)&servaddr, sizeof( servaddr ), configdata );

	exit(0);
}

void update_ns( int packet_sequence_number )
{
	if( (packet_sequence_number + 1) > ns )
	{
		ns = packet_sequence_number + 1; 
	}
}

void update_nr( int packet_sequence_number )
{
	/* Locking nr */
	printf("MAIN PROCESS : Locking recieve buffer..\n");

	if ( ( n = pthread_mutex_lock( &nr_mutex ) ) != 0)
		errno = n, err_sys("pthread_mutex_lock error");
	
	if( packet_sequence_number == nr )
	{
		nr = nr + 1;
	//	while( ( rwnd[nr].msg_iovlen != NULL ) || nr < ns )
		while( nr < ns && ( rwnd1[nr%reciever_window_size].data[0] != 0 ) )
		{
			nr++;
		}
	}

	if ( (n = pthread_mutex_unlock(&nr_mutex)) != 0 )
		errno = n, err_sys( "pthread_mutex_unlock error" );
	printf("MAIN PROCESS : Unlocking recieve buffer..\n");

	/* Unlocking nr */
}

void check_for_packet_drop( double datagram_loss_probability )
{
	double value;
	value = rand()%100 / 100.0;
	printf("PROBABILITY VALUE IS : %f\n", value );	
	if( value < datagram_loss_probability )
	{
		packet_drop = 1;
	}
	else
	{
		packet_drop = 0;
	}		
	return;
}

ssize_t dg_recieve( int fd, void *inbuff, size_t inbytes )
{
	ssize_t			n;
	struct iovec	iovrecv[2];
	char 			IPClient[20];
	char			output[MAXLINE];

	memset( &msgrecv, '\0', sizeof(msgrecv) ); 
	memset( &recvhdr, '\0', sizeof(recvhdr) ); 
	
	msgrecv.msg_name = NULL;
	msgrecv.msg_namelen = 0;
	msgrecv.msg_iov = iovrecv;
	msgrecv.msg_iovlen = 2;
	iovrecv[0].iov_base = &recvhdr;
	iovrecv[0].iov_len = sizeof(struct hdr);
	iovrecv[1].iov_base = inbuff;
	iovrecv[1].iov_len = inbytes;

	/* this has to be changed  */	
	if( n = Recvmsg( fd, &msgrecv, 0) < 0 ) 
	{
		printf("Error in RecvMsg!!\n");
		printf("Error no : %d\n", errno );
		exit(0);
	}

	if( packet_drop == 1 )
	{
		printf("Dropping recieved packet, SEQUENCE NUMBER : %d..\n", recvhdr.seq );
	}	
	else if( recvhdr.seq == -1 )
	{
		printf("Recieved a probing packet from server..\n");
	}	
	else if( (recvhdr.seq >= nr) 
			&& (recvhdr.seq < (nr + reciever_window_size ) ) )
	{
		printf("Recieved Packet with packet_sequence_number : %d from server..\n", recvhdr.seq );	
		printf( "Adding the packet to the receive buffer at %dth position..\n", (recvhdr.seq)%reciever_window_size );
		strcpy( rwnd1[ (recvhdr.seq)%reciever_window_size ].data, inbuff );

		update_ns( recvhdr.seq );
		update_nr( recvhdr.seq );
	}
	else
	{
		printf("Recieved Packet sequence # out of range : seq# :%d..\n", recvhdr.seq );
	}	

	return (1);

//	return ( n- sizeof(struct hdr) );
}

ssize_t dg_send_ack( int fd )
{
	ssize_t			n;
	struct iovec	iovrecv[2];
	char 			IPClient[20];

	memset( &msgrecv, '\0', sizeof( msgrecv ) ); 
	memset( &recvhdr, '\0', sizeof( recvhdr ) ); 
	
	printf("Sending ACK-%d to server..\n", nr );	
	/* Locking nr */

	printf("MAIN PROCESS : Locking recieve buffer..\n");
	if ( ( n = pthread_mutex_lock( &nr_mutex ) ) != 0)
		errno = n, err_sys("pthread_mutex_lock error");
	
	recvhdr.ack_no = nr;
	recvhdr.recv_window_advertisement = reciever_window_size - (nr - consumed - 1 ) ;
        if(recvhdr.recv_window_advertisement==0)
        {
                printf( "\n*********************BUFFER FULL*************************\n");
        }

	if ( (n = pthread_mutex_unlock(&nr_mutex)) != 0 )
		errno = n, err_sys( "pthread_mutex_unlock error" );

	/* Unlocking nr */
	printf("MAIN PROCESS : Unlocking recieve buffer..\n");

	msgrecv.msg_name = NULL;
	msgrecv.msg_namelen = 0;
	msgrecv.msg_iov = iovrecv;
	msgrecv.msg_iovlen = 1;
	iovrecv[0].iov_base = &recvhdr;
	iovrecv[0].iov_len = sizeof( struct hdr );

	if( packet_drop == 0 )
	{	
		if( n = sendmsg( fd, &msgrecv, 0) < 0 ) 
		{
			printf("Error in SendMsg!!\n");
		}	
	}	
	else
	{
		printf("Dropping ACK packet : ACK-%d..\n", recvhdr.ack_no );
	}	

	return ( n );
}

void status_print()
{
	printf("***********************************************************\n");
	printf("STATUS PRINT\n");
	printf("***********************************************************\n");
	printf("Ns  : %d\n",ns );
	printf("Consumed : %d\n",consumed );
	printf("***********************************************************\n");
}

void delete_datasegment( int consumed )
{
	printf("Deleting the consumed segment..\n");
	rwnd1[ consumed%reciever_window_size ].data[0]='\0' ;
}

void * recv_consumer( void *ptr )
{
	char output[MAXLINE]; 
	double sleep_time;
		
	while(1)
	{
		printf("CONSUMER THREAD : Locking recieve buffer..\n");
	 	if ( ( n = pthread_mutex_lock( &nr_mutex ) ) != 0)
			errno = n, err_sys("pthread_mutex_lock error");
		
		while( consumed != (nr - 1) )
		{
			consumed++;
			//Consume packet 
			if( rwnd1[ consumed%reciever_window_size ].data[0] != '\0' )
			{
				sprintf( output, "%s\n", rwnd1[ consumed%reciever_window_size ].data );
				printf( "Deleted segment '%d': %s\n", consumed, output );	
				delete_datasegment( consumed );
			}
			
		}
		if ( (n = pthread_mutex_unlock(&nr_mutex)) != 0 )
			errno = n, err_sys( "pthread_mutex_unlock error" );
		printf("CONSUMER THREAD : Unlocking recieve buffer..\n");


		sleep_time = ( rand() % 100 ) / 100.0;
		sleep_time = log( sleep_time );	
		sleep_time = -1*mu*sleep_time ;
		usleep( sleep_time*1000 );
	
	}

	return(NULL);
}

void dg_cli1( FILE *fp, int sockfd, const SA *pservaddr, socklen_t servlen, config configdata[] )
{
	int 					size, seed_value;
	char 					sendline[MAXLINE], recvline[MAXLINE + 1];
	ssize_t					n;
	size_t 					inbytes;
	socklen_t 				slen;
	struct sockaddr_in      ss;
	char 					IPServer[20];	
	double 					datagram_loss_probability;

	seed_value =  (int) atoi( configdata[4].data );
	mu = (double) atof( configdata[6].data );
	datagram_loss_probability = (double) atof( configdata[5].data );

	srand( seed_value );

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
	printf("\n**********************************************\n");

	printf( "        SERVER INFO \n" );
	printf("**********************************************\n");

	printf( "IP : %s\n",IPServer);
    printf( "SERVER PORT : %d\n", ntohl(ss.sin_port) );
        	
	size = 70000;
	
	setsockopt( sockfd, SOL_SOCKET, SO_SNDBUF, &size, sizeof( size ) );
	setsockopt( sockfd, SOL_SOCKET, SO_RCVBUF, &size, sizeof( size ) );
	char hsMsg[PACKET_SIZE];	
	sprintf(hsMsg,"%s %s",configdata[2].data,configdata[3].data);	
	write( sockfd, hsMsg, strlen( hsMsg ) + 1 );
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

	// Create thread just before the first receive 
	if ( (n = pthread_create(&tid, NULL, recv_consumer, &val)) != 0)
		errno = n, err_sys("pthread_create error");

	rwnd1 = malloc( reciever_window_size*sizeof(receive_buffer) );
	int i;
	for( i=0 ; i<reciever_window_size; i++)
	{
			rwnd1[i].data[0] = '\0';
	}	

	check_for_packet_drop( datagram_loss_probability );	
	memset( recvline, '\0', sizeof( recvline ) );

	while( ( n = dg_recieve( sockfd, recvline, MAXLINE ) ) > 0 )
	{
		if( packet_drop == 0 )
		{
			printf("************************************************\n");
			printf( "Recieved message of %d bytes..\n", n );	
			printf( "RECIEVED MESSAGE : %s\n",recvline );	
			printf("************************************************\n");

			printf("Attempting to send an ACK..\n");
			status_print();
			
			check_for_packet_drop( datagram_loss_probability );
			dg_send_ack( sockfd );
			
			status_print();
		}	

		memset( recvline, '\0', sizeof( recvline ) );
		check_for_packet_drop( datagram_loss_probability );
	}	
}
