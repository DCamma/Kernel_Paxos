#ifndef user_space
#include "kernel_device.h"
#endif
#include "paxos.h"

enum msg_types
{
  GET_STATE,
  STORE_STATE,
  PREPARE,
  ACCEPT,
  REPEAT,
};

extern void paxos_accepted_to_userspace(paxos_accepted* acc);
extern void paxos_prepare_to_userspace(paxos_prepare* prep, eth_address* addr);
extern void paxos_accept_to_userspace(paxos_accept* prep, eth_address* addr);
extern void paxos_repeat_to_userspace(iid_t iid, eth_address* addr);
