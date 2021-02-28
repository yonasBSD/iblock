#include <stdlib.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <stdio.h>
#include <err.h>
#include <netdb.h>
#include <unistd.h>
#include <syslog.h>

int main(void){
        struct sockaddr sock;
        socklen_t slen = sizeof(sock);
        char host[1024] = "";
        char port[1044] = "";
        int status;

	unveil("/sbin/pfctl", "rx");
	pledge("exec inet dns stdio", NULL);

        if(getpeername(0, &sock, &slen))
            err(1, "getpeername");

        status = getnameinfo(&sock, slen, host, sizeof host, port, sizeof port,
                        NI_NUMERICHOST|NI_NUMERICSERV);
	if(status != 0)
	{
            syslog(LOG_DAEMON, "getnameinfo error");
            exit(1);
	}

	syslog(LOG_DAEMON, "blocking %s", host);
        switch(sock. sa_family)
        {
            case AF_INET:
                    execlp("/sbin/pfctl", "pfctl", "-t", "blocked", "-T", "add", host, NULL);
                    break;
            // case AF_INET6:
            //         printf("%s %s\n", host, cmd);
            //         break;
            default:
                    exit(2);
                    //puts("run from console");
        }
}

