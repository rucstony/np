#include	"unpifiplus.h"
#include	<string.h>
#include 	<arpa/inet.h>

#undef MAXLINE
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

	/*
	if(strcmp(IPServer,"127.0.0.1\n")==0)	
	strcpy(IPServer,"127.0.0.1\n");
	*/

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
			socketIndex=0;	
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

	bind( sockfd1, (SA *)&cliaddr, sizeof( cliaddr ) ); //causes kernal to bind an ephemeral port to the socket 
	printf( "bound....%d\n", sockfd1 );	

	printf( "IPClient123: %s\n", inet_ntop( AF_INET, &(cliaddr.sin_addr), IPClient, MAXLINE ) );
	
	slen = sizeof( ss );
	//to obtain IPClient and assigned ephemeral port number	
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
	

	//setting server struct
 	bzero( &servaddr, sizeof( servaddr ) );
	servaddr.sin_family = AF_INET;
//	servaddr.sin_port = htonl(configdata[1].data); //assigning server port from client.in;

	servaddr.sin_port = htonl(12345); //assigning server port from client.in;
	inet_pton( AF_INET, IPServer, &servaddr.sin_addr );

	printf(" calling dg_cli\n");	
   	dg_cli1( stdin, sockfd1, (SA *)&servaddr, sizeof( servaddr ), configdata );

	exit(0);
}

void dg_cli1( FILE *fp, int sockfd, const SA *pservaddr, socklen_t servlen, config configdata[] )
{
	int 					size;
	char 					sendline[MAXLINE], recvline[MAXLINE + 1];
	ssize_t					n;
	socklen_t 				slen;
	struct sockaddr_in      ss;
	char 					IPServer[20];	
	

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
	
	//while ( fgets( sendline, MAXLINE, fp ) != NULL )
	//{
		/*
		sendto( sockfd, sendline, MAXLINE, 0, pservaddr, servlen );
		*/
		write( sockfd, configdata[2].data, strlen( configdata[2].data ) + 1 );
		n = read( sockfd, recvline, MAXLINE );
		/*
		n = recvfrom( sockfd, recvline, MAXLINE, 0, NULL, NULL );
		*/
		printf( "RECIEVED DATAGRAM\n" );
		
		recvline[ n ] = 0;
		fputs( recvline, stdout );
		printf( "Ephemeral Port Number Of Server Child : %s \n", recvline );
		
	ss.sin_port = htonl( (uint16_t) atoi( recvline ) ); //assigning server port from client.in;

		printf( "PORT NUMBER USED TO RECONNECT : %d\n", pservaddr.sin_port );
	
	if( connect( sockfd, pservaddr, servlen ) < 0 )
	{
		printf( "Error in connecting to server..\n" );
		exit(1);
	}	
	//}
}




