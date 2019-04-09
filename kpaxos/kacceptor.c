#include "evpaxos.h"
#include "kernel_device.h"
#include <asm/atomic.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/time.h>
#include <linux/udp.h>
#include <net/sock.h>

const char* MOD_NAME = "KAcceptor";

static char* if_name = "enp4s0";
module_param(if_name, charp, 0000);
MODULE_PARM_DESC(if_name, "The interface name, default enp4s0");

static int id = 0;
module_param(id, int, S_IRUGO);
MODULE_PARM_DESC(id, "The acceptor id, default 0");

static char* path = "./paxos.conf";
module_param(path, charp, S_IRUGO);
MODULE_PARM_DESC(path, "The config file position, default ./paxos.conf");

static struct evacceptor* acc = NULL;

static void
on_deliver(unsigned int iid, char* value, size_t size, void* arg)
{
  kset_message(value, size); // from klearner, change this
}

static void
start_acceptor(int id)
{
  kdevchar_init(id, "kacceptor");
  acc = evacceptor_init(on_deliver, id, if_name, path);
  if (acc == NULL) {
    LOG_ERROR("Could not start the acceptor\n");
  }
}

static int __init
           init_acceptor(void)
{
  start_acceptor(id);
  LOG_INFO("Module loaded");
  return 0;
}

static void __exit
            acceptor_exit(void)
{
  kdevchar_exit();
  if (acc != NULL)
    evacceptor_free(acc);
  LOG_INFO("Module unloaded");
}

module_init(init_acceptor);
module_exit(acceptor_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Emanuele Giuseppe Esposito");
