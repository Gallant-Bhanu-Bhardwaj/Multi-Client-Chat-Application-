# 💬 Multi-Client Chat Application

A TCP-based multi-client chat server built in C on Linux using POSIX threads and raw socket programming. Supports real-time public messaging, private messaging, and unique username registration all from the terminal.

## Features


-  Multi-client support — up to 50 concurrent users over TCP
-  Username registration — unique name enforced on every connect
-  Public messaging — broadcast to all connected users instantly
-  Private messaging — `/msg <username> <text>` seen only by sender and recipient
-  Live user list — `/list` shows everyone currently online
-  Username change — `/name <new_name>` with real-time uniqueness check
-  POSIX threads — one dedicated thread per client connection
- Thread-safe broadcasting — mutex-protected shared client table
-  Graceful shutdown — `Ctrl+C` notifies all clients before the server exits


## Installation

Make sure you have GCC and Make installed on Linux:

```bash
sudo apt update && sudo apt install gcc make
```

Clone the Repository
```bash
git clone https://github.com/your-username/chat-app.git
cd chat-app
```
Build
```bash
make   
```

This produces two binaries in the same folder:
- ./server — the chat server
- ./client — the chat client

To remove binaries:
```bash
make clean
```


## Run Locally

Step 1 — Start the server (Terminal 1)

```bash
  ./server 9090
```

Step 2 — Connect first client (Terminal 2)

```bash
  ./client 127.0.0.1 9090
```
When prompted, enter a username: Bhanu

Step 3 — Connect second client (Terminal 3)
```bash
  ./client 127.0.0.1 9090
```
When prompted, enter a username: Raj

Step 4 — Start chatting
```bash
# Public message (everyone sees it)
hello everyone!

# Private message (only Raj sees it)
/msg Raj hey, this is private!

# See who is online
/list

# Change your username
/name Bhanu99

# Disconnect
/quit
```

## Command Reference

| Command | Description |
|---|---|
| `<text>` | Send a public message to all users |
| `/msg \`<username>\` \`<text>\`` | Send a private message |
| `/list` | Show all currently online users |
| `/name \`<new_name>\`` | Change your display name |
| `/quit` | Disconnect from the server |


## Key APIs Used

| Function | Purpose |
|---|---|
| `socket()` | Create a TCP socket |
| `bind()` | Bind socket to IP and port |
| `listen()` | Mark socket as passive (ready to accept) |
| `accept()` | Accept an incoming client connection |
| `send()` / `recv()` | Transmit and receive data over TCP |
| `pthread_create()` | Spawn a thread per client |
| `pthread_mutex_lock()` | Protect shared client table |## 📸 Screenshots

### Server terminal — live connection and message logs

```text
╔══════════════════════════════════════╗
║    Multi-Client Chat Server ║
╚══════════════════════════════════════╝
Port 9090  |  max 50 clients

[10:21:03] [CONNECT   ] fd=4 uid=1 127.0.0.1:54312
[10:21:05] [JOIN      ] Bhanu (uid 1)
[10:21:09] [CONNECT   ] fd=5 uid=2 127.0.0.1:54318
[10:21:11] [JOIN      ] Raj (uid 2)
[10:21:20] [MSG       ] <Bhanu> hello Raj!
[10:21:25] [PM        ] Bhanu -> Raj: this is private
[10:21:40] [RENAME    ] Raj -> Raj99
[10:21:55] [DISCONNECT] Bhanu


### Client terminal (Bhanu)

```text
╔══════════════════════════════════════╗
║    Multi-Client Chat Client ║
╚══════════════════════════════════════╝
Connected to 127.0.0.1:9090

Choose a username (1-31 chars, no spaces): Bhanu
Logged in as: Bhanu

Commands:
  /msg  <username> <text>   private message
  /list                     show online users
  /name <new_name>          change username
  /quit                     disconnect

hello Raj!
[10:21:25] [PM to Raj]: this is private
[10:21:30] Raj: hey Bhanu, got your message!

*** Raj is now known as Raj99 ***

/list
-- Online (2) --
• Bhanu (you)
• Raj99
```

## 🤝 Contributing

Contributions are welcome! Here's how:

1. Fork the repository
2. Create a feature branch

```bash
git checkout -b feature/your-feature-name
```

3. Commit your changes

```bash
git commit -m "Add: your feature description"
```

4. Push to your branch

```bash
git push origin feature/your-feature-name
```

5. Open a Pull Request on GitHub

### Ideas for contribution

- Add password-protected rooms / channels
- Add message history log to a file
- Add a `/kick` command for server admin
- Port the client to a simple GUI using `ncurses`

Please follow the existing code style — C11, no external libraries beyond `pthreads`.

## References

- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/?utm_source=chatgpt.com) — the gold standard reference for socket programming in C
- [POSIX Threads Programming — LLNL](https://hpc-tutorials.llnl.gov/posix/?utm_source=chatgpt.com) — comprehensive pthreads documentation
- [Linux man-pages Project](https://man7.org/linux/man-pages/?utm_source=chatgpt.com) — `man 2 socket`, `man 2 accept`, `man 3 pthread_create`
- [readme.so](https://readme.so?utm_source=chatgpt.com) — used for README section structure

