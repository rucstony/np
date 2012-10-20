#include	"unpifiplus.h"
#include	<string.h>

#undef MAXLINE
#define MAXLINE 65507
typedef char* string; 

typedef struct 
{
	struct sockaddr  *ip_addr;   /* IP address bound to socket */
	int    sockfd;   /* socket descriptor */
	struct sockaddr  *ntmaddr;  /* netmask address for IP */
	struct sockaddr  *subnetaddr;  /* netmask address */
}socket_info;

typedef struct 
{
	char data[MAXLINE];
}config;

int main( int argc, char **argv )
{
	int 		 			sockfd[10], n = 0, j = 0, countline = 0;
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
		sockinfo[sockcount].ip_addr=(SA *)sa;
		sockinfo[sockcount].ntmaddr=ifi->ifi_ntmaddr;
		/* 
		sockinfo[sockcount].subnetaddr=ifi->sockfd[sockcount];
		*/
		printf("sockfd----- %d\n", sockinfo[sockcount].sockfd);
		printf("IP addr: %s\n", 
				sock_ntop_host( sockinfo[ sockcount ].ip_addr, sizeof( *sockinfo[ sockcount ].ip_addr ) ) ); 
		printf("net addr: %s\n",
        		sock_ntop_host( sockinfo[ sockcount ].ntmaddr, sizeof( *sockinfo[ sockcount ].ntmaddr ) ) );
                sockcount++;
	}
	int x = 0;
	/*
	if(strcmp(IPServer,"127.0.0.1\n")==0)	
	strcpy(IPServer,"127.0.0.1\n");
	*/
	int flag = 0;
	for ( x = 0; sockinfo[x].sockfd!=NULL; x++ )
	{
		printf( "check  IP addr: %s\n",
				sock_ntop_host( sockinfo[x].ip_addr, sizeof( *sockinfo[x].ip_addr ) ) );
		printf( "check  configdata[0].data: %s\n",
    			configdata[0].data );

	
		if( strcmp( sock_ntop_host( sockinfo[x].ip_addr, sizeof( *sockinfo[x].ip_addr ) ), configdata[0].data ) == 0 )
		{
			strcpy( IPServer, "127.0.0.1\n" );
			strcpy( IPClient, "127.0.0.1\n" );
			flag = 1;
			printf( "match found---same host---use loop back %s %s \n", IPServer , IPClient );

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
		}
	}
	if( !flag )
	{
		printf( "long prefix match logic comes here...\n" );
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
		n=read(sockfd,recvline,MAXLINE);
		/*
		n = recvfrom( sockfd, recvline, MAXLINE, 0, NULL, NULL );
		*/
		recvline[ n ] = 0;
		fputs( recvline, stdout );
		printf( "received %d bytes\n", n );
	}
}

