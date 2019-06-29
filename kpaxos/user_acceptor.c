#include "chardevice_message.h"
#include "getopt.h"
#include "poll.h"
#include "user_eth.h"
#include "user_levent.h"
#include "user_storage.h"
#include <lmdb.h>
#include <paxos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int           stop = 1;
struct lmdb_storage* stor;
char error_msg[] = "\x1b[31m[user_acceptor] Error while:\x1b[0m";

int acceptor_write_file(struct server* serv, char* msg, size_t msg_size);

void
stop_execution(int signo)
{
  stop = 0;
}

struct kernel_msg
{
  int  msg_type;
  char value[0];
};

static void
store_paxos_accepted(paxos_accepted* acc)
{
  // printf("[user_acceptor] acc->aid:          [%zu]\n", (unsigned
  // long)acc->aid); printf("[user_acceptor] acc->iid:          [%zu]\n",
  // (unsigned long)acc->iid); printf("[user_acceptor] acc->promise_iid:
  // [%zu]\n",
  //        (unsigned long)acc->promise_iid);
  // printf("[user_acceptor] acc->ballot:       [%zu]\n",
  //        (unsigned long)acc->ballot);
  // printf("[user_acceptor] acc->value_ballot: [%zu]\n",
  //        (unsigned long)acc->value_ballot);

  if (lmdb_storage_tx_begin(stor) != 0)
    printf("%s lmdb_storage_tx_begin\n", error_msg);
  if (lmdb_storage_put(stor, acc) != 0) {
    lmdb_storage_tx_abort(stor);
    printf("%s lmdb_storage_put\n", error_msg);
  }
  if (lmdb_storage_tx_commit(stor) != 0)
    printf("%s lmdb_storage_tx_commit\n", error_msg);
  printf("[user_acceptor] lmdb_storage_put done\n");
}

static void
handle_prepare(struct server* serv, paxos_prepare* prepare)
{
  paxos_accepted acc;
  memset(&acc, 0, sizeof(paxos_accepted));
  if (lmdb_storage_tx_begin(stor) != 0)
    printf("%s lmdb_storage_tx_begin\n", error_msg);

  int found = lmdb_storage_get(stor, prepare->iid, &acc);
  if (!found || acc.ballot <= prepare->ballot) {
    acc.aid = serv->fileop.char_device_id;
    acc.iid = prepare->iid;
    acc.ballot = prepare->ballot;
    if (lmdb_storage_put(stor, &acc) != 0) {
      lmdb_storage_tx_abort(stor);
      printf("%s storage_tx_abort\n", error_msg);
    }
  }

  if (lmdb_storage_tx_commit(stor) != 0)
    printf("%s lmdb_storage_tx_commit\n", error_msg);

  char* buffer = malloc(sizeof(int) + sizeof(paxos_accepted));
  if (buffer == NULL)
    printf("%s malloc returned NULL\n", error_msg);
  int msg_type = PREPARE;
  memcpy(buffer, &msg_type, sizeof(int));
  memcpy(&buffer[sizeof(int)], &acc, sizeof(paxos_accepted));
  acceptor_write_file(serv, buffer, sizeof(int) + sizeof(paxos_accepted));
  free(buffer);
}

static void
unpack_message(struct server* serv, size_t len)
{
  struct user_msg*   mess = (struct user_msg*)serv->ethop.rec_buffer;
  struct kernel_msg* received_msg = (struct kernel_msg*)mess->value;
  switch (received_msg->msg_type) {
    case STORE_STATE:
      store_paxos_accepted((struct paxos_accepted*)received_msg->value);
      break;

    case PREPARE:
      handle_prepare(serv, (struct paxos_prepare*)received_msg->value);
      break;

    default:
      printf("[unpack_message] unrecognized msg_type: %d\n",
             received_msg->msg_type);
      break;
  }
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
  struct pollfd pol[1]; // 1 events: chardevice

  pol[0].fd = serv->fileop.fd;
  pol[0].events = POLLIN;

  while (stop) {
    poll(pol, 1, -1);
    if (pol[0].revents & POLLIN) {
      read_file(serv);
    }
  }
}

int
acceptor_write_file(struct server* serv, char* msg, size_t msg_size)
{
  printf("[user_acceptor] Acceptor write to LKM\n");

  // Send the string to the LKM
  int ret = write(serv->fileop.fd, msg, msg_size);

  if (ret < 0) {
    perror("Failed to write the message to the device.");
    return -1;
  }
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

  printf("[user_acceptor] if_name %s\n", serv->ethop.if_name);
  printf("[user_acceptor] chardevice /dev/paxos/kacceptor%c\n",
         serv->fileop.char_device_id + '0');
  signal(SIGINT, stop_execution);

  if (open_file(&serv->fileop)) {
    printf("[user_acceptor] Failed to open chardev\n");
    goto cleanup;
  }

  stor = lmdb_storage_new(serv->fileop.char_device_id);
  if (lmdb_storage_open(stor) != 0) {
    printf("[user_acceptor] lmdb_storage_open failed\ngoto cleanup\n");
    goto cleanup;
  }

  char msg[] = "user_acceptor ready";
  if (acceptor_write_file(serv, msg, sizeof(msg))) {
    printf("[user_acceptor] acceptor_write_file failed");
    goto cleanup;
  }

  make_acceptor(serv);
  return 0;

cleanup:
  lmdb_storage_close(stor);
  server_free(serv);
  free(stor);
  return 0;
}
