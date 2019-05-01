#include "kernel_user_message.h"
#include "storage_utils.h"
#include <linux/vmalloc.h>

/*
    Message from Kernel space to User space and vice versa
*/
struct s_message
{
  size_t size;
  int    msg_type;
  char   value[0];
};

int msg_id = 0;

void
paxos_accepted_to_user_space(paxos_accepted* acc, int i)
{
  paxos_log_debug(">>> set_message");
  // paxos_log_debug("> [%u] [%u]", sizeof(struct
  // s_message),sizeof(paxos_accepted));

  //   struct s_message* m = vmalloc(sizeof(struct s_message));
  //   m->msg_type = i;
  //   paxos_log_debug("> m->msg_type: %d", m->msg_type);
  //   paxos_log_debug("> sizeof(m): %u", sizeof(m));

  paxos_log_debug("> sizeof(paxos_accepted): [%u]", sizeof(paxos_accepted));
  paxos_log_debug("> sizeof(value):          [%u]", acc->value.paxos_value_len);
  paxos_log_debug("> acc.ballot: [%d] <<<", acc->ballot);

  //   memcpy(m->value, acc, sizeof(paxos_accepted));
  kset_message(paxos_accepted_to_buffer(acc), sizeof(paxos_accepted));

  //   vfree(m);

  msg_id++;
  paxos_log_debug("> msg_id: [%d]", msg_id);
  paxos_log_debug("set_message <<<");
  // paxos_log_debug("> acc:    [%u, %u, %u]", acc->aid,
  // acc->iid,acc->promise_iid);
  //   char c[] = "test test test"; kset_message(c,sizeof(c));
}