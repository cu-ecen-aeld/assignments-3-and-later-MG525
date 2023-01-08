#include "stdio.h"
#include "syslog.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>
#include <stddef.h>
#include <sys/queue.h>
#include <time.h>
#include <errno.h>

#define MYPORT "9000"
#define BACKLOG 10
#define BUFSIZE 5 * 1024 * 1024
#define DUMPFILE "/var/tmp/aesdsocketdata"
#define ALRM_INT_SEC 10

bool term_int_caught = false;
pthread_mutex_t mutex;

int write_to_file(const char *filename, const char *str)
{
    FILE *file = fopen(filename, "ab");
    if (file == NULL)
    {
        syslog(LOG_ERR, "Cannot open file: %s", filename);
        return 1;
    }
    else
    {
        fputs(str, file);
        fclose(file);
    }
    return 0;
}

char *read_file_content(const char *filename)
{
    char *buffer = NULL;
    long length;
    FILE *f = fopen(filename, "rb");
    if (f)
    {
        fseek(f, 0, SEEK_END);
        length = ftell(f);
        fseek(f, 0, SEEK_SET);
        buffer = malloc(length + 1);
        if (buffer)
        {
            int count = fread(buffer, sizeof(char), length + 40, f);
            buffer[count] = '\0';
        }
        fclose(f);
    }
    return buffer;
}

int file_exists(const char *filename)
{
    return access(filename, F_OK);
}

int delete_file(const char *filename)
{
    if (file_exists(filename) == 0)
    {
        if (remove(filename) == 0)
        {
            return 0;
        }
        else
        {
            perror("File Delete issue");
            syslog(LOG_ERR, "file %s cannot be deleted", filename);
            return 1;
        }
    }
    return 0;
}

struct recv_send_socket_data
{
    bool thread_complete;
    bool success;
    int sockfd_accepted;
    pthread_mutex_t *mutex;
    pthread_t *thread;
    SLIST_ENTRY(recv_send_socket_data)
    entries; /* Singly linked list */
};

void *recv_send_socket_thread(void *thread_param)
{
    syslog(LOG_INFO, "Started Thread!");

    struct recv_send_socket_data *thread_func_args = (struct recv_send_socket_data *)thread_param;
    // obtain
    if (pthread_mutex_lock(thread_func_args->mutex) != 0)
    {
        syslog(LOG_ERR, "\n thread failed! .. mutex obtaining failed\n");
    }
    else
    {
        char msg[BUFSIZE];
        memset(&msg, 0, BUFSIZE);
        int recevied_bytes = recv(thread_func_args->sockfd_accepted, msg, BUFSIZE, 0);
        if (recevied_bytes == -1)
        {
            syslog(LOG_ERR, "\n thread failed! .. recv error\n");
        }
        else
        {
            if (!write_to_file(DUMPFILE, msg))
            {
                char *buffer = read_file_content(DUMPFILE);
                if (buffer)
                {
                    if (send(thread_func_args->sockfd_accepted, buffer, strlen(buffer), 0) == -1)
                    {
                        syslog(LOG_ERR, "\n thread failed! .. send error\n");
                    }
                    else
                    {
                        thread_func_args->success = true;
                    }
                }
                free(buffer);
            }
            else
            {
                syslog(LOG_ERR, "\n thread failed! .. write to file fail\n");
            }
        }

        // release
        if (pthread_mutex_unlock(thread_func_args->mutex) != 0)
        {
            syslog(LOG_ERR, "\n thread failed! .. mutex releasing failed\n");
        }
        else
        {
            thread_func_args->thread_complete = true;
        }
    }
    close(thread_func_args->sockfd_accepted);
    return thread_param;
}

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

void time_func(char *outstr)
{
    time_t t;
    struct tm *tmp;

    t = time(NULL);
    tmp = localtime(&t);
    if (tmp == NULL)
    {
        syslog(LOG_ERR, "localtime failed");
    }

    if (strftime(outstr, 200, "%a, %d %b %Y %T %z", tmp) == 0)
    {
        syslog(LOG_ERR, "strftime returned 0");
    }
}

void write_time_to_file(const char *filename)
{
    char *t = malloc(sizeof(char) * 100);
    memset(t, 0, 100);
    time_func(t);

    char *tmp = "timestamp:";

    char *result = malloc(strlen(t) + strlen(tmp) + 2);
    memset(result, 0, strlen(t) + strlen(tmp) + 2);

    strcpy(result, tmp);
    strcat(result, t);
    strcat(result, "\n");

    write_to_file(filename, result);
    free(t);
    free(result);
}

struct write_time_to_file_data
{
    bool thread_complete;
    pthread_t *thread;
    pthread_mutex_t *mutex;
};

void *write_time_to_file_thread(void *thread_param)
{
    syslog(LOG_INFO, "Write Time Started Thread!");
    struct write_time_to_file_data *thread_func_args = (struct write_time_to_file_data *)thread_param;
    // obtain
    if (pthread_mutex_lock(thread_func_args->mutex) != 0)
    {
        syslog(LOG_ERR, "\n thread failed! .. mutex obtaining failed\n");
    }
    else
    {
        write_time_to_file(DUMPFILE);
        // release
        if (pthread_mutex_unlock(thread_func_args->mutex) != 0)
        {
            syslog(LOG_ERR, "\n thread failed! .. mutex releasing failed\n");
        }
    }
    alarm(ALRM_INT_SEC);
    thread_func_args->thread_complete = true;
    return thread_param;
}

void sig_handler(int s)
{
    if (s == SIGINT || s == SIGTERM)
    {
        syslog(LOG_INFO, "Signal Caught INT|TERM");
        term_int_caught = true;
    }
    else if (s == SIGALRM)
    {
        syslog(LOG_INFO, "Signal Caught ALRM");
        pthread_t write_time_thread;
        struct write_time_to_file_data *thread_time_args = malloc(sizeof(struct write_time_to_file_data));
        thread_time_args->mutex = &mutex;
        thread_time_args->thread = &write_time_thread;
        thread_time_args->thread_complete = false;
        int err = pthread_create(&write_time_thread, NULL, &write_time_to_file_thread, thread_time_args);
        if (err != 0)
        {
            syslog(LOG_ERR, "\ncan't create thread");
            free(thread_time_args);
            term_int_caught = true; // End Program
        }
        pthread_join(write_time_thread, NULL);
        free(thread_time_args);
    }
}

int init_sigaction()
{
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = sig_handler;
    if (sigaction(SIGTERM, &sa, NULL) == -1)
    {
        perror("init_sigaction SIGTERM failed: ");
        syslog(LOG_ERR, "init_sigaction SIGTERM failed");
        return 1;
    }

    if (sigaction(SIGINT, &sa, NULL) == -1)
    {
        perror("init_sigaction SIGINT failed: ");
        syslog(LOG_ERR, "init_sigaction SIGINT failed");
        return 1;
    }

    if (sigaction(SIGALRM, &sa, NULL) == -1)
    {
        perror("init_sigaction SIGALRM failed: ");
        syslog(LOG_ERR, "init_sigaction SIGALRM failed");
        return 1;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    openlog(NULL, 0, LOG_USER);

    bool isdaemon = false;
    int opt;
    while ((opt = getopt(argc, argv, "d")) != -1)
    {
        switch (opt)
        {
        case 'd':
            isdaemon = true;
            break;
        default: /* do nothing */;
        }
    }

    int rc = init_sigaction();
    if (rc != 0)
    {
        syslog(LOG_ERR, "init_sigaction error\n");
        return 1;
    }

    const char *node = NULL; // localhost
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

    if ((getaddrinfo(node, MYPORT, &hints, &res)) != 0)
    {
        perror("getaddrinfo failed: ");
        syslog(LOG_ERR, "getaddrinfo error\n");
        return 1;
    }

    int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

    int yes = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1)
    {
        perror("setsockopt failed: ");
        syslog(LOG_ERR, "setsockopt error\n");
        return 1;
    }
    while (bind(sockfd, res->ai_addr, res->ai_addrlen) == -1)
    {
        usleep(1 * 1000 * 1000);
        syslog(LOG_ERR, "bind error\n");
    }

    freeaddrinfo(res);

    if (listen(sockfd, BACKLOG) == -1)
    {
        perror("listen failed: ");
        syslog(LOG_ERR, "listen error\n");
        return 1;
    }

    rc = delete_file(DUMPFILE);
    if (rc != 0)
    {
        syslog(LOG_ERR, "delete_file error\n");
        return 1;
    }

    if (isdaemon)
    {
        if (fork())
            exit(EXIT_SUCCESS);
    }

    if (pthread_mutex_init(&mutex, NULL) != 0)
    {
        syslog(LOG_ERR, "\n mutex init failed\n");
        return 1;
    }

    SLIST_HEAD(slisthead, recv_send_socket_data);
    struct slisthead head;
    SLIST_INIT(&head);

    alarm(ALRM_INT_SEC);
    while (!term_int_caught)
    {
        struct sockaddr_storage their_addr;
        memset(&their_addr, 0, sizeof(struct sockaddr_storage));
        socklen_t addr_size = 0;
        int sockfd_accepted = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
        if (sockfd_accepted == -1)
        {
            syslog(LOG_ERR, "accept error\n");
            if(errno == EINTR) continue;
            else return 1;
        }
        char s[INET6_ADDRSTRLEN];
        memset(&s, 0, INET6_ADDRSTRLEN);
        inet_ntop(their_addr.ss_family,
                  get_in_addr((struct sockaddr *)&their_addr),
                  s, sizeof s);
        syslog(LOG_INFO, "Accepted connection from %s", s);

        pthread_t thread;
        struct recv_send_socket_data *thread_func_args = malloc(sizeof(struct recv_send_socket_data));
        thread_func_args->thread_complete = false;
        thread_func_args->success = false;
        thread_func_args->sockfd_accepted = sockfd_accepted;
        thread_func_args->mutex = &mutex;
        thread_func_args->thread = &thread;
        int err = pthread_create(&thread, NULL, &recv_send_socket_thread, thread_func_args);
        if (err != 0)
        {
            syslog(LOG_ERR, "\ncan't create thread");
            free(thread_func_args);
            return 1;
        }
        else
        {
            // This is not a good implementation since we start searching from previous Head everytime and we are inserting new thread as Head
            // Incase the previous thread is not finished then we inserted a new head then most probably the next interation would also find the new head not finished yet
            // But it works anyway our program is not that complicated and threads are executed fast enough
            struct recv_send_socket_data *n1;
            n1 = SLIST_FIRST(&head);
            while (n1 && n1->thread_complete)
            {
                pthread_join(*(n1->thread), NULL);
                SLIST_REMOVE_HEAD(&head, entries);
                free(n1);
                n1 = SLIST_FIRST(&head);
            }
            SLIST_INSERT_HEAD(&head, thread_func_args, entries);
        }
    }
    close(sockfd);
    delete_file(DUMPFILE);
    exit(EXIT_SUCCESS);
}