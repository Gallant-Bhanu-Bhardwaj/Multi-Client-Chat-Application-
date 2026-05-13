/*
 * ============================================================
 *  Multi-Client Chat Server  v2.0
 *  TCP/IP  |  POSIX Threads  |  Linux
 *
 *  New in v2:
 *    • Username registration on connect (unique, enforced)
 *    • Private messaging  /msg <username> <text>
 * ============================================================
 *
 *  Build:  gcc -Wall -O2 -o server server.c -lpthread
 *  Run:    ./server [port]          (default: 9090)
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <stdarg.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <pthread.h>

/* ── tunables ───────────────────────────────────────────── */
#define DEFAULT_PORT    9090
#define MAX_CLIENTS     50
#define BUFFER_SIZE     2048
#define NAME_LEN        32

/* ── ANSI colours ────────────────────────────────────────── */
#define RESET    "\033[0m"
#define RED      "\033[31m"
#define GREEN    "\033[32m"
#define YELLOW   "\033[33m"
#define CYAN     "\033[36m"
#define MAGENTA  "\033[35m"
#define BOLD     "\033[1m"

/* ── client descriptor ───────────────────────────────────── */
typedef struct {
    int       fd;
    int       uid;
    char      name[NAME_LEN];       /* empty until registered */
    char      ip[INET_ADDRSTRLEN];
    int       port;
    pthread_t tid;
} Client;

/* ── globals ─────────────────────────────────────────────── */
static Client         *clients[MAX_CLIENTS];
static pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
static volatile int    server_fd     = -1;
static int             client_count  = 0;
static int             uid_counter   = 1;

/* ── prototypes ──────────────────────────────────────────── */
static void  add_client(Client *c);
static void  remove_client(int uid);
static void  broadcast(const char *msg, int sender_uid);
static int   send_private(const char *target, const char *msg,
                           int sender_uid);
static void  send_to(int fd, const char *msg);
static int   name_taken(const char *name);
static void *handle_client(void *arg);
static void  server_log(const char *colour, const char *tag,
                        const char *fmt, ...);
static void  timestamp(char *buf, size_t len);
static void  sigint_handler(int sig);

/* ═══════════════════════════════════════════════════════════
 *  main
 * ═══════════════════════════════════════════════════════════ */
int main(int argc, char *argv[])
{
    int port = (argc >= 2) ? atoi(argv[1]) : DEFAULT_PORT;
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port.\n"); return EXIT_FAILURE;
    }

    signal(SIGINT,  sigint_handler);
    signal(SIGPIPE, SIG_IGN);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); return EXIT_FAILURE; }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);

    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port        = htons((uint16_t)port)
    };
    if (bind(server_fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
        perror("bind"); close(server_fd); return EXIT_FAILURE;
    }
    if (listen(server_fd, 10) < 0) {
        perror("listen"); close(server_fd); return EXIT_FAILURE;
    }

    printf(BOLD GREEN
           "\n  ╔══════════════════════════════════════╗\n"
           "  ║    Multi-Client Chat Server v2.0     ║\n"
           "  ╚══════════════════════════════════════╝\n" RESET);
    printf(CYAN "  Port %d  |  max %d clients\n\n" RESET,
           port, MAX_CLIENTS);

    /* ── accept loop ─────────────────────────────────────── */
    while (1) {
        struct sockaddr_in cli_addr;
        socklen_t cli_len = sizeof cli_addr;
        int cli_fd = accept(server_fd,
                            (struct sockaddr *)&cli_addr, &cli_len);
        if (cli_fd < 0) {
            if (errno == EINTR) break;
            perror("accept"); continue;
        }

        pthread_mutex_lock(&clients_mutex);
        int full = (client_count >= MAX_CLIENTS);
        pthread_mutex_unlock(&clients_mutex);

        if (full) {
            send(cli_fd,
                 "SERVER: Server full. Try later.\r\n", 33, 0);
            close(cli_fd); continue;
        }

        Client *c = calloc(1, sizeof *c);
        if (!c) { close(cli_fd); continue; }

        c->fd   = cli_fd;
        c->port = ntohs(cli_addr.sin_port);
        inet_ntop(AF_INET, &cli_addr.sin_addr, c->ip, sizeof c->ip);

        pthread_mutex_lock(&clients_mutex);
        c->uid = uid_counter++;
        pthread_mutex_unlock(&clients_mutex);

        c->name[0] = '\0';   /* not yet registered */
        add_client(c);

        if (pthread_create(&c->tid, NULL, handle_client, c) != 0) {
            perror("pthread_create");
            remove_client(c->uid);
            close(cli_fd); free(c);
        } else {
            pthread_detach(c->tid);
        }
    }

    close(server_fd);
    printf(GREEN "\nServer shut down cleanly.\n" RESET);
    return EXIT_SUCCESS;
}

/* ═══════════════════════════════════════════════════════════
 *  handle_client  –  one thread per connection
 * ═══════════════════════════════════════════════════════════ */
static void *handle_client(void *arg)
{
    Client *c   = (Client *)arg;
    char    buf[BUFFER_SIZE];
    char    msg[BUFFER_SIZE + NAME_LEN * 2 + 64];

    server_log(CYAN, "CONNECT", "fd=%d uid=%d %s:%d",
               c->fd, c->uid, c->ip, c->port);

    /* ══════════════════════════════════════════════════════
     *  PHASE 1 – Username registration
     * ══════════════════════════════════════════════════════ */
    send_to(c->fd,
            "\r\n"
            "  ╔══════════════════════════════════╗\r\n"
            "  ║   Welcome to the Chat Server!    ║\r\n"
            "  ╚══════════════════════════════════╝\r\n\r\n"
            "  Choose a username (1-31 chars, no spaces): ");

    while (c->name[0] == '\0') {
        memset(buf, 0, sizeof buf);
        int n = recv(c->fd, buf, sizeof buf - 1, 0);
        if (n <= 0) goto cleanup;

        buf[strcspn(buf, "\r\n")] = '\0';
        size_t len = strlen(buf);

        if (len < 1 || len >= NAME_LEN) {
            send_to(c->fd,
                    "  Must be 1-31 characters. Try again: ");
            continue;
        }
        if (strchr(buf, ' ')) {
            send_to(c->fd,
                    "  No spaces allowed. Try again: ");
            continue;
        }
        if (name_taken(buf)) {
            send_to(c->fd,
                    "  Username taken. Try another: ");
            continue;
        }

        pthread_mutex_lock(&clients_mutex);
        strncpy(c->name, buf, NAME_LEN - 1);
        c->name[NAME_LEN - 1] = '\0';
        pthread_mutex_unlock(&clients_mutex);
    }

    /* ── registration confirmed ─────────────────────────── */
    snprintf(msg, sizeof msg,
             "\r\n" GREEN "  Logged in as: " BOLD "%s" RESET "\r\n\r\n"
             "  Commands:\r\n"
             "    /msg  <username> <text>   private message\r\n"
             "    /list                     show online users\r\n"
             "    /name <new_name>          change username\r\n"
             "    /quit                     disconnect\r\n\r\n",
             c->name);
    send_to(c->fd, msg);

    snprintf(msg, sizeof msg,
             YELLOW "  *** %s joined the chat ***\r\n" RESET, c->name);
    broadcast(msg, c->uid);
    server_log(YELLOW, "JOIN", "%s (uid %d)", c->name, c->uid);

    /* ══════════════════════════════════════════════════════
     *  PHASE 2 – Chat loop
     * ══════════════════════════════════════════════════════ */
    while (1) {
        memset(buf, 0, sizeof buf);
        int n = recv(c->fd, buf, sizeof buf - 1, 0);
        if (n <= 0) { if (n < 0) perror("recv"); break; }

        buf[strcspn(buf, "\r\n")] = '\0';
        if (buf[0] == '\0') continue;

        /* ── /quit ──────────────────────────────────────── */
        if (strcmp(buf, "/quit") == 0) break;

        /* ── /list ──────────────────────────────────────── */
        if (strcmp(buf, "/list") == 0) {
            char list[BUFFER_SIZE];
            snprintf(list, sizeof list,
                     "  ── Online (%d) ──\r\n", client_count);
            pthread_mutex_lock(&clients_mutex);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i] && clients[i]->name[0] != '\0') {
                    char entry[NAME_LEN + 16];
                    if (clients[i]->uid == c->uid)
                        snprintf(entry, sizeof entry,
                                 "  • %s (you)\r\n", clients[i]->name);
                    else
                        snprintf(entry, sizeof entry,
                                 "  • %s\r\n", clients[i]->name);
                    strncat(list, entry,
                            sizeof list - strlen(list) - 1);
                }
            }
            pthread_mutex_unlock(&clients_mutex);
            strncat(list, "  ─────────────────\r\n",
                    sizeof list - strlen(list) - 1);
            send_to(c->fd, list);
            continue;
        }

        /* ── /name <new_name> ───────────────────────────── */
        if (strncmp(buf, "/name ", 6) == 0) {
            const char *nn = buf + 6;
            size_t nlen = strlen(nn);
            if (nlen < 1 || nlen >= NAME_LEN) {
                send_to(c->fd, "  Name must be 1-31 chars.\r\n");
                continue;
            }
            if (strchr(nn, ' ')) {
                send_to(c->fd, "  No spaces in username.\r\n");
                continue;
            }
            if (name_taken(nn)) {
                send_to(c->fd, "  That name is already taken.\r\n");
                continue;
            }
            char old[NAME_LEN];
            strncpy(old, c->name, NAME_LEN);
            pthread_mutex_lock(&clients_mutex);
            strncpy(c->name, nn, NAME_LEN - 1);
            c->name[NAME_LEN - 1] = '\0';
            pthread_mutex_unlock(&clients_mutex);

            snprintf(msg, sizeof msg,
                     YELLOW "  *** %s is now known as %s ***\r\n" RESET,
                     old, c->name);
            broadcast(msg, -1);
            server_log(YELLOW, "RENAME", "%s -> %s", old, c->name);
            continue;
        }

        /* ── /msg <username> <text> ─────────────────────── */
        if (strncmp(buf, "/msg ", 5) == 0) {
            char *rest  = buf + 5;
            char *space = strchr(rest, ' ');

            if (!space || *(space + 1) == '\0') {
                send_to(c->fd,
                        "  Usage: /msg <username> <message>\r\n");
                continue;
            }
            *space = '\0';
            const char *target = rest;
            const char *text   = space + 1;

            if (strcmp(target, c->name) == 0) {
                send_to(c->fd,
                        "  You cannot PM yourself.\r\n");
                continue;
            }

            char ts[32];
            timestamp(ts, sizeof ts);

            /* what the recipient sees */
            char to_recv[BUFFER_SIZE + NAME_LEN * 2 + 32];
            snprintf(to_recv, sizeof to_recv,
                     MAGENTA "[%s] [PM from %s]: %s\r\n" RESET,
                     ts, c->name, text);

            /* echo back to sender */
            char to_self[BUFFER_SIZE + NAME_LEN * 2 + 32];
            snprintf(to_self, sizeof to_self,
                     MAGENTA "[%s] [PM to %s]: %s\r\n" RESET,
                     ts, target, text);

            if (send_private(target, to_recv, c->uid)) {
                send_to(c->fd, to_self);
                server_log(MAGENTA, "PM",
                           "%s -> %s: %s", c->name, target, text);
            } else {
                snprintf(msg, sizeof msg,
                         "  User '%s' is not online.\r\n", target);
                send_to(c->fd, msg);
            }
            continue;
        }

        /* ── unknown command ────────────────────────────── */
        if (buf[0] == '/') {
            send_to(c->fd,
                    "  Unknown command. "
                    "Try /msg /list /name /quit\r\n");
            continue;
        }

        /* ── public message ─────────────────────────────── */
        char ts[32];
        timestamp(ts, sizeof ts);
        snprintf(msg, sizeof msg,
                 BOLD "[%s] %s" RESET ": %s\r\n",
                 ts, c->name, buf);
        broadcast(msg, c->uid);
        server_log(GREEN, "MSG", "<%s> %s", c->name, buf);
    }

cleanup:
    if (c->name[0] != '\0') {
        snprintf(msg, sizeof msg,
                 YELLOW "  *** %s left the chat ***\r\n" RESET,
                 c->name);
        broadcast(msg, c->uid);
        server_log(RED, "DISCONNECT",
                   "%s (uid %d)", c->name, c->uid);
    } else {
        server_log(RED, "DISCONNECT",
                   "(unregistered) uid=%d", c->uid);
    }

    remove_client(c->uid);
    close(c->fd);
    free(c);
    return NULL;
}

/* ═══════════════════════════════════════════════════════════
 *  Client-table helpers
 * ═══════════════════════════════════════════════════════════ */
static void add_client(Client *c)
{
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i]) { clients[i] = c; client_count++; break; }
    }
    pthread_mutex_unlock(&clients_mutex);
}

static void remove_client(int uid)
{
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && clients[i]->uid == uid) {
            clients[i] = NULL; client_count--; break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

static void broadcast(const char *msg, int sender_uid)
{
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i]
            && clients[i]->uid    != sender_uid
            && clients[i]->name[0] != '\0')
        {
            send_to(clients[i]->fd, msg);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

/* returns 1 on success, 0 if target not found */
static int send_private(const char *target,
                        const char *msg, int sender_uid)
{
    int found = 0;
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i]
            && clients[i]->uid != sender_uid
            && strcmp(clients[i]->name, target) == 0)
        {
            send_to(clients[i]->fd, msg);
            found = 1; break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    return found;
}

static int name_taken(const char *name)
{
    int taken = 0;
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && strcmp(clients[i]->name, name) == 0) {
            taken = 1; break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    return taken;
}

static void send_to(int fd, const char *msg)
{
    ssize_t total = 0, len = (ssize_t)strlen(msg);
    while (total < len) {
        ssize_t n = send(fd, msg + total,
                         (size_t)(len - total), MSG_NOSIGNAL);
        if (n < 0) break;
        total += n;
    }
}

/* ─── utility ────────────────────────────────────────────── */
static void server_log(const char *colour, const char *tag,
                       const char *fmt, ...)
{
    char ts[32]; timestamp(ts, sizeof ts);
    char body[512];
    va_list ap; va_start(ap, fmt);
    vsnprintf(body, sizeof body, fmt, ap);
    va_end(ap);
    printf("%s[%s] [%-10s] %s%s\n", colour, ts, tag, body, RESET);
    fflush(stdout);
}

static void timestamp(char *buf, size_t len)
{
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    strftime(buf, len, "%H:%M:%S", tm);
}

static void sigint_handler(int sig)
{
    (void)sig;
    printf(RED "\n\nCaught SIGINT – shutting down...\n" RESET);
    if (server_fd >= 0) close(server_fd);
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i]) {
            send_to(clients[i]->fd,
                    "\r\nSERVER: Server shutting down. Goodbye!\r\n");
            close(clients[i]->fd);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    exit(EXIT_SUCCESS);
}