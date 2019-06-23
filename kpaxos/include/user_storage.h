#include <paxos_types.h>
#include <stdint.h>
#include <stdlib.h>

// /* Configuration */
// struct paxos_config
// {
//   int trash_files;

//   /* lmdb storage configuration */
//   int    lmdb_sync;
//   char*  lmdb_env_path;
//   size_t lmdb_mapsize;
// };

typedef uint32_t iid_t;
struct lmdb_storage;

// struct s_message
// {
//   size_t size;
//   int    msg_type;
//   char   value[0];
// };

// struct paxos_value
// {
//   int   paxos_value_len;
//   char* paxos_value_val;
// };
// typedef struct paxos_value paxos_value;

// struct paxos_accepted
// {
//   uint32_t    aid;
//   uint32_t    iid;
//   uint32_t    promise_iid;
//   uint32_t    ballot;
//   uint32_t    value_ballot;
//   paxos_value value;
// };
// typedef struct paxos_accepted paxos_accepted;

// char* paxos_accepted_to_buffer(paxos_accepted* acc);
// void  paxos_accepted_from_buffer(char* buffer, paxos_accepted* out);

struct lmdb_storage* lmdb_storage_new(int acceptor_id);
int                  lmdb_storage_open(struct lmdb_storage* store);
void                 lmdb_storage_close(struct lmdb_storage* store);
int                  lmdb_storage_tx_begin(struct lmdb_storage* store);
int                  lmdb_storage_tx_commit(struct lmdb_storage* store);
void                 lmdb_storage_tx_abort(struct lmdb_storage* store);
int                  lmdb_storage_get(struct lmdb_storage* store, iid_t iid,
                                      paxos_accepted* out);
int   lmdb_storage_put(struct lmdb_storage* store, paxos_accepted* acc);
int   lmdb_storage_trim(struct lmdb_storage* store, iid_t iid);
iid_t lmdb_storage_get_trim_instance(struct lmdb_storage* store);