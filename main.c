#include <sys/socket.h>
#include <sys/wait.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <err.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#define PORT "2507"
#define BACKLOG 42
#define DEFAULT_TABLE "iblocked"

static void *get_in_addr(struct sockaddr *);
static void runcmd(const char*, const char**);
static void sigchld(int unused);
static void usage(void);


static void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET)
		return &(((struct sockaddr_in*)sa)->sin_addr);

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

static void runcmd(const char* cmd, const char** arg_list)
{
	pid_t pid = fork();
	if (pid == -1) {
		syslog(LOG_DAEMON, "fork error");
		err(1,"fork");
	} else if (pid == 0) {	/* child */
		execv(cmd, (char **)arg_list);
		/* if this is reached, then exec failed */
		syslog(LOG_DAEMON, "execv error");
		err(1,"execv");
	}
}

void
sigchld(int unused)
{
	(void)unused;
	if (signal(SIGCHLD, sigchld) == SIG_ERR)
		err(1, "can't install SIGCHLD handler:");
	while (waitpid(WAIT_ANY, NULL, WNOHANG) > 0);
}

static void usage(void)
{
	fprintf(stderr, "usage: %s [table]\n", getprogname());
	exit(1);
}

int
main(int argc, char *argv[])
{
	char ip[INET6_ADDRSTRLEN]	= {'\0'};
	const char *table	        = DEFAULT_TABLE;
	int sockfd 	                = 0;
	int new_fd 	                = 0;
	int retval 	                = 0;
	socklen_t sin_size 	        = 0;
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage client_addr;
	const char *bancmd[]	        = { "/usr/bin/doas", "-n",
				            "/sbin/pfctl", "-t", table,
				            "-T", "add", ip,
				            NULL };
	const char *killstatecmd[]	= { "/usr/bin/doas", "-n",
					    "/sbin/pfctl",
					    "-k", ip,
					    NULL };


	if (unveil("/usr/bin/doas", "rx") != 0)
		err(1, "unveil");
	if (pledge("stdio inet exec proc rpath", NULL) != 0)
		err(1, "pledge");

	if (argc > 2)
		usage();
	else if (argc == 2)
		table = argv[1];

	/* initialize structures */
	memset(&client_addr, 0, sizeof(client_addr));
	memset(&hints, 0, sizeof(hints));

	/* set hints for socket */
	hints.ai_family = AF_UNSPEC; /* ip4 or ip6 */
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if ((retval = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
		syslog(LOG_DAEMON, "getaddrinfo failed");
		err(1, "getaddrinfo :%s", gai_strerror(retval));
	}

	/* get a socket and bind */
	for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family,
					p->ai_socktype,
					p->ai_protocol)) == -1) {
			continue;
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			continue;
		}

		break;
	}

	freeaddrinfo(servinfo);

	if (p == NULL) {
		syslog(LOG_DAEMON, "Failed to bind");
		err(1, "Failed to bind");
	}

	if (listen(sockfd, BACKLOG) == -1) {
		syslog(LOG_DAEMON, "listen failed");
		err(1, "listen");
	}

	sigchld(0);

	syslog(LOG_DAEMON, "ready to reap on port %s, muhahaha :>", PORT);

	while (1) {
		sin_size = sizeof(client_addr);
		new_fd = accept(sockfd,
				(struct sockaddr*)&client_addr,
				&sin_size);

		if (new_fd == -1)
			continue;

		/* get client ip */
		inet_ntop(client_addr.ss_family,
			get_in_addr((struct sockaddr *)&client_addr),
			ip, sizeof(ip));

		close(new_fd); /* no longer needed */

		pid_t id = fork();
		if (id == -1) {
			syslog(LOG_DAEMON, "fork error");
			err(1, "fork");
		} else if (id == 0) { /* child process */
			syslog(LOG_DAEMON, "blocking %s", ip);
			runcmd(bancmd[0], bancmd);
			syslog(LOG_DAEMON, "kill states for %s", ip);
			runcmd(killstatecmd[0], killstatecmd);
			close(sockfd);
			exit(0);
		}
	}
	close(sockfd);
	return 0;
}
