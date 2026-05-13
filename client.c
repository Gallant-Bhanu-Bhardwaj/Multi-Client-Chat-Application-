
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <pthread.h>

/* ── tunables ───────────────────────────────────────────── */
#define DEFAULT_HOST  "127.0.0.1"
#define DEFAULT_PORT  9090
#define BUFFER_SIZE   2048

/* ── ANSI colours ───────────────────────────────────────── */
#define RESET    "\033[0m"
#define CYAN     "\033[36m"
#define GREEN    "\033[32m"
#define YELLOW   "\033[33m"
#define RED      "\033[31m"
#define MAGENTA  "\033[35m"
#define BOLD     "\033[1m"

/* ── globals ─────────────────────────────────────────────── */
static volatile int running = 1;
static int          sock_fd = -1;

/* ── prototypes ──────────────────────────────────────────── */
static void *recv_thread(void *arg);
static void  sigint_handler(int sig);
static void  clear_line(void);

 /*  main */
int main(int argc, char *argv[])
{
    const char *host = (argc >= 2) ? argv[1] : DEFAULT_HOST;
    int         port = (argc >= 3) ? atoi(argv[2]) : DEFAULT_PORT;

    signal(SIGINT,  sigint_handler);
    signal(SIGPIPE, SIG_IGN);

    /* ── socket + connect ────────────────────────────────── */
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) { perror("socket"); return EXIT_FAILURE; }

    struct sockaddr_in srv = {
        .sin_family = AF_INET,
        .sin_port   = htons((uint16_t)port)
    };
    if (inet_pton(AF_INET, host, &srv.sin_addr) != 1) {
        fprintf(stderr, "Invalid address: %s\n", host);
        close(sock_fd); return EXIT_FAILURE;
    }
    if (connect(sock_fd, (struct sockaddr *)&srv, sizeof srv) < 0) {
        perror("connect"); close(sock_fd); return EXIT_FAILURE;
    }

    printf(BOLD GREEN
           "\n  ╔══════════════════════════════════════╗\n"
           "  ║    Multi-Client Chat Client     ║\n"
           "  ╚══════════════════════════════════════╝\n" RESET);
    printf(CYAN "  Connected to %s:%d\n\n" RESET, host, port);

    /* ── spawn receive thread ────────────────────────────── */
    pthread_t rtid;
    if (pthread_create(&rtid, NULL, recv_thread, NULL) != 0) {
        perror("pthread_create"); close(sock_fd); return EXIT_FAILURE;
    }
    pthread_detach(rtid);

    /* ── send loop ───────────────────────────────────────── */
    char buf[BUFFER_SIZE];
    while (running) {
        /* The server drives the username prompt itself;
           we just forward whatever the user types.         */
        if (!fgets(buf, sizeof buf, stdin)) break;

        buf[strcspn(buf, "\n")] = '\0';
        if (buf[0] == '\0') continue;

        if (strcmp(buf, "/quit") == 0) {
            send(sock_fd, buf, strlen(buf), MSG_NOSIGNAL);
            break;
        }

        ssize_t sent = send(sock_fd, buf, strlen(buf), MSG_NOSIGNAL);
        if (sent < 0) {
            if (errno != EPIPE) perror("send");
            break;
        }
    }

    running = 0;
    close(sock_fd);
    printf(YELLOW "\nDisconnected. Goodbye!\n" RESET);
    return EXIT_SUCCESS;
}

/* *  recv_thread  –  prints all incoming text from server */
static void *recv_thread(void *arg)
{
    (void)arg;
    char buf[BUFFER_SIZE];

    while (running) {
        memset(buf, 0, sizeof buf);
        int n = recv(sock_fd, buf, sizeof buf - 1, 0);

        if (n <= 0) {
            if (running) {
                clear_line();
                printf(RED "\n  Server closed the connection.\n" RESET);
                running = 0;
            }
            break;
        }

        clear_line();
        printf("%s", buf);
        fflush(stdout);
    }
    return NULL;
}

static void clear_line(void)
{
    printf("\r\033[2K");
    fflush(stdout);
}

static void sigint_handler(int sig)
{
    (void)sig;
    running = 0;
    if (sock_fd >= 0) {
        send(sock_fd, "/quit", 5, MSG_NOSIGNAL);
        close(sock_fd);
    }
    printf(YELLOW "\nInterrupted. Goodbye!\n" RESET);
    exit(EXIT_SUCCESS);
}
