#include <sys/types.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <net/if.h>
#include <net/pfvar.h>

#include <err.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#define DEFAULT_TABLE "iblocked"
#define DEFAULT_PORT "2507"
#define MAXSOCK 2 /* ipv4 + ipv6 */
#define BACKLOG 10

static void *get_in_addr(struct sockaddr *);
static void runcmd(const char*, const char**);
static int setup_server(const char*, int *);
static void usage(void);
static void watch_event(const int, const int *, const char *);


/* return printable ip from sockaddr */
static void
*get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET)
		return &(((struct sockaddr_in*)sa)->sin_addr);

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/* run cmd in execv() after fork() */
static void
runcmd(const char* cmd, const char** arg_list)
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
		waitpid(pid, NULL, 0);
	}
}

static int
setup_server(const char *port, int s[])
{
	int nsock 	               		= 0;
	char server_ip[INET6_ADDRSTRLEN]	= {'\0'};
	const char *err_cause	        	= NULL;
	struct addrinfo hints, *servinfo, *res;

	/* initialize structures */
	memset(&hints, 0, sizeof(hints));

	/* set hints for socket */
	hints.ai_family = AF_UNSPEC; /* ip4 or ip6 */
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	/* get ips for localhost */
	int retval = getaddrinfo("localhost", port, &hints, &servinfo);
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
			server_ip, sizeof(server_ip));
		syslog(LOG_DAEMON, "listening on %s port %s, muahaha :>",
			server_ip,
			port);

		nsock++;
	}

	/* clean up no longer used servinfo */
	freeaddrinfo(servinfo);

	if (nsock == 0)
		err(1, "Error when calling %s", err_cause);

	return nsock;
}

static void
usage(void)
{
	fprintf(stderr, "usage: %s (-t <table>) (-p <port>)\n",
		getprogname());
	exit(1);
}

static void
watch_event(const int nsock, const int s[], const char *table)
{
	int kq 				= 0;
	int new_fd 	                = 0;
	char ip[INET6_ADDRSTRLEN]	= {'\0'};
	struct kevent ev[MAXSOCK]	= {0};
	socklen_t sin_size 	        = 0;
	const char *bancmd[]	        = { "/usr/bin/doas", "-n",
				            "/sbin/pfctl", "-t", table,
				            "-T", "add", ip,
				            NULL };
	const char *killstatecmd[]	= { "/usr/bin/doas", "-n",
					    "/sbin/pfctl",
					    "-k", ip,
					    NULL };
	struct sockaddr_storage client_addr;


	/* initialize structures */
	memset(&client_addr, 0, sizeof(client_addr));

	/* configure events */
	kq = kqueue();

	/* add event for each IP */
	for (int i = 0; i < nsock; i++)
		EV_SET(&(ev[i]), s[i],
			EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, 0);

	/* register event */
	if (kevent(kq, ev, MAXSOCK, NULL, 0, NULL) == -1)
		err(1, "kevent register");

	/* infinite loop to wait for connections */
	for (;;) {
		int nevents = kevent(kq, NULL, 0, ev, MAXSOCK, NULL);
		if (nevents == -1)
			err(1, "kevent get event");

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

	/* probably never reached, but close properly */
	close(kq);
}

int
main(int argc, char *argv[])
{
	char table[PF_TABLE_NAME_SIZE]  = DEFAULT_TABLE;
	char port[6] 			= DEFAULT_PORT;
	int nsock 	                = 0;
	int option	                = 0;
	int s[MAXSOCK]			= {0};


	while ((option = getopt(argc, argv, "t:p:")) != -1) {
		switch (option) {
		case 'p':
			if (strlcpy(port, optarg, sizeof(port)) >=
				sizeof(port))
					err(1, "invalid port");
			break;
		case 't':
			if (strlcpy(table, optarg, sizeof(table)) >=
				sizeof(table))
					err(1, "table name too long");
			break;
		default:
			usage();
			break;
		}
	}


	/* safety first */
	if (unveil("/usr/bin/doas", "rx") != 0)
		err(1, "unveil");
	/* necessary to resolve localhost with getaddrinfo() */
	if (unveil("/etc/hosts", "r") != 0)
		err(1, "unveil");
	if (pledge("stdio inet exec proc rpath", NULL) != 0)
		err(1, "pledge");

	nsock = setup_server(port, s);
	watch_event(nsock, s, table);

	/* probably never reached, but close properly */
	for (int i = 0; i < nsock; i++)
		close(s[i]);
	return 0;
}
