#include "chardevice_message.h"
#include "common.h"
// #include "kernel_device.h"
#include "storage.h"
#include <linux/if_ether.h>
#include <linux/vmalloc.h>

struct lmdb_storage
{
  //   MDB_env* env;
  //   MDB_txn* txn;
  //   MDB_dbi  dbi;
  int acceptor_id;
};

static void lmdb_storage_close(void* handle);

static struct lmdb_storage*
lmdb_storage_new(int acceptor_id)
{
  struct lmdb_storage* s = pmalloc(sizeof(struct lmdb_storage));
  memset(s, 0, sizeof(struct lmdb_storage));
  s->acceptor_id = acceptor_id;
  return s;
}

static int
lmdb_storage_open(void* handle)
{
  return 0;
}

static void
lmdb_storage_close(void* handle)
{}

static int
lmdb_storage_tx_begin(void* handle)
{
  return 0;
}

static int
lmdb_storage_tx_commit(void* handle)
{
  return 0;
}

static void
lmdb_storage_tx_abort(void* handle)
{}

static int
lmdb_storage_get(void* handle, iid_t iid, paxos_accepted* out)
{
  return 0;
}

static void
lmdb_storage_put(void* handle, paxos_accepted* acc)
{
  paxos_accepted_to_user_space(acc);
}

static iid_t
lmdb_storage_get_trim_instance(void* handle)
{
  // struct lmdb_storage* s = handle;
  // int                  result;
  iid_t iid = 0, k = 0;
  // MDB_val key, data; // Generic structure used for passing keys and data in
  // and
  //                    // out of the database

  // key.mv_data = &k;
  // key.mv_size = sizeof(iid_t);

  // if ((result = mdb_get(s->txn, s->dbi, &key, &data)) != 0) {
  //   if (result != MDB_NOTFOUND) {
  //     paxos_log_error("mdb_get failed: %s", mdb_strerror(result));
  //     assert(result == 0);
  //   } else {
  //     iid = 0;
  //   }
  // } else {
  //   iid = *(iid_t*)data.mv_data;
  // }

  return iid;
}

static int
lmdb_storage_put_trim_instance(void* handle, iid_t iid)
{
  //   struct lmdb_storage* s = handle;
  //   iid_t                k = 0;
  //   int                  result;
  //   MDB_val              key, data;

  //   key.mv_data = &k;
  //   key.mv_size = sizeof(iid_t);

  //   data.mv_data = &iid;
  //   data.mv_size = sizeof(iid_t);

  //   result = mdb_put(s->txn, s->dbi, &key, &data, 0);
  //   if (result != 0)
  //     paxos_log_error("%s\n", mdb_strerror(result));
  //   assert(result == 0);

  //   return 0;
  return 0;
}

static int
lmdb_storage_trim(void* handle, iid_t iid)
{
  //   struct lmdb_storage* s = handle;
  //   int                  result;
  //   iid_t                min = 0;
  //   MDB_cursor*          cursor = NULL;
  //   MDB_val              key, data;

  //   if (iid == 0)
  //     return 0;

  //   lmdb_storage_put_trim_instance(handle, iid);

  //   if ((result = mdb_cursor_open(s->txn, s->dbi, &cursor)) != 0) {
  //     paxos_log_error("Could not create cursor. %s", mdb_strerror(result));
  //     goto cleanup_exit;
  //   }

  //   key.mv_data = &min;
  //   key.mv_size = sizeof(iid_t);

  //   do {
  //     if ((result = mdb_cursor_get(cursor, &key, &data, MDB_NEXT)) == 0) {
  //       assert(key.mv_size = sizeof(iid_t));
  //       min = *(iid_t*)key.mv_data;
  //     } else {
  //       goto cleanup_exit;
  //     }

  //     if (min != 0 && min <= iid) {
  //       if (mdb_cursor_del(cursor, 0) != 0) {
  //         paxos_log_error("mdb_cursor_del failed. %s", mdb_strerror(result));
  //         goto cleanup_exit;
  //       }
  //     }
  //   } while (min <= iid);

  // cleanup_exit:
  //   if (cursor) {
  //     mdb_cursor_close(cursor);
  //   }
  //   return 0;
  return 0;
}

void
storage_init_lmdb(struct storage* s, int acceptor_id)
{
  s->handle = lmdb_storage_new(acceptor_id);
  //   s->api.open = lmdb_storage_open;
  //   s->api.close = lmdb_storage_close;
  //   s->api.tx_begin = lmdb_storage_tx_begin;
  //   s->api.tx_commit = lmdb_storage_tx_commit;
  //   s->api.tx_abort = lmdb_storage_tx_abort;
  //   s->api.get = lmdb_storage_get;
  //   s->api.put = lmdb_storage_put;
  //   s->api.trim = lmdb_storage_trim;
  s->api.get_trim_instance = lmdb_storage_get_trim_instance;
}
