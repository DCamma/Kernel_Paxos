#include "getopt.h"
#include "poll.h"
#include "user_eth.h"
#include "user_levent.h"
#include "user_storage.h"
#include <lmdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int stop = 1;

void
stop_execution(int signo)
{
  stop = 0;
}

static void
unpack_message(struct server* serv, size_t len)
{
  struct user_msg*       mess = (struct user_msg*)serv->ethop.rec_buffer;
  struct paxos_accepted* acc = (struct paxos_accepted*)mess->value;

  printf("[user_acceptor] acc->aid:          [%lu]\n", (unsigned long)acc->aid);
  printf("[user_acceptor] acc->iid:          [%lu]\n", (unsigned long)acc->iid);
  printf("[user_acceptor] acc->promise_iid:  [%lu]\n",
         (unsigned long)acc->promise_iid);
  printf("[user_acceptor] acc->ballot:       [%lu]\n",
         (unsigned long)acc->ballot);
  printf("[user_acceptor] acc->value_ballot: [%lu]\n",
         (unsigned long)acc->value_ballot);
}

static void
read_file(struct server* serv)
{
  int len = read(serv->fileop.fd, serv->ethop.rec_buffer, ETH_DATA_LEN);
  if (len < 0)
    return;

  if (len == 0) {
    printf("[user_acceptor] Stopped by kernel module\n");
    stop = 0;
  }
  unpack_message(serv, len);
}

static void
make_acceptor(struct server* serv)
{
  printf("[user_acceptor] Make acceptor\n");
  struct pollfd pol[1]; // 1 events: socket and file

  pol[0].fd = serv->fileop.fd;
  pol[0].events = POLLIN;

  while (stop) {
    poll(pol, 1, -1);
    if (pol[0].revents & POLLIN) { // communicate to chardevice via file
      read_file(serv);
    }
  }
}

int
acceptor_write_file(struct server* serv)
{
  printf("[user_acceptor] Acceptor write to LKM\n");
  char message[] = "User app ready\n";

  // Send the string to the LKM
  int ret = write(serv->fileop.fd, message, sizeof(message));

  if (ret < 0) {
    perror("Failed to write the message to the device.");
    return -1;
  }
  printf("[user_acceptor] Ret: %d\n", ret);
  make_acceptor(serv);
  return 0;
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

  struct lmdb_storage* stor = lmdb_storage_new(serv->fileop.char_device_id);
  if (lmdb_storage_open(stor) != 0) {
    printf("[user_acceptor] lmdb_storage_open failed\ngoto cleanup\n");
    goto cleanup;
  }

  printf("[user_acceptor] lmdb_storage_open ok\n");

  printf("[user_acceptor] if_name %s\n", serv->ethop.if_name);
  printf("[user_acceptor] chardevice /dev/paxos/kacceptor%c\n",
         serv->fileop.char_device_id + '0');
  signal(SIGINT, stop_execution);

  if (open_file(&serv->fileop)) {
    printf("[user_acceptor] Failed to open chardev\n");
    goto cleanup;
  }

  // if (acceptor_write_file(serv)) {
  //   printf("[user_acceptor] acceptor_write_file failed");
  //   goto cleanup;
  // }
  make_acceptor(serv);
  return 0;

cleanup:
  server_free(serv);
  free(stor);
  return 0;
}
