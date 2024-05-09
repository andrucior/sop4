#include "l4-common.h"
#define BACKLOG_SIZE 16
#define MAX_EVENTS 4

volatile sig_atomic_t work = 1;

void sigint_handler(int sigNo) { work = 0; }

void serverWork(int tcp_listen_socket)
{
    int epoll_descriptor;
    char cities[20];
    for (int i = 0; i < 20; i++)
        cities[i] = 'g';
    if ((epoll_descriptor = epoll_create1(0)) < 0)
    {
        ERR("epoll_create:");
    }

    struct epoll_event event, events[MAX_EVENTS];
    event.events = EPOLLIN;
    event.data.fd = tcp_listen_socket;

    if (epoll_ctl(epoll_descriptor, EPOLL_CTL_ADD, tcp_listen_socket, &event) ==                                                                                                              -1)
    {
        perror("epoll_ctl: listen_sock");
        exit(EXIT_FAILURE);
    }
    int clients = 0;
    int clients_fd[4];
    int nfds;
    char data[4];
    ssize_t size;

    while (work)
    {
        if ((nfds = epoll_wait(epoll_descriptor, events, MAX_EVENTS, -1)) > 0)
        {
            for (int n = 0; n < nfds; n++)
            {
                if (events[n].data.fd == tcp_listen_socket)
                {
                    if (clients < MAX_EVENTS)
                    {
                        int client_socket = add_new_client(events[n].data.fd);
                        clients_fd[clients++] = events[n].data.fd;
                        struct epoll_event ev;
                        ev.events = EPOLLIN;
                        ev.data.fd = client_socket;

                        if ((size = bulk_read(client_socket, (char*)data, sizeof                                                                                                             (char[4]))) < 0)
                            ERR("read:");
                        if (size == sizeof(char[4]))
                        {
                            data[3] = '\0';
                            fprintf(stdout, "%s\n", data);
                        }
                        if (epoll_ctl(epoll_descriptor, EPOLL_CTL_ADD, client_so                                                                                                             cket, &ev) < 0)
                        {
                            perror("epoll_ctl EPOLL_CTL_ADD");
                            exit(EXIT_FAILURE);
                        }
                        // if (close(client_socket) < 0)
                        //     ERR("close");
                    }
                    else
                    {
                        fprintf(stderr, "Max clients reached. Rejecting\n");
                        // break;
                    }
                }
                else
                {
                    char message[4];

                    ssize_t bytesR;
                    if ((bytesR = bulk_read(events[n].data.fd, (char*)message, s                                                                                                             izeof(char[4]))) <= 0)
                    {
                        for (int i = 0; i < clients; ++i)
                        {
                            if (clients_fd[i] == events[n].data.fd)
                            {
                                for (int j = i; j < clients - 1; ++j)
                                {
                                    clients_fd[j] = clients_fd[j + 1];
                                }

                                break;
                            }
                        }
                        close(events[n].data.fd);
                        clients--;
                        if (bytesR == 0)
                        {
                            if (epoll_ctl(epoll_descriptor, EPOLL_CTL_DEL, event                                                                                                             s[n].data.fd, NULL) < 0)
                            {
                                perror("epoll_ctl()");
                                exit(EXIT_FAILURE);
                            }


                        }
                        else if (errno != EAGAIN)
                        {
                            perror("read");
                            exit(EXIT_FAILURE);
                        }

                        continue;
                    }
                    if (bytesR == sizeof(char[4]))
                    {
                        message[3] = '\0';
                        fprintf(stdout, "%s\n", message);

                        int first = (message[1]) - '0';
                        int scnd = (message[2]) - '0';
                        int which = 10 * first + scnd;
                        fprintf(stdout, "%d\n", which);

                        if (which < 0 || which >= 20)
                        {
                            fprintf(stderr, "Incorrect index, number should be b                                                                                                             etween 0 and 19\n");
                            continue;
                        }
                        if (message[0] != 'p' && message[0] != 'g')
                        {
                            fprintf(stderr, "Incorrect letter - should be either                                                                                                              g or p\n");
                            continue;
                        }

                        if (cities[which] != message[0])
                        {
                            cities[which] = message[0];
                            for (int i = 0; i < clients; i++)
                            {
                                if (clients_fd[i] != events[n].data.fd)
                                {
                                    if ((bytesR = bulk_write(events[n].data.fd,                                                                                                              (char*)message, sizeof(char[4]))) < 0)
                                        ERR("write");
                                }
                            }
                        }
                    }
                }
            }
        }
        else
        {
            if (errno == EINTR)
                continue;
            ERR("epoll_pwait");
        }
    }
    if (close(epoll_descriptor) < 0)
        ERR("close");

    for (int i = 0; i < 20; i++)
    {
        if (cities[i] == 'g')
        {
            printf("City %d belongs to Greeks\n", i);
        }
        else
        {
            printf("City %d belongs to Spartans\n", i);
        }
    }
}

int main(int argc, char** argv)
{
    if (argc != 2)
        usage(argv[0]);
    int port = atoi(argv[1]);
    int tcpSocket = bind_tcp_socket(port, BACKLOG_SIZE);
    if (tcpSocket < 0)
    {
        perror("bind_tcp_socket");
        exit(EXIT_FAILURE);
    }

    if (sethandler(sigint_handler, SIGINT))
        ERR("set_handler");
    serverWork(tcpSocket);
    if (close(tcpSocket) < 0)
        ERR("close");

    fprintf(stderr, "Server has terminated.\n");
    return EXIT_SUCCESS;
}
