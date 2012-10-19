#include	"unpifiplus.h"

#undef MAXLINE
#define MAXLINE 65507

typedef struct {
        struct sockaddr  *ip_addr;   /* IP address bound to socket */
        int    sockfd;   /* socket descriptor */
        struct sockaddr  *ntmaddr;  /* netmask address for IP */
        struct sockaddr  *subnetaddr;  /* netmask address */

}socket_info;

int
main(int argc, char **argv)
{
	int					sockfd[10];
	socklen_t			len;
	struct sockaddr_in	cliaddr, servaddr;
	const int                       on = 1;
        pid_t                           pid;
        struct ifi_info         *ifi, *ifihead;
        struct sockaddr_in      *sa;

	char            dataline[MAXLINE];
	char *configdata[7];
        socket_info sockinfo[10];

	char  IPServer[20],IPClient[20];   /* IP address bound to socket */
	int sockcount=0;
	
	if (argc != 2)
		err_quit("usage: udpcli <IPaddress>");
	
	FILE *ifp;
	char *mode = "r";
	ifp = fopen("client.in", mode);

	if (ifp == NULL) {
		fprintf(stderr, "Can't open input file client.in!\n");
		exit(1);
	}
	
	int countline=0;
	int n=0;
	while (fgets(dataline,MAXLINE,ifp)!=NULL)
	{
		n=strlen(dataline);	
		configdata[countline]=dataline;	
		dataline[n]=0;
		fputs(dataline,stdout);
		printf("reading from config %s \n", configdata[countline]);
		countline++;	
		}	
       	printf("read %d lines\n", countline);
fclose(ifp);
    sprintf(IPServer,"%s",configdata[0]);
 
	   for (ifihead = ifi = get_ifi_info_plus(AF_INET, 1);
                 ifi != NULL; ifi = ifi->ifi_next) {

        sockfd[sockcount]=-1;

      //  if((    sockfd[sockcount]= socket(AF_INET, SOCK_DGRAM, 0))==NULL)
       // {printf("socket error\n");
        //        exit(1);
       // }       setsockopt(sockfd[sockcount], SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

                sa = (struct sockaddr_in *) ifi->ifi_addr;
                sa->sin_family = AF_INET;
                sa->sin_port = htons((size_t)configdata[1]);
               inet_pton(AF_INET, argv[1], &servaddr.sin_addr);
 
                sockinfo[sockcount].sockfd=sockfd[sockcount];
                sockinfo[sockcount].ip_addr=(SA *)sa;
                sockinfo[sockcount].ntmaddr=ifi->ifi_ntmaddr;
                //sockinfo[sockcount].subnetaddr=ifi->sockfd[sockcount];
printf("sockfd----- %d\n", sockinfo[sockcount].sockfd);

printf("  IP addr: %s\n",
                                                sock_ntop_host(sockinfo[sockcount].ip_addr, sizeof(*sockinfo[sockcount].ip_addr)));

printf("net addr: %s\n",
                                                sock_ntop_host(sockinfo[sockcount].ntmaddr, sizeof(*sockinfo[sockcount].ntmaddr)));

                sockcount++;

}
int x=0;
if(strcmp(IPServer,"127.0.0.1\n")==0);
printf("voilaa!\n");
for (x=0;sockinfo[x].sockfd!=NULL;x++)
{
printf(" check  IP addr: %s\n",
                                                sock_ntop_host(sockinfo[x].ip_addr, sizeof(*sockinfo[x].ip_addr)));

	
	if(strcmp(sock_ntop_host(sockinfo[x].ip_addr, sizeof(*sockinfo[x].ip_addr)),"127.0.0.1\n")==0)
	{
	printf("match found\n");	
	}
}
	sockfd[0] = socket(AF_INET, SOCK_DGRAM, 0);

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons((size_t)configdata[1]);
	inet_pton(AF_INET, configdata[0], &servaddr.sin_addr);

	//sockfd = socket(AF_INET, SOCK_DGRAM, 0);

        dg_cli(stdin, sockfd[0], (SA *) &servaddr, sizeof(servaddr));

	exit(0);
}

void
dg_cli(FILE *fp, int sockfd, const SA *pservaddr, socklen_t servlen)
{
        int                     size;
        char            sendline[MAXLINE], recvline[MAXLINE + 1];
        ssize_t         n;
connect(sockfd,pservaddr,servlen);
        size = 70000;
        setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size));
        setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
while (fgets(sendline,MAXLINE,fp)!=NULL)
{
printf("1\n");
        //sendto(sockfd, sendline, MAXLINE, 0, pservaddr, servlen);
write(sockfd,sendline,strlen(sendline));
	printf("1\n");
n=read(sockfd,recvline,MAXLINE);
        //n = recvfrom(sockfd, recvline, MAXLINE, 0, NULL, NULL);
printf("1\n");

	recvline[n]=0;
       fputs(recvline,stdout);
	printf("received %d bytes\n", n);
}
}

