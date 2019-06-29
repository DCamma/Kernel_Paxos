#ifndef user_space
#include "kernel_device.h"
#endif
#include "paxos.h"

enum msg_types
{
  GET_STATE,
  STORE_STATE,
  PREPARE,
};

void paxos_accepted_to_user_space(paxos_accepted* acc);
void prepare_to_userspace(paxos_prepare* prep);
