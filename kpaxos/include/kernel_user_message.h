#include "kernel_device.h"
#include "paxos.h"

struct s_message;

void paxos_accepted_to_user_space(paxos_accepted* acc, int i);
