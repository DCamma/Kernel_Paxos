#include "getopt.h"
#include "poll.h"
#include "user_eth.h"
#include "user_levent.h"
#include <stdio.h>

static int stop = 1;

void
stop_execution(int signo)
{
  stop = 0;
}

static void
unpack_message(struct server* serv, size_t len)
{
  // struct user_msg* mess = (struct user_msg*)serv->ethop.rec_buffer;
  // struct client_value* val = (struct client_value*)mess->value;
  char* val = (char*)serv->ethop.rec_buffer;
  printf("Unpack message, value: [%s]\n", val);
  // printf("Client received value %.16s with %zu bytes\n", val);
  // struct connection* conn = find_connection(serv, val->client_id);
  // if (conn)
  // eth_sendmsg(&serv->ethop, conn->address, mess, len);
}

static void
read_file(struct server* serv)
{
  int len = read(serv->fileop.fd, serv->ethop.rec_buffer, ETH_DATA_LEN);
  if (len < 0)
    return;

  if (len == 0) {
    printf("Stopped by kernel module\n");
    stop = 0;
  }
  printf("message len: %d\n", len);
  for (int i = 0; i < 8; i++) {
    printf("%d, ", (int)*(serv->ethop.rec_buffer + i));
    // printf("%s\n", rec_buffer + i);
  }
  printf("\n%s\n", (serv->ethop.rec_buffer + 8));
  // unpack_message(serv, len);
}

static void
make_acceptor(struct server* serv)
{
  struct pollfd pol[1]; // 1 events: socket and file

  // chardevice
  if (open_file(&serv->fileop)) {
    printf("Debug-C: goto cleanup");
    goto cleanup;
  }

  pol[0].fd = serv->fileop.fd;
  pol[0].events = POLLIN;

  while (stop) {
    poll(pol, 1, -1);
    if (pol[0].revents & POLLIN) { // communicate to chardevice via file
      read_file(serv);
    }
  }

cleanup:
  server_free(serv);
}

static void
check_args(int argc, char* argv[], struct server* serv)
{
  int opt = 0, idx = 0;

  static struct option options[] = { { "chardev_id", required_argument, 0,
                                       'c' },
                                     { "if_name", required_argument, 0, 'i' },
                                     { "help", no_argument, 0, 'h' },
                                     { 0, 0, 0, 0 } };

  while ((opt = getopt_long(argc, argv, "c:i:h", options, &idx)) != -1) {
    switch (opt) {
      case 'c':
        serv->fileop.char_device_id = atoi(optarg);
        break;
      case 'i':
        serv->ethop.if_name = optarg;
        break;
      default:
        usage(argv[0], 0);
    }
  }
}

int
main(int argc, char* argv[])
{
  struct server* serv = server_new();
  serv->ethop.if_name = "enp0s3";
  serv->fileop.char_device_id = 0;
  serv->fileop.char_device_name = "/dev/paxos/kacceptor0";
  new_connection_list(serv);

  check_args(argc, argv, serv);

  printf("if_name %s\n", serv->ethop.if_name);
  printf("chardevice /dev/paxos/kacceptor%c\n",
         serv->fileop.char_device_id + '0');
  signal(SIGINT, stop_execution);
  make_acceptor(serv);

  return 0;
}
