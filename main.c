#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <syslog.h>
#include <sys/wait.h>
#include <unistd.h>

#include <sys/socket.h>

#define DEFAULT_TABLE "iblocked"
#define TABLE_LEN 32 /* see PF_TABLE_NAME_SIZE in net/pfvar.h */

int main(int argc, char *argv[]){
	struct sockaddr_storage sock = {0};
	socklen_t slen = sizeof(sock);
	char ip[INET6_ADDRSTRLEN] = {'\0'}; /* INET6_ADDRSTRLEN > INET_ADDRSTRLEN */
	char table[TABLE_LEN] = DEFAULT_TABLE;
	int status = 0;
	pid_t id;

	if (unveil("/usr/bin/doas", "rx") != 0)
		err(1, "unveil");
	if (pledge("exec inet proc stdio", NULL) != 0)
		err(1, "pledge");

	/* configuration */
	if (argc == 2)
		if (strlcpy(table, argv[1], TABLE_LEN) >= sizeof(table))
			errx(1, "table name is too long");

	/* get socket structure */
	if (getpeername(STDIN_FILENO, (struct sockaddr *)&sock, &slen))
		err(1, "getpeername");

	/* get ip */
	status = getnameinfo((struct sockaddr *)&sock, slen, ip, sizeof(ip),
						  NULL, 0, NI_NUMERICHOST);

	if (status != 0) {
		syslog(LOG_DAEMON, "getnameinfo error");
		exit(1);
	}

	switch (sock.ss_family) {
	case AF_INET: /* FALLTHROUGH */
	case AF_INET6:
		id = fork();

		if (id == -1) {
			syslog(LOG_DAEMON, "fork error");
			exit(1);
		} else if (id == 0) {
			// child process
			syslog(LOG_DAEMON, "blocking %s", ip);
			execl("/usr/bin/doas", "doas", "/sbin/pfctl",
				"-t", table, "-T", "add", ip, NULL);

		} else {
			// parent process
			wait(NULL);
			syslog(LOG_DAEMON, "kill states for %s", ip);
			execl("/usr/bin/doas", "doas", "/sbin/pfctl",
				"-k", ip, NULL);
		}
		break;
	default:
		exit(2);
	}
}
