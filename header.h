#define CONFIG_FILE "httpd.conf"
#define MAXEVENTS 64
#define FILE_SIZEBUFFER_LENGTH 9

typedef struct {						/* structure to store POST body string and it's length */
		char post_param[1024];
		int length;
    } post;


void* thread_func(void* arg);
int upload_file(char* content, char* fn, char* end);
    static int PORT;
    static char _ROOT_DIR_[256];
    static char _PHP_CGI_[256];
    static char _PHP_CGI_PATH_[256];
    static char _FILE_UPLOAD_DIR_[256];
	void sig_handler(int sign);    /* signal handler prototype function */
	int nport, nbytes;
    int on = 1;
    int sd;
