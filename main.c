#include <stdlib.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <stdio.h>
#include <err.h>
#include <netdb.h>

int main(void){
        struct sockaddr sock;
        socklen_t slen = sizeof(sock);
        char host[1024] = "";
        char port[1044] = "";
        int status;

        if(getpeername(0, &sock, &slen))
            err(1, "getpeername");

        status = getnameinfo(&sock, slen, host, sizeof host, port, sizeof port,
                        NI_NUMERICHOST|NI_NUMERICSERV);
	if(status > 0)
	{
            fprintf(stderr, "getnameinfo error");
            exit(1);
	}

        switch(sock. sa_family)
        {
            case AF_INET:
                    printf("%s\n", host);
                    break;
            case AF_INET6:
                    printf("%s\n", host);
                    break;
            default:
                    puts("run from console");
        }
}

