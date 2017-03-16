#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <signal.h>
#include <time.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>
#include <pthread.h>

#define CONFIG_FILE "httpd.conf"
#define MAXEVENTS 64
#define FILE_SIZEBUFFER_LENGTH 9

typedef struct {						/* structure to store POST body string and it's length */
		char post_param[1024];
		int length;
    } post;
    
   
void* thread_func(void* arg);
int upload_file(char* content, char* bound_end);
    static int PORT;
    static char _ROOT_DIR_[256];
    static char _PHP_CGI_[256];
    static char _PHP_CGI_PATH_[256];
    static char _FILE_UPLOAD_DIR_[256];
	void sig_handler(int sign);    /* signal handler prototype function */
	int nport, nbytes;
    int on = 1;
    int sd;
	
    
    void read_configuration() {
		char buf[256];
		int n_lines = 0;   								/* initial size */
		char* arg[20];									/* default number */
		bzero(buf, 256);
		int i = 0;
		FILE* f = fopen(CONFIG_FILE, "r");
		if( f == NULL ) {
			perror("can't open configuration file or file missing ");
			exit(1);
		}
		do {											/* loop - read all lines of file and store them in array of strings */
			arg[i] = malloc(64);						/* allocate memory 64 bytes for each line and store pointer in array */
			fgets(arg[i], 64, f);
			i++;
		} while( !feof(f));
		n_lines = i - 1;
		/* now let's parse each string */
		char *p;
		for( i = 0; i <= n_lines; i++) {
			
			if( arg[i][0] == '#' ) continue;
			if( (p = strstr(arg[i], "PORT")) != NULL ) {	
				if( (p = strchr(arg[i], '=')) != NULL ) {
					p++;
					PORT = atoi(p);
				}
			}
			if( (p = strstr(arg[i], "ROOT_DIR")) != NULL ) {	
				if( (p = strchr(arg[i], '=')) != NULL ) {
					p++;
					strcpy(_ROOT_DIR_, p);
					p = strchr(_ROOT_DIR_, '\n');        /* remove EOL */
					if( p != NULL ) *p = '\0';
				}
			}	
			if( (p = strstr(arg[i], "PHP_CGI_FILE")) != NULL ) {	
				if( (p = strchr(arg[i], '=')) != NULL ) {
					p++;
					strcpy(_PHP_CGI_, p);
					p = strchr(_PHP_CGI_, '\n');        /* remove EOL */
					if( p != NULL ) *p = '\0';
				}
			}	
			if( (p = strstr(arg[i], "PHP_CGI_PATH")) != NULL ) {
				if( (p = strchr(arg[i], '=')) != NULL ) {
					p++;
					strcpy(_PHP_CGI_PATH_, p);
					p = strchr(_PHP_CGI_PATH_, '\n');        /* remove EOL */
					if( p != NULL ) *p = '\0';
				}
			}	
			if( (p = strstr(arg[i], "FILE_UPLOAD_DIR")) != NULL ) {	
				if( (p = strchr(arg[i], '=')) != NULL ) {
					p++;
					strcpy(_FILE_UPLOAD_DIR_, p);
					p = strchr(_FILE_UPLOAD_DIR_, '\n');        /* remove EOL */
					if( p != NULL ) *p = '\0';
				}
			}	
				
		}
		
		for( i = 0; i <= n_lines; i++)					/* free memory */
			free(arg[i]);
		fclose(f);
	}
    
    int content_type(char* header) {
		char s_multipart[] = "multipart/form-data";
		char s_bound[] = "boundary";
		char s_filename[] = "filename";
		char *boundary;    
        char *upload_file_name;
		char bound_begin[64];
		char bound_end[64];
		boundary = malloc(64);
		char *b = boundary;
		bzero(boundary, 64);
		upload_file_name = malloc(256);
		char *fn = upload_file_name;
		bzero(upload_file_name, 256);
		char *p = strstr(header, s_multipart);
		/**************getting boundary*******************/
		if( p != NULL ) {
			p = strstr(header, s_bound);
			p += sizeof(s_bound);
		}
		else return 1;	
		while( *p != '\r' ) {
			*boundary = *p;
			boundary++;
			p++;
		}
		/***************gettinf file name*****************/
		char *pfn = strstr(header, s_filename);
		if( pfn != NULL ) {
			pfn += sizeof(s_filename) + 1;
		} 
		else return 1;
		while( *pfn != '"' ) {
			*upload_file_name = *pfn;
			upload_file_name++;
			pfn++;
		}
	
		/****************getting file content**************/
		strcpy(bound_begin, "--");
		strcat(bound_begin, b);         /* ------boundary */
		strcpy(bound_end, bound_begin);
		strcat(bound_end, "--");      /* ------boundary-- */
		
		p = strstr(header, bound_begin);  /* begin of file header */
		
		for(; ;) {
			if( *p == '\r' ) { 
				p += 2;
				if( *p == '\r' ) {
					p += 2;               /* now p --> points to beginning of file content */
					break;
				}
			}
			else {
				p++;
			}
		}
		upload_file(p, fn);
		printf("Boundary is: %s\n", b);
		printf("File name is: %s\n", fn);
		printf("================File content is=====================: %s\n", p);
		return 0;
	}
	
	int upload_file(char* content, char* filename) {
		char *p;
		char full_path[256];
		strcpy(full_path, _FILE_UPLOAD_DIR_);
		strcat(full_path, "/");
		strcat(full_path, filename);
		
		FILE* f = fopen(full_path, "w");
		if( f == NULL ) {
			perror("error creating file:");
			return 0;
		}
		p = content;
		char buf[1];
		bzero(buf, sizeof(buf));
		while( 1 ) {
			if( *p == '\r' ) { 
				p ++;
				if( *p == '\n' ) {
					break;
				}
			}
			fputc(*p, f);
			p++;
		}
		fclose(f);
		return 0;
	}
	
    void not_found(int socket) {   
		char not_found[] = "HTTP/1.1 404 Not Found\r\nConnection: close\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: 100\r\n\r\n \
		<html><body><h1>404 Not found!</h1> </br> <h3>Sorry, required page doesn't exist!</h3></body></html>";	
		send(socket, not_found, sizeof(not_found), 0);
		close(socket);
	}
	
	int parse_for_method(char* header) {
		char *p = strstr(header, "GET");
		if( p != NULL ) return 0;
		p = strstr(header, "POST");
		if( p != NULL ) return 1;
		return -1;
	}
	
    char* parse_head_for_filename(char* header) {
		char *p;
		char *file_name = malloc(256);
		char *f = file_name;
		bzero(file_name, 256);
		p = strstr(header, "GET");       /* !!!!!!!!!!!!!!!  not for POST yet */
		if( p != NULL ) {
			p = strchr(header, '/');
			p++;
			if( p != NULL ) {
				while( *p != 32  ) {
					if( *p == '?' )
						return f;
					*file_name = *p;
					p++;
					file_name++;
				}
			}
			else { free(file_name); return NULL; }
		}
		else if( strstr(header, "POST") != NULL ) {
			p = strchr(header, '/');
			p++;
			if( p != NULL ) {
				while( *p != 32 ) {
					*file_name = *p;
					p++;
					file_name++;
				}			
			}
		}	
		else { free(file_name); return NULL; }
		if( strcmp(f, "") == 0 ) {					/* if start page is not specified in URL */
			strcpy(f, "index.html");				/* default start page "index.html" as usual */
		}
		
		return f;
	}
	
	int check_ext_type(char* filename) {
		char* p = filename;
		char ext[8];
		bzero(ext, 8);
		int i = 0;
		p = strchr(filename, '.');
		if ( p != NULL ) {
			p++; 					/* skip '.' */
			while( *p != '\0' ) {
				ext[i] = *p;
				p++;
				i++;
			}
		}
		else { exit(1); }
		if( (strcmp(ext, "jpeg") && strcmp(ext, "jpg") && strcmp(ext, "png") && strcmp(ext, "gif") && strcmp(ext, "bmp")) == 0 ) {
			return 0;    		/* 0 - pictures */
		}
		else if( (strcmp(ext, "txt") && strcmp(ext, "html") && strcmp(ext, "js") && strcmp(ext, "htm") && 
								strcmp(ext, "xhtml") && strcmp(ext, "xml") ) == 0 ) {
			return 1;   	 	/* 1 - txt and html files */
		}
		else if( strcmp(ext, "php") == 0 ) {			
			return 2;			/* 2 - php scripts */
		}
		else 
			return 3;			/* 3 - other */
	}
	
	char* parse_for_contype(char* header) {
		char *p;
		char *connection_type = malloc(64);
		bzero(connection_type, 64);
		char *con = connection_type;
		p = strstr(header, "Connection:");
		if( p != NULL ) {
				while( *p != 32 ) {
					p++;
				}
				while( *p != '\n' ) {		
					*con = *p;
					p++;
					con++;
				}
		}
		else { free(connection_type); return NULL; }
		return connection_type; 					/* connection typ: keep-alive or close */
	}
	
	int file_size(char* filename) {
		FILE * f;
		int f_size = 0;
		f = fopen(filename, "r");
		if( f == NULL ) {
			perror("file not found: ");
			return 1;
		}
		while ( !feof(f) ) {
			fgetc(f);
			f_size++;
		}
		fclose(f);
		return f_size - 1;
		free(filename);
	}
	
	char* get_server_rootdir() {
		char *path = malloc(1024);
		bzero(path, 1024);
		getcwd(path, 1024);
		return path;
	}
	
	void send_header(char *filename, int size, int socket) {
		char length[FILE_SIZEBUFFER_LENGTH];
		bzero(length, FILE_SIZEBUFFER_LENGTH);
		int f_size = 0;
		if( size == 0 ) f_size = file_size(filename);
        else f_size = size;
		sprintf(length, "%d", f_size);
		char head_p1[] = "HTTP/1.1 200 OK\r\nHost: localhost\r\nContent-Type: text/html; charset=utf-8";
		char head_p2[] = "\r\nContent-Length: ";
		char head_p3[] = "\r\nConnection: keep-alive\r\n\r\n"; 
		send(socket, head_p1, sizeof(head_p1), 0);
		printf("%s",head_p1);
		strcat(head_p2, length);
		int i = 0;
		while( head_p2[i] != '\0' ) {      		/* count string size ( may be should to make separate function) */ 		
			i++;
		}
		send(socket, head_p2, i, 0);
		printf("%s",head_p2);	
		send(socket, head_p3, sizeof(head_p3) - 1, 0);
		printf("%s",head_p3);
		//free(filename);	
	}
	
	char* remove_phpcgi_header(char* response) {
		/* Remove "Content-type: text/html; charset=UTF-8" string
		 * in the response of php-cgi, as it goes first before the usful result */
		 char *p = strstr(response, "charset=UTF-8");
		 if( p == NULL ) return NULL;
		 while( *p != '\n' ) {
			 if( *p == 32 ) {
				 p++;
				 return p;
			 }
			 p++;
		 }
		 p++; 									/* now p -> points to beginning of result */
		 return p;	 
	}
	
	int php_cgi(char *header, char* post_param, int length, int socket) {
												/* example */
												/* GET /submit.php?login=hello+world HTTP/1.1 */
		pid_t pid;
		int status;
		char filename[64];
		char parameter[1024];
		char buf[1];							/* buffer to read result from pipe and sent to the client by one char */
		char query_string[1024];
		char script_filename[1024];
		char s_length[6];						/* buffer to store string representation of Content-length */
		char cgi_script[256];
		bzero(s_length, 6);
		bzero(query_string, 1024);
		bzero(parameter, 1024);
		bzero(filename, 64);
		bzero(buf, 2);
		int i = 0;
		char full_filename[256];
		bzero(full_filename, 1024);
		char* p = strchr(header, '?');		/* extract string of parameters */
		if( p != NULL ) {
			p++;
			i = 0;
			while( *p != 32 ) {
				parameter[i] = *p;		
				p++;		
				i++;
			}
		}
		else {
			strcpy(parameter, "");   	/* if there are no parameters, pass an empty string instead */
		}					
		strcpy(full_filename, _ROOT_DIR_ /*get_rootdir()*/);    /* do not forget to FREE memory */
		strcat(full_filename, "/");
		strcat(full_filename, parse_head_for_filename(header));
		/* check for file existence */
		//printf("%s\n", full_filename);
		FILE *f = fopen(full_filename, "r");
		if( f == NULL ) {						/* if file doesn't exist then send 404 Error */
			not_found(socket);
			return -1;
		}
		sprintf(s_length, "%d", length);	
		strcpy(query_string, "QUERY_STRING=");
		if( post_param == NULL) {
			strcat(query_string, parameter);
		} 
		strcpy(script_filename, "SCRIPT_FILENAME=");
		strcat(script_filename, full_filename);
		int fd[2], fd2[2];              						/* fd - pipe for output of result by php-cgi,*/
		if( (pipe(fd) != 0) || (pipe(fd2) != 0) )				/* fd2 - pipe for output of errors */
			printf("pipes creation error!\n");							
		if( (pid = fork()) != -1 ) {
			if( pid == 0 ) {
				dup2(fd[1], STDOUT_FILENO);						/* redirect stdout to --> pipe write */
				dup2(fd2[1], STDERR_FILENO);					/* redirect stderr to --> pipe write */
				               		/* if we call .php script with global POST and GET vars, then set environment's vars */ 
			    
				if( parse_for_method(header) ) {
					    strcpy(cgi_script, get_server_rootdir());
					    strcat(cgi_script, "/cgi.sh");
						if( -1 == execl(cgi_script, "cgi.sh", post_param, s_length, "POST", full_filename, _PHP_CGI_PATH_, 
													(char*)NULL)) /* calling php-cgi process */
					       perror("execl error: ");	
					}
					else {
						putenv("GATEWAY_INTERFACE=CGI/1.1");    /* setting environment variables for php-cgi to work ( GET method ) */
						putenv("SERVER_PROTOCOL=HTTP/1.1");
						putenv(query_string);
						putenv(script_filename);
						putenv("REQUEST_METHOD=GET");
						putenv("REDIRECT_STATUS=200");
						putenv("HTTP_ACCEPT=text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
						putenv("CONTENT_TYPE=application/x-www-form-urlencoded");
						if( -1 == execl(_PHP_CGI_PATH_, _PHP_CGI_,  (char*)NULL))   /* calling php-cgi process */
							perror("execl error: ");	
				}	
				close(fd[1]);
				close(fd[0]);				
				close(fd2[1]);
				close(fd2[0]);							
			}
			else {												/* parent process */
				close(fd[1]);
				close(fd2[1]);
				int i = 0;
				int n = 0;
				char *mem = malloc(1000000); 					/* allocating 1MB of memory for php-cgi output result */
				if( mem == NULL ) {
					perror("can't allocate memory: ");
					return 0;
				}
				char *p = mem;
				char *p_f = mem;
				bzero(mem, 1000000);	
				while( (n = read(fd[0], buf, 1)) > 0 ) { 		/* reading result from pipe --> memory */
					*mem = *buf;
					mem++;
					i++;
				}		
				while( (n = read(fd2[0], buf, 1)) > 0 ) { 		/* reading result from pipe --> memory */
					*mem = *buf;
					mem++;
					i++;
			}
			    waitpid(-1, &status, 0);						/* wait for child process (php-cgi) to terminate */
				if ( (p = remove_phpcgi_header(p)) != NULL ) {
					i -= 38;
					send_header(filename, i, socket);			/* send header */									
					while( i != 0 ) {
						send(socket, p, 1, 0);		     		/* send result one-by-one to browser */
						p++;
						i--;
					}
				}
				else {
					send_header(filename, i, socket);
					p = p_f;
					while( i != 0 ) {
						send(socket, p, 1, 0);		     		/* send result one-by-one */
				     	p++;
						i--;
					}
				}
				free(p_f);				
				close(fd[0]);
				close(fd2[0]);
			}
		}
		else {
			perror("fork error, can't create process: ");
			return 1;
		}
		return 0;
	}
		
	int read_media_file(char* filename, char* header, int socket) {
		FILE* f;
		f = fopen(filename, "rb");
		if ( f == NULL ) {
			not_found(socket);
			return 1;
		}
		char length[FILE_SIZEBUFFER_LENGTH];	
		bzero(length, FILE_SIZEBUFFER_LENGTH);
		int f_size = file_size(filename);
		sprintf(length, "%d", f_size);//image/jpeg
		char head_p1[] = "HTTP/1.1 200 OK\r\nHost: localhost\r\nContent-Type: image/jpeg";
		char head_p2[] = "\r\nContent-Length: ";
		char head_p3[] = "\r\nConnection: close\r\n\r\n"; 
		send(socket, head_p1, sizeof(head_p1), 0);
		printf("%s",head_p1);
		strcat(head_p2, length);
		int i = 0;
		while( head_p2[i] != '\0' ) {	
			i++;
		}
		send(socket, head_p2, i, 0);
		printf("%s",head_p2);	
		send(socket, head_p3, sizeof(head_p3) - 1, 0);
		printf("%s",head_p3);		
		char *buf = malloc(1);
		bzero(buf, 1);
		int n;
		while ( (n = fread(buf, 1, 1, f)) > 0  ) {			
			if( send(socket, buf, n, 0) == -1 )
				printf("%s","Error sending file!!");
			bzero(buf, 1);
		} 
		fclose(f);
		free(buf);
		if( strcmp(parse_for_contype(header), "close") == 0 ) {
			close(socket);
		}
		free(filename);
		return 0;
	}
	
	
	int read_html_file(char* filename, char* header, int socket) {
		FILE* f;
		f = fopen(filename, "r");
		if ( f == NULL ) {
			not_found(socket);
			return 1;
		}
		send_header(filename, 0, socket);    /* call send_header */
		char *buf = malloc(1);
		bzero(buf, 1);
		int n;
		while ( (n = fread(buf, 1, 1, f)) > 0  ) {			
			if( send(socket, buf, n, 0) == -1 )
				printf("%s","Error sending file!!");
			bzero(buf, 1);
		} 
		fclose(f);
		free(buf);	
		free(filename);	
		if( strcmp(parse_for_contype(header), "close") == 0 ) {
			close(socket);
		}
		return 0;
	}
		
    static int make_socket_non_blocking (int sfd)
	{
		int flags, s;
		flags = fcntl (sfd, F_GETFL, 0);
		if (flags == -1) {
			perror ("fcntl");
			return -1;
		}
		flags |= O_NONBLOCK;
		s = fcntl (sfd, F_SETFL, flags);
		if (s == -1) {
			perror ("fcntl");
			return -1;
		}
		return 0;
	}
	
	void parse_post_params(char* header, post* p) {  /* takes POST response-header from client as a parameter, parses header, */
		int i = 0;							/* extracts body and Content-Length */
		char *ar[32];
		char *c = header;
		int j = 0, flag = 0;
		for( i = 0; i <= 32; i++ ) {
			ar[i] = malloc(1024);
			do  {
				
				ar[i][j] = *c;
				if( ar[i][0] == '\r' ) { c++; flag = 1; break; }
				c++;
				j++;
			} while( *c != 10 );
			c++;
			j = 0;
			if( flag ) { strcpy(p->post_param, c); break;}	
		}
	
		for( i = 0; i <= 32; i++ ) {
			if( strstr(ar[i], "Content-Length:") != NULL ) {
				c = strchr(ar[i], 32);
				c++;
				int n = atoi(c);
				p->length = n;
				//printf("%d\n", n); 
				break;
			}		
		}	
	}	
    
    int main(int argc, char* argv[]) {
        read_configuration();
        printf("%s\n", _FILE_UPLOAD_DIR_);
        printf("%s\n",_ROOT_DIR_);
		printf("%s\n",_PHP_CGI_);
		printf("%s\n",_PHP_CGI_PATH_);

        pthread_t thread_1, thread_2;
        pthread_attr_t attr;
        //nport = atoi("127.0.0.1");						/* get number of port from command line as a parameter  */
											
        struct sockaddr_in6 serv_addr;
        struct hostent;
        bzero(&serv_addr, sizeof(serv_addr));
        serv_addr.sin6_family = AF_INET6; 						/* support ipv6 */
        //serv_addr.sin_addr.s_addr = INADDR_ANY; 				/* only for ipv4 */
        serv_addr.sin6_addr = in6addr_any; 
        serv_addr.sin6_port = htons(PORT);
        serv_addr.sin6_scope_id = 5;
        if( (sd = socket(AF_INET6, SOCK_STREAM, 0)) == -1 ) {	/* AF_INET6 socket is supported, compatible with ipv4 */
            perror("error calling socket()"); 					/* socket accepts both ipv4 and ipv6 conectionы. ::1 - localhost in ipv6 */
            exit(EXIT_FAILURE);
        }
        if( fcntl(sd, F_SETFL, O_NONBLOCK) ) {
			printf("Could not make the socket non-blocking: %m\n");
			close(sd);
			return 3;
		}
		if( setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) ) {
			printf("Could not set socket %d option for reusability: %m\n", sd);
			close(sd);
			return 4;
		}
        
        if( bind(sd,(struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1  ) {
            perror("error calling bind()"); 
            close(sd);
            exit(EXIT_FAILURE);
        }
        printf("Server is ready, waiting fo connection...\n");   /* send ready prompt to the client */
        if( listen(sd, 50) == -1 ) {
            perror("error calling listen()"); 
            close(sd);
            exit(EXIT_FAILURE);
        }
       
        signal(SIGINT, sig_handler); 				/* set signal handler */     
        pthread_attr_init(&attr);
        int ret = pthread_create(&thread_1, &attr, &thread_func, &sd);
        if( ret != 0 ) {
			perror("thread creating error:");
			exit(1);
		}
		ret = pthread_create(&thread_2, &attr, &thread_func, &sd);
        if( ret != 0 ) {
			perror("thread creating error:");
			exit(1);
		}
		pthread_join(thread_1, NULL);
		pthread_join(thread_2, NULL);
		 
	 }
	 
	void* thread_func(void* arg) {
		int ns, efd;
		struct epoll_event event;
		struct epoll_event *events;
		socklen_t addrlen;
        struct sockaddr_in6  clnt_addr;
        int sd = *(unsigned int*)arg;
        efd = epoll_create(5);                     /* epoll descriptor */
		if (efd < 0) {
			printf("Could not create the epoll fd: %m");
			close(efd);
			return NULL;
		}
         event.data.fd = sd;
		 event.events = EPOLLIN | EPOLLET;
		 int s = epoll_ctl(efd, EPOLL_CTL_ADD, sd, &event); /* adds server's socket to epoll */
		 if (s == -1) {
			  perror ("error by epoll_ctl");
			  close(efd);
			  exit(1);
		 }
         events = calloc(MAXEVENTS, sizeof event);         /* memory for events */
         while( 1 ) {	
				 						/* eternal loop for accept client's connections */    
			 int n, i;
			 printf("before epoll wait\n");
			 n = epoll_wait(efd, events, MAXEVENTS, -1);
			 for (i = 0; i < n; i++) {	
				 //printf("%d\n", i);		 
				  if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP)|| (!(events[i].events & EPOLLIN))) {
					  /* An error has occured on this fd, or the socket is not
						 ready for reading (why were we notified then?) */
					  fprintf (stderr, "epoll error\n");
					  close (events[i].data.fd);
					  continue;
				  }	
				  else if( (events[i].events & EPOLLIN) && (events[i].data.fd == sd) ) {
								bzero(&clnt_addr, sizeof(clnt_addr));
								addrlen = sizeof(clnt_addr);
								if( (ns = accept(sd,(struct sockaddr*)&clnt_addr, &addrlen)) == -1 ) { /* wait for client to connect */
									  if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
										  /* We have processed all incoming
											 connections. */
										  break;
										}
									  else {
										  perror ("accept");
										  break;
										}
									}
								printf("accepted\n");
								s = make_socket_non_blocking(ns);
								if (s == -1)
									exit(1);
								event.events = EPOLLIN | EPOLLET;
								event.data.fd = ns;
								if (epoll_ctl(efd, EPOLL_CTL_ADD, ns, &event) < 0) {
									  printf("Couldn't add client socket to epoll set:\n");
									  exit(1);      
								}
							} 
							else {
								    //int done = 0;
								    ssize_t count;
									char buf[1000000];
									bzero(buf, sizeof(buf));		
									while( (count = read(events[i].data.fd, buf, sizeof(buf))) > 0 ) {
										printf("%s\n",buf);
										content_type(buf);
										char *f_name = parse_head_for_filename(buf);	       /*   1 */
										if ( f_name != NULL) {
											if (check_ext_type(f_name) == 1) {             /* 2 */
												read_html_file(f_name, buf, events[i].data.fd);
											}
											else if( check_ext_type(f_name) == 0 ) {
												read_media_file(f_name, buf, events[i].data.fd);
											}
											else if( check_ext_type(f_name) == 2 ) {
												post p;
												if( parse_for_method(buf) == 1) {					
													parse_post_params(buf, &p);
													//printf("Params: %s ", p.post_param);
													php_cgi(buf, p.post_param, p.length, events[i].data.fd);
												} 
												else {
													php_cgi(buf, NULL, 0, events[i].data.fd);
												}		
											}  
											else if( check_ext_type(f_name) == 3 ) {
												not_found(events[i].data.fd);				/* if erver will be support another file extensions then change it!!!!! */
											}
										} 
										else close(events[i].data.fd);
										bzero(buf, sizeof(buf));
									}
							  
	                   }
			}    
        }
        //free(events);
        close(sd);
    }
    
    
       
    void sig_handler(int sign)     /* signal handler */
    {
		if ( sign == SIGINT ) {    /* catch Ctrl-C signal, to terminate server correctly with releasing socket descriptor */
            close(sd);
            printf("Good bye\n");
            exit(EXIT_FAILURE);
        }
     }
