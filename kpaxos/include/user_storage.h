#include <paxos_types.h>
#include <stdint.h>
#include <stdlib.h>

typedef uint32_t iid_t;
struct lmdb_storage;

extern char* paxos_accepted_to_buffer(paxos_accepted* acc);
extern void  paxos_accepted_from_buffer(char* buffer, paxos_accepted* out);

extern struct lmdb_storage* lmdb_storage_new(int acceptor_id);
extern int                  lmdb_storage_open(struct lmdb_storage* store);
extern void                 lmdb_storage_close(struct lmdb_storage* store);
extern int                  lmdb_storage_tx_begin(struct lmdb_storage* store);
extern int                  lmdb_storage_tx_commit(struct lmdb_storage* store);
extern void                 lmdb_storage_tx_abort(struct lmdb_storage* store);
extern int   lmdb_storage_get(struct lmdb_storage* store, iid_t iid,
                              paxos_accepted* out);
extern int   lmdb_storage_put(struct lmdb_storage* store, paxos_accepted* acc);
extern int   lmdb_storage_trim(struct lmdb_storage* store, iid_t iid);
extern iid_t lmdb_storage_get_trim_instance(struct lmdb_storage* store);