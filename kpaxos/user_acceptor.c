#include "chardevice_message.h"
#include "getopt.h"
// #include "paxos.h"
#include "poll.h"
#include "user_eth.h"
#include "user_levent.h"
#include "user_storage.h"
#include <lmdb.h>
#include <paxos.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int           stop = 1;
struct lmdb_storage* stor;

void
print_error(char* str)
{
  printf("\x1b[31mError\x1b[0m %s line: %d,  %s\n", __FILE__, __LINE__, str);
}

int acceptor_write_file(struct server* serv, char* msg, size_t msg_size);

void
stop_execution(int signo)
{
  stop = 0;
}

struct kernel_msg // TODO: add value len
{
  int     msg_type;
  uint8_t src[6];
  char    value[0];
};

void
paxos_accepted_to_promise(paxos_accepted* acc, paxos_message* out)
{
  out->type = PAXOS_PROMISE;
  out->u.promise = (paxos_promise){ acc->aid,
                                    acc->iid,
                                    acc->ballot,
                                    acc->value_ballot,
                                    { acc->value.paxos_value_len,
                                      acc->value.paxos_value_val } };
}

void
paxos_accept_to_accepted(int id, paxos_accept* acc, paxos_message* out)
{
  char*  value;
  size_t value_size = acc->value.paxos_value_len;
  if (value_size > 0) {
    value = malloc(value_size);
    memcpy(value, acc->value.paxos_value_val, value_size);
  }
  out->type = PAXOS_ACCEPTED;
  out->u.accepted =
    (paxos_accepted){ .aid = id,
                      .iid = acc->iid,
                      .promise_iid = 0,
                      .ballot = acc->ballot,
                      .value_ballot = acc->ballot,
                      .value = (paxos_value){ value_size, value } };
}

void
paxos_accepted_to_preempted(int id, paxos_accepted* acc, paxos_message* out)
{
  out->type = PAXOS_PREEMPTED;
  out->u.preempted = (paxos_preempted){ id, acc->iid, acc->ballot };
}

void
paxos_value_free(paxos_value* v)
{
  free(v->paxos_value_val);
  free(v);
}

static void
paxos_value_destroy(paxos_value* v)
{
  if (v->paxos_value_len > 0)
    free(v->paxos_value_val);
}

void
paxos_accepted_free(paxos_accepted* a)
{
  paxos_accepted_destroy(a);
  free(a);
}

void
paxos_promise_destroy(paxos_promise* p)
{
  paxos_value_destroy(&p->value);
}

void
paxos_accept_destroy(paxos_accept* p)
{
  paxos_value_destroy(&p->value);
}

void
paxos_accepted_destroy(paxos_accepted* p)
{
  paxos_value_destroy(&p->value);
}

void
paxos_client_value_destroy(paxos_client_value* p)
{
  paxos_value_destroy(&p->value);
}

void
paxos_message_destroy(paxos_message* m)
{
  switch (m->type) {
    case PAXOS_PROMISE:
      paxos_promise_destroy(&m->u.promise);
      break;
    case PAXOS_ACCEPT:
      paxos_accept_destroy(&m->u.accept);
      break;
    case PAXOS_ACCEPTED:
      paxos_accepted_destroy(&m->u.accepted);
      break;
    case PAXOS_CLIENT_VALUE:
      paxos_client_value_destroy(&m->u.client_value);
      break;
    default:
      break;
  }
}

static void
accepted_to_kspace(struct server* serv, uint8_t* src, paxos_accepted* acc,
                   size_t size_acc)
{
  size_t len = acc->value.paxos_value_len;
  size_t total_size = sizeof(int) + 6 * sizeof(uint8_t) + size_acc + len;
  char*  buffer = malloc(total_size);
  if (buffer == NULL)
    print_error("malloc returned NULL\n");
  int    msg_type = PREPARE;
  size_t pad = sizeof(int);
  memcpy(buffer, &msg_type, pad);
  memcpy(buffer + pad, src, 6 * sizeof(uint8_t));
  pad += 6 * sizeof(uint8_t);
  memcpy(buffer + pad, acc, size_acc);
  if (len > 0) {
    pad += size_acc;
    memcpy(buffer + pad, acc->value.paxos_value_val, len);
  }

  acceptor_write_file(serv, buffer, total_size);
  free(buffer);
}

static void
repeat_to_kspace(struct server* serv, uint8_t* src, paxos_accepted* acc,
                 size_t size_acc)
{
  size_t len = acc->value.paxos_value_len;
  size_t total_size = sizeof(int) + 6 * sizeof(uint8_t) + size_acc + len;
  char*  buffer = malloc(total_size);
  if (buffer == NULL)
    print_error("malloc returned NULL\n");
  int    msg_type = REPEAT;
  size_t pad = sizeof(int);
  memcpy(buffer, &msg_type, pad);
  memcpy(buffer + pad, src, 6 * sizeof(uint8_t));
  pad += 6 * sizeof(uint8_t);
  memcpy(buffer + pad, acc, size_acc);
  if (len > 0) {
    pad += size_acc;
    memcpy(buffer + pad, acc->value.paxos_value_val, len);
  }

  acceptor_write_file(serv, buffer, total_size);
  free(buffer);
}

static void
accept_to_kspace(struct server* serv, uint8_t* src, paxos_message* out,
                 size_t message_size)
{
  // out can be either of type accepted or preempted
  // IF accepted then has paxos value

  size_t total_size = sizeof(int) + 6 * sizeof(uint8_t) + message_size;
  size_t value_len = 0;
  if (out->type == PAXOS_ACCEPTED) {
    value_len = out->u.accepted.value.paxos_value_len;
    total_size += value_len;
  }

  char* buffer = malloc(total_size);
  if (buffer == NULL)
    print_error("malloc returned NULL\n");
  int    msg_type = ACCEPT;
  size_t pad = sizeof(int);
  memcpy(buffer, &msg_type, pad);
  memcpy(buffer + pad, src, 6 * sizeof(uint8_t));
  pad += 6 * sizeof(uint8_t);
  memcpy(buffer + pad, out, message_size);
  if (out->type == PAXOS_ACCEPTED) {
    pad += value_len;
    memcpy(buffer + pad, out->u.accepted.value.paxos_value_val, value_len);
  }

  acceptor_write_file(serv, buffer, total_size);
  free(buffer);
}

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
    print_error("lmdb_storage_tx_begin\n");
  if (lmdb_storage_put(stor, acc) != 0) {
    lmdb_storage_tx_abort(stor);
    print_error("lmdb_storage_put\n");
  }
  if (lmdb_storage_tx_commit(stor) != 0)
    print_error("lmdb_storage_tx_commit\n");
}

static paxos_accepted*
handle_prepare(struct server* serv, paxos_prepare* prepare)
{
  paxos_accepted* acc = malloc(sizeof(paxos_accepted));
  // memset(&acc, 0, sizeof(paxos_accepted));
  if (lmdb_storage_tx_begin(stor) != 0)
    print_error("lmdb_storage_tx_begin\n");

  int found = lmdb_storage_get(stor, prepare->iid, acc);
  if (!found || acc->ballot <= prepare->ballot) {
    acc->aid = serv->fileop.char_device_id;
    acc->iid = prepare->iid;
    acc->ballot = prepare->ballot;
    if (lmdb_storage_put(stor, acc) != 0) {
      lmdb_storage_tx_abort(stor);
      print_error("storage_tx_abort\n");
    }
  }

  if (lmdb_storage_tx_commit(stor) != 0)
    print_error("lmdb_storage_tx_commit\n");

  // accepted_to_kspace(serv, src, &acc, sizeof(paxos_accepted));
  return acc;
}

static void
handle_accept(struct server* serv, uint8_t* src, paxos_accept* req)
{
  paxos_message out;
  paxos_prepare prepare =
    (paxos_prepare){ .iid = req->promise_iid, .ballot = req->ballot };
  uint32_t promise_iid = 0;

  // handle accept
  paxos_accepted acc;
  memset(&acc, 0, sizeof(paxos_accepted));
  if (lmdb_storage_tx_begin(stor) != 0)
    print_error("lmdb_storage_tx_begin\n");

  int found_out1 = lmdb_storage_get(stor, req->iid, &acc);

  if (!found_out1 || acc.ballot <= req->ballot) {
    /* IF not found
       THEN transform to accepted and store
     */
    paxos_accept_to_accepted(serv->fileop.char_device_id, req, &out);

    printf("req.v.len: %lu\n", req->value.paxos_value_len);
    // needed here to store accepted
    if (lmdb_storage_put(stor, &out.u.accepted) != 0) {
      lmdb_storage_tx_abort(stor);
      print_error("storage_tx_abort\n");
    }
    if (lmdb_storage_tx_commit(stor) != 0)
      print_error("lmdb_storage_tx_commit\n");
  } else {
    paxos_accepted_to_preempted(serv->fileop.char_device_id, &acc, &out);
    printf("else\n");
  }
  paxos_accepted_destroy(&acc);
  // accept handling over

  // new handle prepare
  paxos_accepted* acc2 = handle_prepare(serv, &prepare);
  // new handle prepare over

  // promise_iid = out2.u.promise.iid;
  promise_iid = acc2->iid; // accepted to primise is done in kspace
  if (acc2->ballot > req->ballot || acc2->value_ballot > 0) {
    out.u.accepted.promise_iid = 0; // difference with stored accepted?
    // send_paxos_message(get_dev(a->peers), src, &out2); // send promise
    accepted_to_kspace(serv, src, acc2, sizeof(paxos_accepted));
    free(acc2);
    // will be transformed to promise in kspace and sent
    promise_iid = 0;
  }
  if (out.type == PAXOS_ACCEPTED) { // only if it wasn't in storage or
                                    // acc.ballot <= req.ballot
    out.u.accepted.promise_iid = promise_iid;
  }

  accept_to_kspace(serv, src, &out, sizeof(paxos_message));
  paxos_message_destroy(&out);
  // paxos_message_destroy(&out2);
}

static void
handle_repeat(struct server* serv, uint8_t* src, iid_t* iid)
{
  paxos_accepted acc;
  memset(&acc, 0, sizeof(paxos_accepted));
  if (lmdb_storage_tx_begin(stor) != 0)
    print_error("lmdb_storage_tx_begin\n");

  int found = lmdb_storage_get(stor, *iid, &acc);

  if (lmdb_storage_tx_commit(stor) != 0)
    print_error("lmdb_storage_tx_commit\n");

  if (found) {
    repeat_to_kspace(serv, src, &acc, sizeof(paxos_accepted));
  }
}

static void
unpack_message(struct server* serv, size_t len)
{
  struct user_msg*   mess = (struct user_msg*)serv->ethop.rec_buffer;
  struct kernel_msg* received_msg = (struct kernel_msg*)mess->value;
  paxos_accepted*    acc;
  switch (received_msg->msg_type) {
    case STORE_STATE:
      store_paxos_accepted((struct paxos_accepted*)received_msg->value);
      break;

    case PREPARE:
      printf("RECEIVED PREPARE\n");
      acc = handle_prepare(serv, (struct paxos_prepare*)received_msg->value);
      accepted_to_kspace(serv, received_msg->src, acc, sizeof(paxos_accepted));
      free(acc);
      break;
    case ACCEPT:
      printf("RECEIVED ACCEPT\n");
      handle_accept(serv, received_msg->src,
                    (struct paxos_accept*)received_msg->value);
      break;
    case REPEAT:
      handle_repeat(serv, received_msg->src, (iid_t*)received_msg->value);
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
  // printf("[user_acceptor] Acceptor write to LKM\n");

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
