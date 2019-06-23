#include "chardevice_message.h"
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
  paxos_log_debug("[paxos_acc_to_user_s] acc.iid:    [%zu]", acc->iid);
  paxos_log_debug("[paxos_acc_to_user_s] acc.ballot: [%zu]", acc->ballot);

  size_t len = acc->value.paxos_value_len;
  char*  buffer = pmalloc(sizeof(int) + sizeof(paxos_accepted) + len);
  if (buffer == NULL)
    paxos_log_error(
      "[paxos_acc_to_user_s] pmalloc returned NULL in paxos_accepted_to_user");
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