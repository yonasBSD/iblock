#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <syslog.h>
#include <unistd.h>

#include <sys/socket.h>

#define DEFAULT_TABLE "iblocked"
#define TABLE_LEN 32 /* see PF_TABLE_NAME_SIZE in net/pfvar.h */

int main(int argc, char *argv[]){
	struct sockaddr_storage sock;
	socklen_t slen = sizeof(sock);
	char ip[INET6_ADDRSTRLEN] = {'\0'}; /* INET6_ADDRSTRLEN > INET_ADDRSTRLEN */
	char table[TABLE_LEN] = DEFAULT_TABLE;
	int status;

	if (unveil("/usr/bin/doas", "rx") != 0)
		err(1, "unveil");
	if (pledge("exec inet stdio", NULL) != 0)
		err(1, "pledge");

	/* configuration */
	if (argc == 2) {
		if (strlen(argv[1]) > sizeof(table))
			errx(1, "table name is too long");
		strlcpy(table, argv[1], TABLE_LEN);
	}

	/* get socket structure */
	if(getpeername(STDIN_FILENO, (struct sockaddr *)&sock, &slen))
		err(1, "getpeername");

	/* get ip */
	status = getnameinfo((struct sockaddr *)&sock, slen, ip, sizeof(ip),
						  NULL, 0, NI_NUMERICHOST);

	if(status != 0) {
		syslog(LOG_DAEMON, "getnameinfo error");
		exit(1);
	}

	syslog(LOG_DAEMON, "blocking %s", ip);
	switch(sock.ss_family) {
	case AF_INET: /* FALLTHROUGHT */
	case AF_INET6:
		execlp("/usr/bin/doas", "doas", "/sbin/pfctl", "-t", table, "-T", "add", ip, NULL);
		break;
	default:
		exit(2);
	}
}

