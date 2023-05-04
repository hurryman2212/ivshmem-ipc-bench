#ifndef SOCKETS_H
#define SOCKETS_H

#include <arpa/inet.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/types.h>

/******************** DEFINITIONS ********************/

#define BUFFER_SIZE 64000

typedef enum Direction { SEND, RECEIVE } Direction;

struct timeval;
typedef struct timeval timeval;

/******************** INTERFACE ********************/

int socket_buffer_size(int socket_fd, Direction direction);

void set_socket_buffer_size(int socket_fd, Direction direction);
void set_socket_both_buffer_sizes(int socket_fd);

timeval socket_timeout(int socket_fd, Direction direction);
double socket_timeout_seconds(int socket_fd, Direction direction);

void set_socket_timeout(int socket_fd, timeval *timeout, Direction direction);
void set_socket_both_timeouts(int socket_fd, int seconds, int microseconds);

int get_socket_flags(int socket_fd);
void set_socket_flags(int socket_fd, int flags);

int set_socket_non_blocking(int socket_fd);
int unset_socket_non_blocking(int socket_fd);

bool socket_is_non_blocking(int socket_fd);

int set_io_flag(int socket_fd, int flag);

int receive(int connection, void *buffer, int size, int busy_waiting);

typedef struct SocketArgs {
  int count;
  int size;

  int rcvbuf_size;
  int sndbuf_size;

  const char *server_addr;
  int server_port;

  const char *shmem_backend;
  int shmem_index;

  int is_nodelay;
  int is_cork;

  int wait_all;

  int is_nonblock;

  int is_debug;
} SocketArgs;
void socket_parse_args(SocketArgs *args, int argc, char *argv[]);

void socket_tcp_read_data(int fd, void *buffer, size_t size,
                          struct SocketArgs *args);
void socket_tcp_write_data(int fd, void *buffer, size_t size,
                           struct SocketArgs *args);

void socket_udp_read_data(int fd, void *buffer, size_t size,
                          struct sockaddr_in *peer_addr, socklen_t *sock_len,
                          struct SocketArgs *args);
void socket_udp_write_data(int fd, void *buffer, size_t size,
                           const struct sockaddr_in *peer_addr,
                           socklen_t sock_len, struct SocketArgs *args);

#define SOCKET_DEFAULT_SERVER_ADDR "127.0.0.1"
#define SOCKET_DEFAULT_SERVER_PORT 12345

#endif /* SOCKETS_H */
