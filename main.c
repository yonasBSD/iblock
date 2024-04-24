#include <sys/types.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <err.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#define DEFAULT_TABLE "iblocked"
#define PORT "2507"
#define MAXSOCK 2 /* ipv4 + ipv6 */
#define BACKLOG 10

static void *get_in_addr(struct sockaddr *);
static void runcmd(const char*, const char**);
static void usage(void);


/* return printable ip from sockaddr */
static void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET)
		return &(((struct sockaddr_in*)sa)->sin_addr);

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/* run cmd in execv() after fork() */
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
	} else { /* parent */
		waitpid(pid, NULL, WNOHANG);
	}
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
	const char *err_cause	        = NULL;
	int new_fd 	                = 0;
	int nsock 	                = 0;
	int kq	                        = 0;
	socklen_t sin_size 	        = 0;
	int s[MAXSOCK]			= {0};
	struct kevent ev[MAXSOCK]	= {0};
	struct addrinfo hints, *servinfo, *res;
	struct sockaddr_storage client_addr;

	if (argc > 2)
		usage();
	else if (argc == 2)
		table = argv[1];

	const char *bancmd[]	        = { "/usr/bin/doas", "-n",
				            "/sbin/pfctl", "-t", table,
				            "-T", "add", ip,
				            NULL };
	const char *killstatecmd[]	= { "/usr/bin/doas", "-n",
					    "/sbin/pfctl",
					    "-k", ip,
					    NULL };

	/* safety first */
  /*
	if (unveil("/usr/bin/doas", "rx") != 0)
		err(1, "unveil");
  */
	/* necessary to resolve localhost with getaddrinfo() */
  /*
	if (unveil("/etc/hosts", "r") != 0)
		err(1, "unveil");
	if (pledge("stdio inet exec proc rpath", NULL) != 0)
		err(1, "pledge");
  */

	/* initialize structures */
	memset(&client_addr, 0, sizeof(client_addr));
	memset(&hints, 0, sizeof(hints));

	/* set hints for socket */
	hints.ai_family = AF_UNSPEC; /* ip4 or ip6 */
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	/* get ips for localhost */
	int retval = getaddrinfo("localhost", PORT, &hints, &servinfo);
	if (retval != 0) {
		syslog(LOG_DAEMON, "getaddrinfo failed");
		err(1, "getaddrinfo :%s", gai_strerror(retval));
	}

	/* create sockets and bind for each local ip, store them in s[] */
	for (res = servinfo; res && nsock < MAXSOCK; res = res->ai_next) {

		s[nsock] = socket(res->ai_family,
				res->ai_socktype,
				res->ai_protocol);
		if (s[nsock] == -1) {
			err_cause = "socket";
			continue;
		}
		/* make sure PORT can be reused by second IP */
		int yes = 1;
		if (setsockopt(s[nsock], SOL_SOCKET, SO_REUSEPORT, &yes,
			sizeof(int)) == -1)
			err(1, "setsockopt");

		if (bind(s[nsock], res->ai_addr, res->ai_addrlen) == -1) {
			close(s[nsock]);
			err_cause = "bind()";
			continue;
		}

		if (listen(s[nsock], BACKLOG) == -1)
			err_cause = "listen";

		/* log the obtained ip */
		inet_ntop(res->ai_family,
			get_in_addr((struct sockaddr *)res->ai_addr),
			ip, sizeof(ip));
		syslog(LOG_DAEMON, "listening on %s port %s, muahaha :>",
			ip,
			PORT);

		nsock++;
	}

	/* clean up no longer used servinfo */
	freeaddrinfo(servinfo);

	if (nsock == 0)
		err(1, "Error when calling %s", err_cause);

	/* configure events */
	kq = kqueue();

	/* add event for each IP */
	for (int i = 0; i <= nsock; i++)
		EV_SET(&(ev[i]), s[i], EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, 0);

	/* register event */
	if (kevent(kq, ev, MAXSOCK, NULL, 0, NULL) == -1)
		err(1, "kevent");

	/* infinite loop to wait for connections */
	for (;;) {
		int nevents = kevent(kq, NULL, 0, ev, MAXSOCK, NULL);
		if (nevents == -1)
			err(1, "kevent");

		/* loop for events */
		for (int i = 0; i < nevents; i++) {

			if (ev[i].filter & EVFILT_READ) {

				/* get client ip */
				sin_size = sizeof(client_addr);
				new_fd = accept(ev[i].ident,
					(struct sockaddr*)&client_addr,
					&sin_size);
				if (new_fd == -1)
					continue;
				inet_ntop(client_addr.ss_family,
					get_in_addr((struct sockaddr *)&client_addr),
					ip, sizeof(ip));

				close(new_fd); /* no longer required */

				/* ban this ip */
				syslog(LOG_DAEMON, "blocking %s", ip);
				runcmd(bancmd[0], bancmd);
				syslog(LOG_DAEMON, "kill states for %s", ip);
				runcmd(killstatecmd[0], killstatecmd);
			}
			if (ev[i].filter & EVFILT_SIGNAL) {
				break;
			}
		} /* events loop */
	} /* infinite loop */

	/* probably never reached */
	close(kq);
	for (int i = 0; i <= nsock; i++)
		close(s[i]);
	return 0;
}
