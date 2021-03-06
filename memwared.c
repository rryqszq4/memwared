/*
 * gcc -std=gnu99 -o memwared memwared.c -levent
 *
 *
 **/
#include "memwared.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sysexits.h>
#include <signal.h>
#include <unistd.h>
#include <limits.h>
#include <sys/resource.h>
#include <pwd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <pthread.h>
#include <bson.h>
#include <mongoc.h>
#include <msgpack.h>

/* defaults */
 static void settings_init(void);

/* event handling, network IO*/
void do_accept( int sfd, short event, void *arg);
void do_read(int sfd, short event, mw_conn *conn);
void do_write(int sfd, short event, mw_conn *mw_conn);
void *conn_work_process(void *data);
//void *mongo_clt_worker (void *data, char *res, char *db, char *table);
int *mongo_clt_worker (void *data);
static void conn_init(void);

void memwared_close(int sig);


/** exported globals **/
struct stats stats;
struct settings settings;


static struct event_base *main_base;
mongoc_client_pool_t *mongoc_pool;
mongoc_uri_t *mongoc_uri;
static int thread_number = 10;







static void settings_init(void) {
    settings.use_cas = true;
    settings.access = 0700;
    settings.port = 11211;
    settings.udpport = 11211;
    /* By default this string should be NULL for getaddrinfo() */
    settings.inter = NULL;
    settings.maxbytes = 64 * 1024 * 1024; /* default is 64MB */
    settings.maxconns = 1024;         /* to limit connections-related memory to about 5MB */
    settings.verbose = 0;
    settings.oldest_live = 0;
    settings.evict_to_free = 1;       /* push old items out of cache when memory runs out */
    settings.socketpath = NULL;       /* by default, not using a unix socket */
    settings.factor = 1.25;
    settings.chunk_size = 48;         /* space for a modest key and value */
    settings.num_threads = 4;         /* N workers */
    settings.num_threads_per_udp = 0;
    settings.prefix_delimiter = ':';
    settings.detail_enabled = 0;
    settings.reqs_per_event = 20;
    settings.backlog = 1024;
    settings.binding_protocol = negotiating_prot;
    settings.item_size_max = 1024 * 1024; /* The famous 1MB upper limit. */
    settings.maxconns_fast = false;
    settings.lru_crawler = false;
    settings.lru_crawler_sleep = 100;
    settings.lru_crawler_tocrawl = 0;
    settings.hashpower_init = 0;
    settings.slab_reassign = false;
    settings.slab_automove = 0;
    settings.shutdown_command = false;
    settings.tail_repair_time = TAIL_REPAIR_TIME_DEFAULT;
    settings.flush_enabled = true;
}

static void conn_init(void){
	
}



void do_accept(int fd, short event, void *arg)
{
	int sfd;
	struct sockaddr_in sin;
	socklen_t addrlen;
	addrlen = sizeof(sin);
	sfd = accept(fd, (struct sockaddr *)&sin, &addrlen);
	if (sfd == -1){
		perror("accept failed");
		close(sfd);
		return ((void)1);
	}else {
		//printf("accept: sfd[%p]\n",&sfd);
	}

	if (fcntl(sfd, F_SETFL, fcntl(sfd, F_GETFL) | O_NONBLOCK) < 0){
		perror("setting O_NONBLOCK");
		close(sfd);
		return ((void)2);
	}
	
	//mw_conn *conn = (mw_conn*)malloc(sizeof(mw_conn));
	//conn->sfd = sfd;
	printf("thread_id: 0x%x, working on task sfd %d\n",pthread_self(), sfd);
	dispatch_conn(sfd,thread_number);


	//threadpool_add_worker(conn_work_process, conn);
	//conn_work_process(conn);
	//conn_work_process(&fds[0]);

	//printf("main_base ptr: %p\n",main_base);
	//struct event *rev = (struct event*)malloc(sizeof(struct event));
	//conn->revent = rev;
	
	/*
	conn->revent = (struct event *)malloc(sizeof(struct event));
	event_set(conn->revent, sfd, EV_READ|EV_PERSIST,do_read, conn);
	event_base_set(main_base, conn->revent);
	event_add(conn->revent, 0);
	*/

	//event_del(arg);
	//close(sfd);
	return ;
}

void *conn_work_process(void *data)
{
	//struct event *rev = (struct event*)malloc(sizeof(struct event));
	//conn->revent = rev;
	mw_conn *conn = data;
	printf("thread_id: 0x%x, working on task sfd %d\n",pthread_self(), conn->sfd);
	conn->revent = (struct event *)malloc(sizeof(struct event));
	event_set(conn->revent, conn->sfd, EV_READ|EV_PERSIST,do_read, (void *)conn);
	event_base_set(conn->main_base,conn->revent);
	event_add(conn->revent, 0);
	
	return NULL;
}

void do_read(int sfd, short event, mw_conn *conn){
	printf("read fd: %d\n", sfd);
	char buffer[1024];
	int rev_size;
	//memset(buffer, 0, 1024);
	rev_size = recv(sfd, buffer, 1024, 0);

	//char db[32];
	//char collection[64];
	//char write_buffer[1024];
	if (rev_size > 0){
		msgpack_zone mempool;
		msgpack_zone_init(&mempool, 2048);

		msgpack_object obj;
		int res_unpack = msgpack_unpack(buffer, sizeof(buffer), NULL, &mempool, &obj);
		//printf("%d\n",obj.type);
		if (res_unpack == 1){
			if (obj.type == MSGPACK_OBJECT_ARRAY){
				msgpack_object* p = obj.via.array.ptr;
				//memset(db, 0, 32);
				//strncpy(db, p->via.str.ptr,p->via.str.size);
				
				memset(conn->rquery.db, 0, 256);
				strncpy(conn->rquery.db, p->via.str.ptr,p->via.str.size);
				
				//strncpy(conn->rquery->db, p->via.str.ptr,p->via.str.size);
				p++;
				//memset(collection, 0, 64);
				//strncpy(collection, p->via.str.ptr, p->via.str.size);

				memset(conn->rquery.collection,0,256);
				strncpy(conn->rquery.collection, p->via.str.ptr, p->via.str.size);

				p++;

				memset(conn->rquery.method,0,256);
				strncpy(conn->rquery.method, p->via.str.ptr, p->via.str.size);
				printf("conn->rquery.method %s\n",conn->rquery.method);

				p++;
				if(p->type == MSGPACK_OBJECT_ARRAY){
					//conn->rquery.query = (msgpack_object *)malloc(sizeof(msgpack_object));
					msgpack_object* query = p->via.array.ptr;
					if (query->type == MSGPACK_OBJECT_MAP){
						msgpack_object_kv* kv = query->via.map.ptr;
						printf("kv->key.type : %d\n",kv->key.via.str.size);
						printf("kv->val.type : %d\n",kv->val.via.u64);
						char tmp[64];
						memset(tmp, 0, 64);
						strncpy(tmp, kv->key.via.str.ptr, kv->key.via.str.size);
						conn->rquery.query = bson_new();
						printf("tmp : %s\n", tmp);
						BSON_APPEND_INT32(conn->rquery.query, tmp, kv->val.via.u64);
					}
				}
				
				//msgpack_object_print(stdout, obj);
				//printf("\n");
				//printf("recv: %s", buffer);
				//memset(write_buffer, 0, 1024);
				//printf("%s,%s",db,collection);
				//conn->wbuf = (char *)malloc(1024);
				//conn->wbuf = "test";
				//mongo_clt_worker(mongoc_pool, conn->wbuf, db, collection);
			}else if(obj.type == MSGPACK_OBJECT_POSITIVE_INTEGER) {
				printf("error recv : MSGPACK_OBJECT_POSITIVE_INTEGER\n");
			}
		}else {
			printf("error recv can't parse \n");
		}
		//mongo_clt_worker(mongoc_pool,"gamedb","entity_ff14_ClassJob");
		msgpack_zone_destroy(&mempool);
	}else {
		printf("error recv \n");
	}
	
	/*
	//struct event *send = (struct event*)malloc(sizeof(struct event));
	//conn->wevent = send;
	conn->wevent = (struct event*)malloc(sizeof(struct event));
	event_set(conn->wevent, sfd, EV_WRITE|EV_PERSIST, do_write, (void *)conn);
	event_base_set(main_base,conn->wevent);
	event_add(conn->wevent, 0);
	event_del(conn->revent);
	free(conn->revent);
	//close(sfd);
	*/


	//conn->wbuf = (char *)malloc(1024*2);
	//memset(conn->wbuf,0,1024*2);
	//mongothreadpool_add_worker(mongo_clt_worker,conn);
	//mongo_clt_worker(conn);
	
	//pthread_t thread;
	//pthread_create(&thread,NULL,mongo_clt_worker,conn);
	//pthread_detach(thread);
	
	conn->wevent = (struct event*)malloc(sizeof(struct event));
	event_set(conn->wevent, conn->sfd, EV_WRITE|EV_PERSIST, do_write, (void *)conn);
	event_base_set(conn->main_base,conn->wevent);
	event_add(conn->wevent, 0);
	event_del(conn->revent);
	free(conn->revent);

	//printf("sfd==============[%d]\n",conn->sfd);
	return ;
}

void do_write(int sfd, short event, mw_conn *conn){
	//printf("write fd: %d\n",sfd);

	//conn->wbuf = (char *)malloc(1024*2);
	//memset(conn->wbuf,0,1024*2);
	//mongothreadpool_add_worker(mongo_clt_worker,conn);

	//mongo_clt_worker(conn);

	char buffer[1024*4];
	int send_size;
	//memset(buffer,0,1024*2+1);
	//mongo_clt_worker(conn);
	
	mongoc_client_t *client;
	mongoc_collection_t *collection;
	mongoc_cursor_t *cursor;
	const bson_t *doc;
	//bson_t *query;
	char *str;

	client = mongoc_client_pool_pop(mongoc_pool);
	if (!client){
		return ;
	}
	
	collection = mongoc_client_get_collection(client, conn->rquery.db, conn->rquery.collection);
	//query = bson_new();
	
	//BSON_APPEND_INT32(query, "cost", 8);
	cursor = mongoc_collection_find(collection, MONGOC_QUERY_NONE,0,0,0,conn->rquery.query, NULL, NULL);
	while(mongoc_cursor_next(cursor, &doc)){
		str = bson_as_json(doc, NULL);
		
		memset(buffer, 0, 1024*4+1);
		strcpy(buffer,str);
		sprintf(buffer,"%s\n",buffer);
		send_size = send(sfd,buffer, sizeof(buffer),0);
		if (send_size < 0){
			printf("error send \n");
		}else {
			//printf("send: %s", buffer);
		}

		bson_free(str);
	}
	
	bson_destroy(conn->rquery.query);
	mongoc_cursor_destroy(cursor);
	mongoc_collection_destroy(collection);
	
	//printf("sfd==============[%d]\n",conn->sfd);
	mongoc_client_pool_push(mongoc_pool,client);

	
	/*strcpy(buffer, "test");
	//sprintf(buffer, "\n");
	//printf("%d",sizeof(buffer));
	send_size = send(sfd,buffer, sizeof(buffer),0);
	if (send_size < 0){
		printf("error send \n");
	}else {
		//printf("send: %s", buffer);
	}
	*/		
	
	event_del(conn->wevent);
	free(conn->wevent);
	//free(conn->wbuf);
	close(conn->sfd);
	free(conn);
	conn = NULL;
	//printf("event write sfd [%d]\n",sfd);
	close(sfd);	
	
	return ;
}

//void *mongo_clt_worker (void *data, char* res, char* db, char* table)
int *mongo_clt_worker (void *data)
{
	char buffer[1024*2];
	int send_size;

	mw_conn *conn = data;
	//printf("thread_id: 0x%x, working on task sfd %d\n",pthread_self(), conn->sfd);
	//printf("%s\n",conn->wbuf);
	//mongoc_client_pool_t *pool = data;
	mongoc_client_t *client;
	mongoc_collection_t *collection;
	mongoc_cursor_t *cursor;
	const bson_t *doc;
	bson_t *query;
	char *str;

	client = mongoc_client_pool_pop(mongoc_pool);
	if (!client){
		return NULL;
	}
	
	collection = mongoc_client_get_collection(client, conn->rquery.db, conn->rquery.collection);
	query = bson_new();
	BSON_APPEND_INT32(query, "cost", 8);
	cursor = mongoc_collection_find(collection, MONGOC_QUERY_NONE,0,0,0,query, NULL, NULL);
	while(mongoc_cursor_next(cursor, &doc)){
		str = bson_as_json(doc, NULL);
		//printf("conn->wbuf====>[%d]%s\n", conn->sfd, str);

		//strcpy(res,str);

		//conn->wbuf = (char *)malloc(1024*2);
		//strcpy(conn->wbuf,str);
		//printf("sizeof(conn->wbuf) : %s%d\n",conn->wbuf,sizeof(conn->wbuf));

		bson_free(str);
	}
	
	bson_destroy(query);
	mongoc_cursor_destroy(cursor);
	mongoc_collection_destroy(collection);
	
	//printf("sfd==============[%d]\n",conn->sfd);
	mongoc_client_pool_push(mongoc_pool,client);
	

	//conn->wevent = (struct event*)malloc(sizeof(struct event));
	//event_set(conn->wevent, conn->sfd, EV_WRITE|EV_PERSIST, do_write, (void *)conn);
	//event_base_set(main_base,conn->wevent);
	//event_add(conn->wevent, 0);

	return 0;
}

void memwared_close(int sig)
{
	printf("memwared close, please 'ctrl+c' to shutdown \n");
	//threadpool_destroy();
	//mongothreadpool_destroy();
	
	mongoc_uri_destroy(mongoc_uri);
	mongoc_cleanup();
	event_base_loopbreak(main_base);
	signal(SIGINT, SIG_DFL);
}

/*static int server_socket(const char *interface,
						int prot,
						enum network_transport transport,
						FILE *portnumber_file){
	int sfd;
	struct linger ling = {0,0};
	struct addrinfo *ai;
	struct addrinfo *next;
	struct addrinfo hints = {.ai_flags = AI_PASSIVE,
							.ai_family = AF_UNSPEC};
	char port_buf[NI_MAXSERV];
	int error;
	int success = 0;
	int flags = 1;

	hints.ai_socktype = IS_UDP(transport) ? SOCK_DGRAM : SOCKSTREAM;

	if (port == -1){
		port = 0;
	}
	snprintf(port_buf, sizeof(port_buf), "%d", port);
	error = getaddrinfo(interface, port_buf, &hints, &ai);
	if (error != 0){
		if (error != EAI_SYSTEM){
			fprintf(stderr, "getaddrinfo(): %s\n", gai_strerror(error));
		}else {
			perror("getaddrinfo()");
			return 1;
		}
	}

	for (next = ai; next; next = next->ai_next){
		conn *listen_conn_add;
		if ((sfd = new_socket(next)) == -1){
			if (errno = EMFILE){
				perror("server_socket");
				exit(EX_OSERR);
			}
			continue;
		}

		setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, (void *)&flags, sizeof(flags));
	}

}

static int server_sockets(int port, enum network_transport transport,
		FILE *portnumber_file)
{
	if (settings.inter == NULL)	{
		return server_socket(settings_inter, port, transport, portnumber_file);
	}else {
		char *b;
		int ret = 0;
		char *list = strdup(settings.inter);

		if (list == NULL){
			fprintf(stderr, "Failed to allocate memory for parsing server interface string\n");
			return 1;
		}
		for (char *p = strtok_r(list, ";,", &b);
				p != NULL;
				p = strtok_r(NULL, ";,", &b)){
			int the_port = port;
			char *s = strchr(p, ':');
			if (s != NULL){
				*s = '\0';
				++s;
				if (!safe_strtol(s, &the_port)){
					fprintf(stderr, "Invalid port number: \"%s\"", s);
					return 1;
				}
			}
			if (strcmp(p, "*") == 0){
				p = NULL;
			}
			ret |= server_socket(p, the_port, transport, portnumber_file);
		}
		free(list);
		return ret;
	}
}
*/


static void 
sig_handler(const int sig)
{
	printf("Signal handled: %s.\n", strsignal(sig));
	exit(EXIT_SUCCESS);
}

static bool 
sanitycheck(void)
{
	const char *ever = event_get_version();
	printf("libevent version :%s\n",ever);
	if (ever != NULL){
		if (strncmp(ever, "1.", 2) == 0){
			if ((ever[2] == '1' || ever[2] == '2') && !isdigit(ever[3])){
				fprintf(stderr, "You are using libevent %s.\nPlease upgrade to "
						"a more recent version (1.3 or newer)\n",
						event_get_version());
				return false;
			}
		}
	}
	return true;
}

int 
main (int argc, char **argv)
{
	int c;
	enum hashfunc_type hash_type = JENKINS_HASH;
	int maxcore = 0;
	struct rlimit rlim;
	char *username = NULL;
	struct passwd *pw;


	if (!sanitycheck()){
		return EX_OSERR;
	}
	printf("msgpack-c:%s\n", msgpack_version());
	printf("mongo-c-driver:%s\n", MONGOC_VERSION_S);

	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);

	/* set stderr non-buffering */
	//setbuf(stderr, NULL);
	
	while (-1 != (c = getopt(argc, argv,
			"p:" // TCP port
			"P:"
			"r:" // maxcore core file limit
			"u:" // user identity to run as
		))){
		switch (c){
		case 'p':
			settings.port = atoi(optarg);
			printf("TCP port: %d\n",atoi(optarg));
			break;
		case 'r':
			maxcore = 1;
			break;
		case 'u':
			username = optarg;
			break;
		default:
			fprintf(stderr, "Illegal argument \"%c\"\n",c );
			return 1;
		}
	}

	/*if (hash_init(hash_type) != 0){
		fprintf(stderr, "Failed to initialize hash_algorithm!\n");
		exit(EX_USAGE);
	}*/

	/* maxcore file limit */
	/*if (maxcore != 0){
		struct rlimit rlim_new;
		if (getrlimit(RLIMIT_CORE, &rlim) == 0){
			rlim_new.rlim_cur = rlim_new.rlim_max = RLIM_INFINITY;
			if (setrlimit(RLIMIT_CORE, &rlim_new) != 0){
				rlim_new.rlim_cur = rlim_new.rlim_max = rlim.rlim_max;
				(void)setrlimit(RLIMIT_CORE, &rlim_new);
			}
		}

		if ((getrlimit(RLIMIT_CORE, &rlim) != 0) || rlim.rlim_cur == 0){
			fprintf(stderr, "failed to ensure corefile creation\n");
			exit(EX_OSERR);
		}
	}
	
	if (getrlimit(RLIMIT_NOFILE, &rlim) != 0){
		fprintf(stderr, "failed to getrlimit number of files\n");
		exit(EX_OSERR);
	}else {
		rlim.rlim_cur = 1024;
		rlim.rlim_max = 1024;
		if (setrlimit(RLIMIT_NOFILE, &rlim) != 0){
			fprintf(stderr, "failed to set rlimit for open files. Try starting as root or requesting smaller maxconns value.\n");
			exit(EX_OSERR);
		}
	}*/

	/* start allow root user */
	if (getuid() == 0 || geteuid() == 0){
		if (username == 0 || *username == '\0'){
			fprintf(stderr, "can't run as root without the -u switch\n");
			exit(EX_USAGE);
		}
		if ((pw = getpwnam(username)) == 0){
			fprintf(stderr, "can't find the user %s to switch to \n", username);
			exit(EX_NOUSER);
		}
		if (setgid(pw->pw_gid) < 0 || setuid(pw->pw_uid) < 0){
			fprintf(stderr, "failed to assume identity of user %s \n", username);
			exit(EX_OSERR);
		}
	}

	/* initialize main thread libevent instance */
	main_base = event_init();

	/* initialize other stuff*/
	//assoc_init(settings.hashpower_init);
	//conn_init();
	
	if (sigignore(SIGPIPE) == -1){
		perror("failed to ignore SIGPIPE; sigaction");
		exit(EX_OSERR);
	}
	
	/* thread_pool_init */
	//threadpool_init(4);
	thread_init(thread_number, main_base);
	
	//mongothreadpool_init(10);

	/* socket tcp ,bind */
	/*if (settings.socketpath == NULL){
		const char *portnumber_filename = getenv("MEMCACHE_PORT_FILENAME");
		char temp_portnumber_filename[PATH_MAX];
		FILE *portnumber_file = NULL;
		
		if (portnumber_filename != NULL){
			snprintf(temp_portnumber_filename,
					sizeof(temp_portnumber_filename),
					"%s.lck",portnumber_filename);
			portnumber_file = fopen(temp_portnumber_filename, "a");
			if (portnumber_file == NULL){
				fprintf(stderr, "Failed to open \"%s\": %s\n",
						temp_portnumber_filename, strerror(errno));
			}
		}

		errno = 0;
		if (settings.port && server_sockets(settings.port, tcp_transport,
					portnumber_file)){
			vperror("failed to listen on TCP port %d", settings.port);
			exit(EX_OSERR);
		}

		if (portnumber_file){
			fclose(portnumber_file);
			rename(temp_portnumber_filename, portnumber_filename);
		}
	}*/

	int sfd;
	int flags;
	struct sockaddr_in serv_addr;
	if ((sfd = socket(PF_INET, SOCK_STREAM, 0)) < 0){
		perror("sokcet error");
		return -1;
	}
	if ((flags = fcntl(sfd, F_GETFL, 0) < 0 || 
				fcntl(sfd, F_SETFL, flags | O_NONBLOCK) < 0)){
		perror("setting O_NONBLOCK");
		close(sfd);
		return -1;
	}
	
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(settings.port);
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

	int ireuseadd_on = 1;
	setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &ireuseadd_on, sizeof(ireuseadd_on));
	
	if (bind(sfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != 0){
		perror("bind() error");
		close(sfd);
		return -1;
	}

	if (listen(sfd, settings.backlog) < 0){
		perror("listen() error");
		close(sfd);
		return -1;
	}else {
		printf("listenning...\n");
		//dispatch_new(sfd);
	}

	mongoc_init();
	mongoc_uri = mongoc_uri_new("mongodb://root:root@127.0.0.1:27017/?authSource=gamedb&minpollsize=16");
	mongoc_pool = mongoc_client_pool_new(mongoc_uri);

	struct event ev;
	printf("socket fd: %d\n",sfd);
	//printf("main_base ptr: %p\n",main_base);
	event_set(&ev, sfd, EV_READ | EV_PERSIST, do_accept, &ev);
	event_base_set(main_base, &ev);
	event_add(&ev, 0);
	
	signal(SIGINT, memwared_close);

	if (event_base_loop(main_base, 0) != 0){
		exit(EX_OSERR);
	}


	printf("close memwared bye!\n");
	close(sfd);
	return 0;
}
