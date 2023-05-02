#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

#include "common/arguments.h"
#include "common/sockets.h"
#include "common/utility.h"

typedef struct timeval timeval;

int socket_buffer_size(int socket_fd, Direction direction) {
  int return_code;
  int buffer_size;
  socklen_t value_size = sizeof buffer_size;

  // clang-format off
	return_code = getsockopt(
		socket_fd,
		SOL_SOCKET,
		(direction == SEND) ? SO_SNDBUF : SO_RCVBUF,
		&buffer_size,
		&value_size
	);
  // clang-format on

  if (return_code == -1) {
    throw("Error getting buffer size");
  }

  return buffer_size;
}

timeval socket_timeout(int socket_fd, Direction direction) {
  int return_code;
  timeval timeout;
  socklen_t value_size = sizeof timeout;

  // clang-format off
	return_code = getsockopt(
		socket_fd,
		SOL_SOCKET,
		(direction == SEND) ? SO_SNDTIMEO : SO_RCVTIMEO,
		&timeout,
		&value_size
	);
  // clang-format on

  if (return_code == -1) {
    throw("Error getting socket timeout");
  }

  return timeout;
}

double socket_timeout_seconds(int socket_fd, Direction direction) {
  timeval timeout = socket_timeout(socket_fd, direction);
  return timeout.tv_sec + (timeout.tv_usec / 1e6);
}

void set_socket_buffer_size(int socket_fd, Direction direction) {
  int buffer_size = BUFFER_SIZE;

  // set/getsockopt is the one-stop-shop for all socket options.
  // With it, you can get/set a variety of options for a given socket.
  // Arguments:
  // 1. The socket file-fd.
  // 2. The level of the socket, should be SOL_SOCKET.
  // 3. The option you want to get/set.
  // 4. The address of a value you want to get/set into.
  // 5. The memory-size of said value.
  // clang-format off

	// Set the sockets send-buffer-size (SNDBUF)
	int return_code = setsockopt(
		socket_fd,
		SOL_SOCKET,
		(direction == SEND) ? SO_SNDBUF : SO_RCVBUF,
		&buffer_size,
		sizeof buffer_size
	);
  // clang-format on

  if (return_code == -1) {
    throw("Error setting socket buffer size");
  }
}

void set_socket_both_buffer_sizes(int socket_fd) {
  // clang-format off
	set_socket_buffer_size(
		socket_fd,
		SEND
	 );

	set_socket_buffer_size(
		socket_fd,
		RECEIVE
	 );
  // clang-format on
}

void set_socket_timeout(int socket_fd, timeval *timeout, Direction direction) {
  // clang-format off
	int return_code = setsockopt(
		socket_fd,
		SOL_SOCKET,
		(direction == SEND) ? SO_SNDTIMEO : SO_RCVTIMEO,
		timeout,
		sizeof *timeout
	);
  // clang-format on

  if (return_code == -1) {
    throw("Error setting blocking timeout");
  }
}

void set_socket_both_timeouts(int socket_fd, int seconds, int microseconds) {
  struct timeval timeout = {seconds, microseconds};

  set_socket_timeout(socket_fd, &timeout, SEND);
  set_socket_timeout(socket_fd, &timeout, RECEIVE);
}

int set_io_flag(int socket_fd, int flag) {
  int old_flags;

  // Get the old flags, because we must bitwise-OR our flag to add it
  // fnctl takes as arguments:
  // 1. The file fd to modify
  // 2. The command (e.g. F_GETFL, F_SETFL, ...)
  // 3. Arguments to that command (variadic)
  // For F_GETFL, the arguments are ignored (that's why we pass 0)
  if ((old_flags = fcntl(socket_fd, F_GETFL, 0)) == -1) {
    return -1;
  }

  if (fcntl(socket_fd, F_SETFL, old_flags | flag)) {
    return -1;
  }

  return 0;
}

int receive(int connection, void *buffer, int size, int busy_waiting) {
  if (busy_waiting) {
    while (recv(connection, buffer, size, 0) < size) {
      if (errno != EAGAIN)
        return -1;
    }
  } else if (recv(connection, buffer, size, 0) < size) {
    return -1;
  }

  return 0;
}

int get_socket_flags(int socket_fd) {
  int flags;
  if ((flags = fcntl(socket_fd, F_GETFL)) == -1) {
    throw("Error retrieving flags");
  }

  return flags;
}

void set_socket_flags(int socket_fd, int flags) {
  if ((flags = fcntl(socket_fd, F_SETFL, flags)) == -1) {
    throw("Error setting flags");
  }
}

int set_socket_non_blocking(int socket_fd) {
  int flags;

  flags = get_socket_flags(socket_fd);
  flags |= O_NONBLOCK;
  set_socket_flags(socket_fd, flags);

  return flags;
}

int unset_socket_non_blocking(int socket_fd) {
  int flags;

  flags = get_socket_flags(socket_fd);
  // This function is supposed to return the old flags
  set_socket_flags(socket_fd, flags & ~O_NONBLOCK);

  return flags;
}

bool socket_is_non_blocking(int socket_fd) {
  return get_socket_flags(socket_fd) & O_NONBLOCK;
}

static void socket_usage(const char *progname) {
  printf("Usage: %s [OPTION]...\n"
         "  -b <block_size> (default is %d)\n"
         "  -c <count> (default is %d)\n"
         "  -r <rcvbuf_size>\n"
         "  -s <sndbuf_size>\n"
         "  -A <server_address> (default is %s)\n"
         "  -S <server_port> (default is %d)\n"
         "  -M <shmem_backend>\n"
         "  -i <shmem_index> (default is 0)\n"
         "  -d: Disable TCP_NODELAY (default is `enable`)\n"
         "  -N: Non-block mode (default is `false`)\n"
         "  -D: Debug mode (default is `false`)\n",
         progname, DEFAULT_MESSAGE_COUNT, DEFAULT_MESSAGE_SIZE,
         SOCKET_DEFAULT_SERVER_ADDR, SOCKET_DEFAULT_SERVER_PORT);
}
void socket_parse_args(SocketArgs *args, int argc, char *argv[]) {
  int c;

  args->count = DEFAULT_MESSAGE_COUNT;
  args->size = DEFAULT_MESSAGE_SIZE;

  args->rcvbuf_size = -1;
  args->sndbuf_size = -1;

  args->server_addr = SOCKET_DEFAULT_SERVER_ADDR;
  args->server_port = SOCKET_DEFAULT_SERVER_PORT;

  args->shmem_backend = NULL;
  args->shmem_index = 0;

  args->is_nodelay = 1;

  args->is_nonblock = 0;

  args->is_debug = 0;

  while ((c = getopt(argc, argv, "hdNDb:c:r:s:A:S:M:i:")) != -1) {
    switch (c) {
    case 'b': /* Block size */
      args->size = atoi(optarg);
      break;
    case 'c': /* Count */
      args->count = atoi(optarg);
      break;

    case 'r': /* SO_RCVBUF */
      args->rcvbuf_size = atoi(optarg);
      break;
    case 's': /* SO_SNDBUF */
      args->sndbuf_size = atoi(optarg);
      break;

    case 'A': /* Server address */
      args->server_addr = optarg;
      break;
    case 'S': /* Server's port # */
      args->server_port = atoi(optarg);
      break;

    case 'M': /* Path of shared memory backend */
      args->shmem_backend = optarg;
      break;
    case 'i': /* Memory index */
      args->shmem_index = atoi(optarg);
      break;

    case 'd': /* Disable TCP_NODELAY */
      args->is_nodelay = 0;
      break;

    case 'N': /* Non-blocking mode */
      args->is_nonblock = 1;
      break;

    case 'D': /* Debug mode */
      args->is_debug = 1;
      break;

    case 'h': /* help */
    default:
      socket_usage(argv[0]);
      exit(EXIT_FAILURE);
      break;
    }
  }
}
