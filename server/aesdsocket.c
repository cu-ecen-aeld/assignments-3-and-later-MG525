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

#define MYPORT "9000"
#define BACKLOG 10
#define BUFSIZE 5*1024*1024
#define DUMPFILE "/var/tmp/aesdsocketdata"

bool term_int_caught = false;

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
        buffer = malloc(length+1);
        if (buffer)
        {
            fread(buffer, sizeof(char), length+40, f);
            buffer[length] = '\0';
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

void sig_handler(int s)
{
    if (s == SIGINT || s == SIGTERM)
    {
        syslog(LOG_INFO, "Signal Caught INT|TERM");
        term_int_caught = true;
    }
}

int init_sigaction()
{
    struct sigaction sa;
    sa.sa_handler = sig_handler; // reap all dead processes
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_flags = SA_RESTART;
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

    return 0;
}

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
    openlog(NULL, 0, LOG_USER);

    bool isdaemon = false;
    int opt;
    while ((opt = getopt(argc, argv, "d")) != -1) {
        switch (opt) {
        case 'd': isdaemon = true; break;
        default: /* do nothing */ ;
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
    if (bind(sockfd, res->ai_addr, res->ai_addrlen) == -1)
    {
        perror("bind failed: ");
        syslog(LOG_ERR, "bind error\n");
        return 1;
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

    if(isdaemon){
        if(fork()) exit(EXIT_SUCCESS);
    }

    while (!term_int_caught)
    {
        struct sockaddr_storage their_addr;
        memset(&their_addr, 0, sizeof(struct sockaddr_storage));
        socklen_t addr_size = 0;
        int sockfd_accepted = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
        if (sockfd_accepted == -1)
        {
            syslog(LOG_ERR, "accept error\n");
            return 1;
        }
        char s[INET6_ADDRSTRLEN];
        memset(&s, 0, INET6_ADDRSTRLEN);
        inet_ntop(their_addr.ss_family,
                  get_in_addr((struct sockaddr *)&their_addr),
                  s, sizeof s);
        syslog(LOG_INFO, "Accepted connection from %s", s);

        char msg[BUFSIZE];
        memset(&msg, 0, BUFSIZE);
        int recevied_bytes = recv(sockfd_accepted, msg, BUFSIZE, 0);
        if (recevied_bytes == -1)
        {
            syslog(LOG_ERR, "recv error\n");
            return 1;
        }
        else
        {
            write_to_file(DUMPFILE, msg);
            if (!fork())
            {
                close(sockfd);
                char *buffer = read_file_content(DUMPFILE);
                if (buffer)
                {
                    if (send(sockfd_accepted, buffer, strlen(buffer), 0) == -1)
                    {
                        syslog(LOG_ERR, "send error\n");
                    }
                }
                free(buffer);
                close(sockfd_accepted);
                exit(EXIT_SUCCESS);
            }
        }
        close(sockfd_accepted);
    }
    close(sockfd);
    delete_file(DUMPFILE);
    exit(EXIT_SUCCESS);
}