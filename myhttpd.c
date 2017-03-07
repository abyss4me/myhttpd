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

#define MAXEVENTS 64
#define PORT 7771
#define FILE_SIZEBUFFER_LENGTH 9


	void sig_handler(int sign);    /* signal handler prototype function */
	int ns, nport, nbytes;
    int on = 1;

	int sd, efd, clientsd, fd;
	struct epoll_event event;
    struct epoll_event *events;
    
    void not_found(int socket) {
		char not_found[] = "HTTP/1.1 404 Not Found\r\nConnection: close\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: 43\r\n\r\n<html><body>Page not found!!!</body></html>";	
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
		return f;
	}
	
	int check_content_type(char* filename) {
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
		else if( (strcmp(ext, "txt") && strcmp(ext, "html") && strcmp(ext, "js") && strcmp(ext, "htm") && strcmp(ext, "xml") ) == 0 ) {
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
		while ( !feof(f) ) {
			fgetc(f);
			f_size++;
		}
		fclose(f);
		return f_size - 1;
		free(filename);
	}
	
	char* get_rootdir() {
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
		char head_p3[] = "\r\nConnection: close\r\n\r\n"; 
		send(socket, head_p1, sizeof(head_p1), 0);
		printf("%s",head_p1);
		strcat(head_p2, length);
		int i = 0;
		while( head_p2[i] != '\0' ) {      								/* count string size ( may be should to make separate function) */ 		
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
		 p++; 															/* now p -> points to beginning of result */
		 return p;	 
	}
	
	int php_cgi(char *header, int socket) {
																		/* example */
																		/* GET /submit.php?login=hello+world HTTP/1.1 */
		pid_t pid;
		int status;
		char filename[64];
		char parameter[1024];
		bzero(parameter, 1024);
		bzero(filename, 64);
		int i = 0;
		char full_filename[1024];
		bzero(full_filename, 1024);
		
		char *p = strstr(header, "GET");
		if( p != NULL ) {
			p = strchr(header, '/');
			p++;
			if( p != NULL ) {
				while( *p != 32 ) {
					if( *p == '?' ) break;
					filename[i] = *p;
					p++;
					i++;
				}
				p = strchr(header, '?');
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
					strcpy(parameter, "");
				}				
			}
		}
		
		strcpy(full_filename, get_rootdir());    /* do not forget to FREE memory */
		strcat(full_filename, "/");
		strcat(full_filename, filename);
		printf("%s\n",full_filename);
		char buf[1];
		char query_string[1024];
		char script_filename[1024];
		bzero(query_string, 1024);
		bzero(buf, 2);
		strcpy(query_string, "QUERY_STRING=");
		strcat(query_string, parameter);
		strcpy(script_filename, "SCRIPT_FILENAME=");
		strcat(script_filename, full_filename);
		int fd[2], fd2[2];    											/* fd - pipe for output of result by php-cgi,*/
		if( (pipe(fd) != 0) || (pipe(fd2) != 0) )						/* fd2 - pipe for output of errors */
			printf("pipes creation error!\n");							
		if( (pid = fork()) != -1 ) {
			if( pid == 0 ) {
				
				dup2(fd[1], STDOUT_FILENO);								/* redirect stdout to --> pipe write */
				dup2(fd2[1], STDERR_FILENO);							/* redirect stderr to --> pipe write */
				if( strcmp(query_string, "") != 0 ) {   				/* if we call .php script with global POST and GET vars, then set environment's vars */ 
					putenv(query_string);		
					putenv("REDIRECT_STATUS=true");
					putenv("GATEWAY_INTERFACE=CGI/1.1");
					//putenv("REQUEST_METHOD=GET");
				}
				if( parse_for_method(header) ) {
						putenv("REQUEST_METHOD=POST");
					}
					else {
						putenv("REQUEST_METHOD=GET");
				}
                putenv("SERVER_PROTOCOL=HTTP/1.1");
                putenv("HTTP_ACCEPT_CHARSET=ISO-8859-1,utf-8;q=0.7,*;q=0.7");             
                putenv(script_filename); 
                //fcntl(fd[1], F_SETFL, O_NONBLOCK); 						
				if( -1 == execl("/usr/bin/php-cgi", "php-cgi", "-l",  (char*)NULL)) /* calling php-cgi process */
					printf("%s\n","execl error\n");				
				close(fd[1]);
				close(fd[0]);				
				close(fd2[1]);
				close(fd2[0]);				
			}
			else {	
				close(fd[1]);
				close(fd2[1]);
				int i = 0;
				int n = 0;
				char *mem = malloc(1000000); 							/* allocating 1MB of memory for php-cgi output result */
				if( mem == NULL ) {
					perror("can't allocate memory: ");
					return 0;
				}
				char *p = mem;
				char *p_f = mem;
				bzero(mem, 1000000);	
				while( (n = read(fd[0], buf, 1)) > 0 ) { 				/* reading result from pipe --> memory */
					*mem = *buf;
					mem++;
					//printf("%c\n", *mem);	
					i++;
				}	
				//if( i > 0 )	{	
				while( (n = read(fd2[0], buf, 1)) > 0 ) { 				/* reading result from pipe --> memory */
					*mem = *buf;
					mem++;
					//printf("%c\n", *mem);	
					i++;
				//}
			}
				//printf("%d\n",i);
			    waitpid(-1, &status, 0);	/* wait for child process (php-cgi) to terminate */
				if ( (p = remove_phpcgi_header(p)) != NULL ) {
				//p = remove_phpcgi_header(p);
					i -= 38;
					send_header(filename, i, socket);
						 					/* send header */
					while( i != 0 ) {
						send(socket, p, 1, 0);		     					/* send result one-by-one */
						p++;
						i--;
					}
				}
				else {
					send_header(filename, i, socket);
					p = p_f;
					while( i != 0 ) {
						send(socket, p, 1, 0);		     					/* send result one-by-one */
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
		//free(filename);
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
    
    int main(int argc, char* argv[]) {
        int nport = PORT; 
        //nport = atoi("127.0.0.1");								/* get number of port from command line as a parameter  */
                   					/* thread  */
                         					/* holds thread args */
        socklen_t addrlen;
        struct sockaddr_in6 serv_addr, clnt_addr;
        struct hostent;
        bzero(&serv_addr, sizeof(serv_addr));
        serv_addr.sin6_family = AF_INET6; 						/* support ipv6 */
        //serv_addr.sin_addr.s_addr = INADDR_ANY; 				/* only for ipv4 */
        serv_addr.sin6_addr = in6addr_any; 
        serv_addr.sin6_port = htons(nport);
        serv_addr.sin6_scope_id = 5;
        if( (sd = socket(AF_INET6, SOCK_STREAM, 0)) == -1 ) {	/* AF_INET6 socket is supported, compatible with ipv4 */
            perror("error calling socket()"); 					/* socket accepts both ipv4 and ipv6 conectionы. ::1 - localhost in ipv6 */
            exit(EXIT_FAILURE);
        }
        
         if (fcntl(sd, F_SETFL, O_NONBLOCK))
		   {
			  printf("Could not make the socket non-blocking: %m\n");
			  close(sd);
			  return 3;
		   }
		  if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)))
		   {
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
        if( listen(sd, 5) == -1 ) {
            perror("error calling listen()"); 
            close(sd);
            exit(EXIT_FAILURE);
        }
       
        signal(SIGINT, sig_handler); 				/* set signal handler */
            
         efd = epoll_create(5);
		 if (efd < 0) {
			  printf("Could not create the epoll fd: %m");
			  return 1;
		 }

         event.data.fd = sd;
		 event.events = EPOLLIN | EPOLLET;
		 int s = epoll_ctl(efd, EPOLL_CTL_ADD, sd, &event);
		 if (s == -1) {
			  perror ("epoll_ctl");
			  exit(1);
		 }
         events = calloc(MAXEVENTS, sizeof event);
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
										  printf("Accept wouldblock\n");
										  break;
										}
									  else {
										  perror ("accept");
										  break;
										}
									}

								printf("accepted\n");
								s = make_socket_non_blocking (ns);
								if (s == -1)
									exit(1);
								event.events = EPOLLIN |  EPOLLET;
								event.data.fd = ns;
								if (epoll_ctl(efd, EPOLL_CTL_ADD, ns, &event) < 0) {
									  printf("Couldn't add client socket %d to epoll set: %m\n", clientsd);
									  exit(1);      
								}
							} 
							else {
								    int done = 0;
								    ssize_t count;
									char buf[2048];
									bzero(buf, sizeof(buf));
									count = read(events[i].data.fd, buf, sizeof buf);
									printf("%s\n",buf);
									char *f_name = parse_head_for_filename(buf);	
									if ( f_name != NULL) {
									//printf("%s", f_name);
										if (check_content_type(f_name) == 1) {
											read_html_file(f_name, buf, events[i].data.fd);
										}
										else if( check_content_type(f_name) == 0 ) {
											read_media_file(f_name, buf, events[i].data.fd);
										}
										else if( check_content_type(f_name) == 2 ) {
											php_cgi(buf, events[i].data.fd);
										}   
									}
									else close(events[i].data.fd);
									if( count == -1 ) {
										  /* If errno == EAGAIN, that means we have read all
											 data. So go back to the main loop. */
										  if (errno != EAGAIN) {
											  done = 1;
										  }
										  break;
								     }
									else if( count == 0 ) {
										/* End of file. The remote has closed the
											connection. */
											done = 1;
											break;
									}
						     if(done) {
							   printf ("Closed connection on descriptor %d\n", events[i].data.fd);         
							  /* Closing the descriptor will make epoll remove it
								 from the set of descriptors which are monitored. */
							  close (events[i].data.fd);
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
