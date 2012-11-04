CC = gcc

LIBS =  -lsocket\
        /home/courses/cse533/Stevens/unpv13e_solaris2.10/libunp.a

FLAGS =  -g -O2
CFLAGS = ${FLAGS} -I/home/courses/cse533/Stevens/unpv13e_solaris2.10/lib

PROGS	=	client server

all:	client server \
#	get_ifi_info_plus.o prifinfo_plus.o
#	${CC} -o prifinfo_plus.o get_ifi_info_plus.o ${LIBS}

get_ifi_info_plus.o: get_ifi_info_plus.c
	${CC} ${CFLAGS} -c get_ifi_info_plus.c

prifinfo_plus.o: prifinfo_plus.c
	${CC} ${CFLAGS} -c prifinfo_plus.c

client:	udpcli.o get_ifi_info_plus.o
	${CC} ${CFLAGS} -o $@ get_ifi_info_plus.o udpcli.o ${LIBS} -lm

server:	rtt_plus.o udpserv.o get_ifi_info_plus.o 
		${CC} ${CFLAGS} -o $@  get_ifi_info_plus.o udpserv.o rtt_plus.o ${LIBS}

rtt_plus.o:	rtt_plus.c
	${CC} ${CFLAGS} -c rtt_plus.c 

clean:
		rm -f prifinfo_plus prifinfo_plus.o get_ifi_info_plus.o rtt_plus.o udpcli.o udpserv.o udpcli udpserv ${PROGS} ${CLEANFILES}
