#include "chardevice_message.h"
#ifndef user_space
#include <eth.h>
#include <linux/vmalloc.h>
#endif

void
paxos_accepted_to_userspace(paxos_accepted* acc)
{
  size_t len = acc->value.paxos_value_len;
  char*  buffer = pmalloc(sizeof(int) + sizeof(paxos_accepted) + len);
  if (buffer == NULL)
    paxos_log_error("[paxos_acc_to_user_s] pmalloc returned NULL");
  int msg_type = STORE_STATE;
  memcpy(buffer, &msg_type, sizeof(int));
  memcpy(&buffer[sizeof(int)], acc, sizeof(paxos_accepted));
  if (len > 0) {
    memcpy(&buffer[sizeof(int) + sizeof(paxos_accepted)],
           acc->value.paxos_value_val, len);
  }

  kset_message(buffer, sizeof(int) + sizeof(paxos_accepted) + len);
  pfree(buffer);
}

void
paxos_prepare_to_userspace(paxos_prepare* req, eth_address* src)
{
  size_t total_size = sizeof(int) + 6 * sizeof(uint8_t) + sizeof(paxos_prepare);
  char*  buffer = pmalloc(total_size);
  if (buffer == NULL)
    paxos_log_error("[prepare to user] pmalloc returned NULL");
  int    msg_type = PREPARE;
  size_t padd = sizeof(int);
  memcpy(buffer, &msg_type, padd);
  memcpy(buffer + padd, src, 6 * sizeof(uint8_t));
  padd += 6 * sizeof(uint8_t);
  memcpy(buffer + padd, req, sizeof(paxos_prepare));

  kset_message(buffer, total_size);
  pfree(buffer);
}

void
paxos_accept_to_userspace(paxos_accept* req, eth_address* src)
{
  size_t total_size = sizeof(int) + 6 * sizeof(uint8_t) + sizeof(paxos_accept);
  char*  buffer = pmalloc(total_size);
  if (buffer == NULL)
    paxos_log_error("[prepare to user] pmalloc returned NULL");
  int    msg_type = ACCEPT;
  size_t padd = sizeof(int);
  memcpy(buffer, &msg_type, padd);
  memcpy(buffer + padd, src, 6 * sizeof(uint8_t));
  padd += 6 * sizeof(uint8_t);
  memcpy(buffer + padd, req, sizeof(paxos_accept));

  kset_message(buffer, total_size);
  pfree(buffer);
}

void
paxos_repeat_to_userspace(iid_t iid, eth_address* src)
{
  size_t total_size = sizeof(int) + 6 * sizeof(uint8_t) + sizeof(iid_t);
  char*  buffer = pmalloc(total_size);
  if (buffer == NULL)
    paxos_log_error("[prepare to user] pmalloc returned NULL");
  int    msg_type = REPEAT;
  size_t padd = sizeof(int);
  memcpy(buffer, &msg_type, padd);
  memcpy(buffer + padd, src, 6 * sizeof(uint8_t));
  padd += 6 * sizeof(uint8_t);
  memcpy(buffer + padd, &iid, sizeof(iid_t));

  kset_message(buffer, total_size);
  pfree(buffer);
}

// void
// request_trim_to_user_space(iid_t i)
// {
//   paxos_log_debug("[request_trim_to_user_space] iid:    [%zu]", i);

//   size_t len = acc->value.paxos_value_len;
//   char*  buffer = pmalloc(sizeof(int) + sizeof(paxos_accepted) + len);
//   if (buffer == NULL)
//     paxos_log_error(
//       "[paxos_acc_to_user_s] pmalloc returned NULL in
//       paxos_accepted_to_user");
//   int msg_type = STORE_STATE;
//   memcpy(buffer, &msg_type, sizeof(int));
//   memcpy(&buffer[sizeof(int)], acc, sizeof(paxos_accepted));
//   if (len > 0) {
//     memcpy(&buffer[sizeof(int) + sizeof(paxos_accepted)],
//            acc->value.paxos_value_val, len);
//   }

//   kset_message(buffer, sizeof(int) + sizeof(paxos_accepted) + len);
//   pfree(buffer);
// }

// void
// get_state_user_space()
// {
//   size_t len = acc->value.paxos_value_len;
//   char*  buffer = pmalloc(sizeof(paxos_accepted) + len);
//   if (buffer == NULL)
//     return NULL;
//   memcpy(buffer, acc, sizeof(paxos_accepted));
//   if (len > 0) {
//     memcpy(&buffer[sizeof(paxos_accepted)], acc->value.paxos_value_val, len);
//   }
//   return buffer;
// }