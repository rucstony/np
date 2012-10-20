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

int main( int argc, char **argv )
{
	int 		 			sockfd[10], n = 0, j = 0, countline = 0, x = 0, loopback = 0 ;
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
	char 					*mode = "r";
	
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
	printf( "IPServer:%s\n", IPServer );

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
		sa->sin_port = htons( ( size_t )configdata[1].data );
		inet_pton( AF_INET, configdata[0].data, &servaddr.sin_addr );
 
		sockinfo[sockcount].sockfd=-1;
		sockinfo[sockcount].ip_addr=sa;
		sockinfo[sockcount].ntmaddr=(struct sockaddr_in *)ifi->ifi_ntmaddr;
		/* 
		sockinfo[sockcount].subnetaddr=ifi->sockfd[sockcount];
		*/
		printf("sockfd----- %d\n", sockinfo[sockcount].sockfd);
	/*	printf("IP addr: %s\n", 
				sock_ntop_host( sockinfo[ sockcount ].ip_addr, sizeof( *sockinfo[ sockcount ].ip_addr ) ) ); 
		printf("net addr: %s\n",
        		sock_ntop_host( sockinfo[ sockcount ].ntmaddr, sizeof( *sockinfo[ sockcount ].ntmaddr ) ) );
                sockcount++;
	*/
	}
	/*
	if(strcmp(IPServer,"127.0.0.1\n")==0)	
	strcpy(IPServer,"127.0.0.1\n");
	*/

	for ( x = 0; sockinfo[x].sockfd!=NULL; x++ )
	{
		if( strcmp( sock_ntop_host( (SA *)sockinfo[x].ip_addr, sizeof( *sockinfo[x].ip_addr ) ), configdata[0].data ) == 0 )
		{
			loopback = 1;
		
			strcpy( IPServer, "127.0.0.1\n" );
			strcpy( IPClient, "127.0.0.1\n" );
			printf( "Match found : same host : Using loopback..\n" );

			if( ( sockinfo[x].sockfd = socket( AF_INET, SOCK_DGRAM, 0 ) ) == NULL )
			{
				printf( "socket error\n" );
				exit(1);
		    }
			
			setsockopt( sockinfo[x].sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof( on ) );
			bzero( &servaddr, sizeof( servaddr ) );
		 
			servaddr.sin_family = AF_INET;
			servaddr.sin_port = 0;

			bind( sockinfo[x].sockfd, (SA *)&servaddr, sizeof( servaddr ) );
			printf( "bound....%d\n", sockinfo[x].sockfd );	
			
			if( getsockname( sockinfo[x].sockfd, (SA *)&ss, &slen ) < 0 )
			{
				printf( "sockname error\n" );
		        exit(1);
			}	
			/*
			printf("IPClient: %s\n",inet_ntop(AF_INET,ss.sin_addr.s_addr,str, sizeof(ss.sin_addr)));
			*/
			printf( "bound-port-%d-\n", ss.sin_port );

			printf( "socket bound: %d\n", sockinfo[x].sockfd );
			break;
		}
	}
	if( !loopback )
	{
		printf( "long prefix match logic comes here...\n" );
		/* Longest prefix matching on output from getifinfo_plus */
		struct in_addr ip;
		struct in_addr netmask;
		struct in_addr subnet;
		
		char temp1[MAXLINE];
		char temp2[MAXLINE];
		char *network;
		//We are converting the fucking ip_addr to string and ntm_addr to string.

		struct sockaddr_in *sd = (struct sockaddr_in *)(sockinfo[0].ip_addr);
		struct sockaddr_in *se = (struct sockaddr_in *)(sockinfo[0].ntmaddr);
		struct sockaddr_in *sf = (struct sockaddr_in *)(sockinfo[0].subnetaddr);

		ip = sd->sin_addr;
		netmask = se->sin_addr;
		subnet = sf->sin_addr;

//		inet_ntop( AF_INET, &sockinfo[0].ip_addr, ipa, MAXLINE );
//		inet_ntop( AF_INET, &sockinfo[0].ntmaddr, ntm, MAXLINE );

//		inet_aton( ipa, &ip );
 //	   	inet_aton( ntm, &netmask );
 		// bitwise AND of ip and netmask gives the network
	 	printf("HIIIIIII123\n");
   
 	   	subnet.s_addr = ip.s_addr & netmask.s_addr;
    	printf("HIIIIIII456\n");
    	
    	inet_ntop( AF_INET, &subnet, network, MAXLINE );
   		//printf("HIIIIIII789\n");
    	
    	sprintf( temp1, "%lu\n", net );
    	printf("%s\n", temp1 );
//		inet_aton( configdata[0].data, &ip );
 //	   	inet_aton( sockinfo[x].ntmaddr, &netmask );
 
    	// bitwise AND of ip and netmask gives the network
   // 	network.s_addr = ip.s_addr & netmask.s_addr;
    //	sprintf( temp1, "%s\n", inet_ntoa( network ) );
 	

	}
	sockfd[0] = socket( AF_INET, SOCK_DGRAM, 0 );

	bzero( &servaddr, sizeof( servaddr ) );
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons( ( size_t )configdata[1].data );
	inet_pton( AF_INET, configdata[0].data, &servaddr.sin_addr );
	/*
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	*/
    dg_cli( stdin, sockfd[0], (SA *)&servaddr, sizeof( servaddr ) );
	exit(0);
}

void dg_cli( FILE *fp, int sockfd, const SA *pservaddr, socklen_t servlen )
{
	int 		size;
	char 		sendline[MAXLINE], recvline[MAXLINE + 1];
	ssize_t		n;
	
	connect( sockfd, pservaddr, servlen );
	size = 70000;
	
	setsockopt( sockfd, SOL_SOCKET, SO_SNDBUF, &size, sizeof( size ) );
	setsockopt( sockfd, SOL_SOCKET, SO_RCVBUF, &size, sizeof( size ) );
	
	while ( fgets( sendline, MAXLINE, fp ) != NULL )
	{
		/*
		sendto( sockfd, sendline, MAXLINE, 0, pservaddr, servlen );
		*/
		write( sockfd, sendline, strlen( sendline ) );
		printf("1\n");
		n = read(sockfd,recvline,MAXLINE);
		/*
		n = recvfrom( sockfd, recvline, MAXLINE, 0, NULL, NULL );
		*/
		recvline[ n ] = 0;
		fputs( recvline, stdout );
		printf( "received %d bytes\n", n );
	}
}

