#include "io_helper.h"
#include "request.h"

#define MAX_BUFFER (8192)

//
//  TODO: add code to create and manage the buffer
//

// 1 some imp details highlighted with colors
void cyan()
{
    printf("\e[1;36m");
}

void red()
{
    printf("\033[1;31m");
}

void yellow()
{
    printf("\033[1;33m");
}

void reset()
{
    printf("\033[0m");
}

// 2 structure of objs in buffer
struct buffer_obj
{
    int fd;
    char *site_file_name;
    int site_file_size;
    struct buffer_obj *next;
};

// 2 struct file_q a circular queue to manage
struct file_q
{
    int number_of_objs;
    struct buffer_obj *start;
    struct buffer_obj *end;
};

// 4 to delete obj from buffer
struct file_q *d_q_obj(struct file_q *q, struct buffer_obj **del_obj)
{
    struct buffer_obj *temp;
    if (q->start == NULL)
    {

        return NULL;
    }
    temp = q->start;
    q->start = q->start->next;
    if (q->start == NULL)
        q->end = NULL;
    temp->next = NULL;
    q->number_of_objs = q->number_of_objs - 1;
    *del_obj = temp;
    yellow();
    printf("Request for site_file Name %s  Size %d is removed from buffer\n\n", temp->site_file_name, temp->site_file_size);
    reset();
    return q;
}

// 3 creating queue(buffer) of files
struct file_q *create()
{
    struct file_q *q = (struct file_q *)malloc(sizeof(struct file_q));
    q->number_of_objs = 0;
    q->start = NULL;
    q->end = NULL;
    return q;
}

// 3 creating new file obj as a required structure
struct buffer_obj *createNewobj(int fd, char *site_file_name, int site_file_size)
{
    struct buffer_obj *new_obj = (struct buffer_obj *)malloc(sizeof(struct buffer_obj));
    new_obj->fd = fd;
    new_obj->site_file_name = site_file_name;
    new_obj->site_file_size = site_file_size;
    new_obj->next = NULL;
    return new_obj;
}

//
// Sends out HTTP response in case of errors
//
void request_error(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
    char buf[MAX_BUFFER], body[MAX_BUFFER];

    // Create the body of error message first (have to know its length for starter)

    sprintf(body, ""
                  "<!doctype html>\r\n"
                  "<start>\r\n"
                  "  <title>OSTEP WebServer Error</title>\r\n"
                  "</start>\r\n"
                  "<body>\r\n"
                  "  <h2>%s: %s</h2>\r\n"
                  "  <p>%s: %s</p>\r\n"
                  "</body>\r\n"
                  "</html>\r\n",
            errnum, shortmsg, longmsg, cause);

    // Write out the starter information for this response
    red();

    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    write_or_die(fd, buf, strlen(buf));

    sprintf(buf, "Content-Type: text/html\r\n");
    write_or_die(fd, buf, strlen(buf));

    sprintf(buf, "Content-Length: %lu\r\n\r\n", strlen(body));
    write_or_die(fd, buf, strlen(buf));
    reset();

    // Write out the body last
    write_or_die(fd, body, strlen(body));

    // close the socket connection
    close_or_die(fd);
}

//
// Reads and discards everything up to an file_buffer_empty text line
//
void request_read_starters(int fd)
{
    char buf[MAX_BUFFER];

    readline_or_die(fd, buf, MAX_BUFFER);
    while (strcmp(buf, "\r\n"))
    {
        readline_or_die(fd, buf, MAX_BUFFER);
    }
    return;
}

//
// Return 1 if static, 0 if dynamic content (executable site_file)
// Calculates site_filename (and cgiargs, for dynamic) from uri
//
int request_parse_uri(char *uri, char *site_filename, char *cgiargs)
{
    char *ptr;

    if (!strstr(uri, "cgi"))
    {
        // static
        strcpy(cgiargs, "");
        sprintf(site_filename, ".%s", uri);
        if (uri[strlen(uri) - 1] == '/')
        {
            strcat(site_filename, "index.html");
        }
        return 1;
    }
    else
    {
        // dynamic
        ptr = index(uri, '?');
        if (ptr)
        {
            strcpy(cgiargs, ptr + 1);
            *ptr = '\0';
        }
        else
        {
            strcpy(cgiargs, "");
        }

        cyan();
        sprintf(site_filename, ".%s", uri);
        reset();
        return 0;
    }
}

int file_q_created = 0;                                      // 1 checks whether document queue is created or not
struct file_q *file_Queue = NULL;                       // 2 initializing
pthread_mutex_t m1 = PTHREAD_MUTEX_INITIALIZER;                   // 2 lock
pthread_cond_t file_buffer_empty = PTHREAD_COND_INITIALIZER; // 5 condition variable
pthread_cond_t file_buffer_fill = PTHREAD_COND_INITIALIZER;  // 5 condition variable

//
// file_buffer_fills in the site_filetype given the site_filename
//
void request_get_site_filetype(char *site_filename, char *site_filetype)
{
    if (strstr(site_filename, ".html"))
        strcpy(site_filetype, "text/html");
    else if (strstr(site_filename, ".gif"))
        strcpy(site_filetype, "image/gif");
    else if (strstr(site_filename, ".jpg"))
        strcpy(site_filetype, "image/jpeg");
    else
        strcpy(site_filetype, "text/plain");
}

//
// Handles requests for static content
//
void request_serve_static(int fd, char *site_filename, int site_filesize)
{
    int srcfd;
    char *srcp, site_filetype[MAX_BUFFER], buf[MAX_BUFFER];

    request_get_site_filetype(site_filename, site_filetype);
    srcfd = open_or_die(site_filename, O_RDONLY, 0);

    // Rather than call read() to read the site_file into memory,
    // which would require that we allocate a buffer, we memory-map the site_file
    srcp = mmap_or_die(0, site_filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    close_or_die(srcfd);

    // put together response
    yellow();
    sprintf(buf, ""
                 "HTTP/1.0 200 OK\r\n"
                 "Server: OSTEP WebServer\r\n"
                 "Content-Length: %d\r\n"
                 "Content-Type: %s\r\n\r\n",
            site_filesize, site_filetype);
    reset();
    write_or_die(fd, buf, strlen(buf));

    //  Writes out to the client socket the memory-mapped site_file
    write_or_die(fd, srcp, site_filesize);
    munmap_or_die(srcp, site_filesize);
}

// Inserting according to scheduling policies if schedule_class 0 then first in first out else shortest file first
struct file_q *file_buffer_schedulling_algo(struct file_q *q, int fd, char *site_file_name, int site_file_size, int schedule_class)
{
    pthread_mutex_lock(&m1);
    while (q->number_of_objs == buffer_max_size)
    {
        pthread_cond_wait(&file_buffer_empty, &m1);
    }
    struct buffer_obj *new_obj = createNewobj(fd, site_file_name, site_file_size);
    if (schedule_class == 0) // for first in first out
    {
        if (q->start == NULL)
        {
            q->start = new_obj;
            q->end = new_obj;
        }
        else
        {
            q->end->next = new_obj;
            q->end = q->end->next;
        }
        q->number_of_objs = q->number_of_objs + 1;
        cyan();
        printf("No of objs in buffer %d\n", q->number_of_objs);

        printf("Added to First come First serve \n");
        reset();
    }
    else // for shortest file first
    {
        struct file_q *temp = q;
        struct file_q *ptr = NULL;
        if (temp->start == NULL || new_obj->site_file_size < temp->start->site_file_size)
        {
            new_obj->next = temp->start;
            temp->start = new_obj;
        }
        else
        {
            ptr = q;
            while (ptr->start->next != NULL && (new_obj->site_file_size > ptr->start->next->site_file_size))
            {
                ptr->start = ptr->start->next;
            }
            new_obj->next = ptr->start->next;
            ptr->start->next = new_obj;
        }
        q->number_of_objs = q->number_of_objs + 1;
        cyan();
        printf("No of objs in buffer %d\n", q->number_of_objs);

        printf("Added to Shortest Doc First \n");

        reset();
    }
    pthread_cond_signal(&file_buffer_fill);
    pthread_mutex_unlock(&m1);
    return q;
}

void *thread_request_serve_static(void *arg)
{
    // TODO: write code to actualy respond to HTTP requests
    struct buffer_obj *n = NULL;
    pthread_mutex_lock(&m1);
    if (file_q_created == 0)
    {
        file_Queue = create();
        file_q_created = 1;
    }
    pthread_mutex_unlock(&m1);
    while (1)
    {
        pthread_mutex_lock(&m1);

        while (file_Queue->number_of_objs == 0)
        {
            pthread_cond_wait(&file_buffer_fill, &m1);
        }

        cyan();
        file_Queue = d_q_obj(file_Queue, &n);
        reset();
        pthread_cond_signal(&file_buffer_empty);
        pthread_mutex_unlock(&m1);
        if (n != NULL)
        {
            request_serve_static(n->fd, n->site_file_name, n->site_file_size);
        }
    }
    return NULL;
}

//
// Initial handling of the request
//
void request_handle(int fd)
{
    int is_static;
    struct stat sbuf;
    char buf[MAX_BUFFER], method[MAX_BUFFER], uri[MAX_BUFFER], version[MAX_BUFFER];
    char site_filename[MAX_BUFFER], cgiargs[MAX_BUFFER];

    // get the request type, site_file path and HTTP version
    readline_or_die(fd, buf, MAX_BUFFER);
    sscanf(buf, "%s %s %s", method, uri, version);
    printf("method:%s uri:%s version:%s\n", method, uri, version);
    

    // verify if the request type is GET is not
    if (strcasecmp(method, "GET"))
    {

        request_error(fd, method, "501", "Not Implemented", "server does not implement this method");

        return;
    }
    request_read_starters(fd);

    // check requested content type (static/dynamic)
    is_static = request_parse_uri(uri, site_filename, cgiargs);

    // Security check to prohibit traversing up in the file system 
    if(strstr(site_filename, "..")) {
		request_error(fd, site_filename, "403", "Forbidden", "Traversing up in filesystem is now allowed");
		return;
	}
    // get some data regarding the requested site_file, also check if requested site_file is present on server
    if (stat(site_filename, &sbuf) < 0)
    {
        request_error(fd, site_filename, "404", "Not found", "server could not find this site_file");
        return;
    }

    // verify if requested content is static
    if (is_static)
    {
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
        {
            request_error(fd, site_filename, "403", "Forbidden", "server could not read this site_file");
            return;
        }
        // code for producer

        // TODO: write code to add HTTP requests in the buffer based on the scheduling policy
        yellow();
        file_Queue = file_buffer_schedulling_algo(file_Queue  , fd, site_filename, sbuf.st_size, scheduling_algo); // inserting into struct file_q according to schedule
        reset();
                                                                                                                                     // pthread_cond_signal(&p_cv);
    }
    else
    {
        request_error(fd, site_filename, "501", "Not Implemented", "server does not serve dynamic content request");
    }
}
