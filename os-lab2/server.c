#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#define DEFAULT_PORT 5000
#define BACKLOG      16

volatile sig_atomic_t was_sighup = 0;
volatile sig_atomic_t was_sigterm = 0;

void sighup_handler(int signo)  { (void)signo; was_sighup = 1; }
void sigterm_handler(int signo) { (void)signo; was_sigterm = 1; }

int create_server_socket(uint16_t port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) { perror("socket"); return -1; }

    int yes = 1;
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1 ||
        bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1 ||
        listen(fd, BACKLOG) == -1 ||
        fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK) == -1) {
        perror("create_server_socket");
        close(fd);
        return -1;
    }
    return fd;
}

int setup_signals(sigset_t *orig_mask, sigset_t *wait_mask) {
    const int sigs[] = { SIGHUP, SIGTERM, SIGINT };
    sigset_t blocked;
    sigemptyset(&blocked);
    for (int i = 0; i < 3; i++) sigaddset(&blocked, sigs[i]);
    if (sigprocmask(SIG_BLOCK, &blocked, orig_mask) == -1) { perror("sigprocmask"); return -1; }

    *wait_mask = *orig_mask;
    for (int i = 0; i < 3; i++) sigdelset(wait_mask, sigs[i]);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);
    for (int i = 0; i < 3; i++) {
        sa.sa_handler = (sigs[i] == SIGHUP) ? sighup_handler : sigterm_handler;
        if (sigaction(sigs[i], &sa, NULL) == -1) { perror("sigaction"); return -1; }
    }
    return 0;
}

void handle_new_connection(int server_fd, int *client_fd) {
    struct sockaddr_in peer;
    socklen_t len = sizeof(peer);
    int fd = accept(server_fd, (struct sockaddr *)&peer, &len);
    if (fd == -1) {
        if (errno != EAGAIN && errno != EWOULDBLOCK && errno != ECONNABORTED && errno != EINTR)
            perror("accept");
        return;
    }
    char ip[INET_ADDRSTRLEN] = "?";
    inet_ntop(AF_INET, &peer.sin_addr, ip, sizeof(ip));

    int keep = (*client_fd == -1 && fd < FD_SETSIZE);
    printf("Новое соединение %s:%d (fd %d): %s\n", ip, ntohs(peer.sin_port), fd,
           keep ? "принято" : "закрыто, клиент уже подключён");
    if (keep) *client_fd = fd; else close(fd);
    fflush(stdout);
}

void handle_client_data(int *client_fd) {
    char buf[4096];
    ssize_t n = recv(*client_fd, buf, sizeof(buf), 0);
    if (n == -1 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK))
        return;
    if (n > 0) {
        printf("Получено байт: %zd\n", n);
    } else {
        if (n == 0) printf("Клиент отключился\n"); else perror("recv");
        close(*client_fd);
        *client_fd = -1;
    }
    fflush(stdout);
}

int main(int argc, char *argv[]) {
    long port = (argc > 1) ? strtol(argv[1], NULL, 10) : DEFAULT_PORT;
    if (port <= 0 || port > 65535) { fprintf(stderr, "Некорректный порт\n"); return EXIT_FAILURE; }

    int server_fd = create_server_socket((uint16_t)port);
    if (server_fd == -1) return EXIT_FAILURE;

    sigset_t orig_mask, wait_mask;
    if (setup_signals(&orig_mask, &wait_mask) == -1) { close(server_fd); return EXIT_FAILURE; }

    printf("Сервер запущен: PID %ld, порт %ld\n", (long)getpid(), port);
    fflush(stdout);

    int client_fd = -1, exit_code = EXIT_SUCCESS;
    while (!was_sigterm) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(server_fd, &rfds);
        if (client_fd != -1) FD_SET(client_fd, &rfds);
        int max_fd = (client_fd > server_fd) ? client_fd : server_fd;

        if (pselect(max_fd + 1, &rfds, NULL, NULL, NULL, &wait_mask) == -1) {
            if (errno != EINTR) { perror("pselect"); exit_code = EXIT_FAILURE; break; }
            if (was_sighup) {
                was_sighup = 0;
                printf("Получен сигнал SIGHUP\n");
                fflush(stdout);
            }
            continue;
        }
        if (client_fd != -1 && FD_ISSET(client_fd, &rfds)) handle_client_data(&client_fd);
        if (FD_ISSET(server_fd, &rfds)) handle_new_connection(server_fd, &client_fd);
    }

    printf("Завершение работы\n");
    if (client_fd != -1) close(client_fd);
    close(server_fd);
    sigprocmask(SIG_SETMASK, &orig_mask, NULL);
    return exit_code;
}
