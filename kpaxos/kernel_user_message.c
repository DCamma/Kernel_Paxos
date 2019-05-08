#include "kernel_user_message.h"
#include "storage_utils.h"
#include <linux/vmalloc.h>

enum msg_types
{
  GET_STATE,
  STORE_STATE,
};

void
paxos_accepted_to_user_space(paxos_accepted* acc)
{
  paxos_log_debug(">>> set_message");
  paxos_log_debug("> sizeof(paxos_accepted): [%u]", sizeof(paxos_accepted));
  paxos_log_debug("> sizeof(value):          [%u]", acc->value.paxos_value_len);
  paxos_log_debug("> acc.ballot: [%d] <<<", acc->ballot);

  size_t len = acc->value.paxos_value_len;
  char*  buffer = pmalloc(sizeof(int) + sizeof(paxos_accepted) + len);
  if (buffer == NULL)
    paxos_log_error("pmalloc returned NULL in paxos_accepted_to_user");
  int msg_type = STORE_STATE;
  memcpy(buffer, &msg_type, sizeof(int));
  memcpy(&buffer[sizeof(int)], acc, sizeof(paxos_accepted));
  if (len > 0) {
    memcpy(&buffer[sizeof(int) + sizeof(paxos_accepted)],
           acc->value.paxos_value_val, len);
  }

  kset_message(buffer, sizeof(int) + sizeof(paxos_accepted) + len);

  pfree(buffer);
  paxos_log_debug("set_message <<<");
  // paxos_log_debug("> acc:    [%u, %u, %u]", acc->aid,
  // acc->iid,acc->promise_iid);
  //   char c[] = "test test test"; kset_message(c,sizeof(c));
}

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