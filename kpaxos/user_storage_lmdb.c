/*
 * Copyright (c) 2014, Xiaoguang Sun <sun dot xiaoguang at yoyosys dot com>
 * Copyright (c) 2014, University of Lugano
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the copyright holders nor the names of it
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "paxos.h"
#include <assert.h>
#include <errno.h>
#include <lmdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// TODO: generate config from config file
struct paxos_config paxos_config = { .trash_files = 1,
                                     .lmdb_sync = 0,
                                     .lmdb_env_path = "/tmp/acceptor",
                                     .lmdb_mapsize = 10 * 1024 * 1024 };

struct lmdb_storage
{
  MDB_env* env;
  MDB_txn* txn;
  MDB_dbi  dbi;
  int      acceptor_id;
};

char*
paxos_accepted_to_buffer(paxos_accepted* acc)
{
  size_t len = acc->value.paxos_value_len;
  char*  buffer = malloc(sizeof(paxos_accepted) + len);
  if (buffer == NULL)
    return NULL;
  memcpy(buffer, acc, sizeof(paxos_accepted));
  if (len > 0) {
    memcpy(&buffer[sizeof(paxos_accepted)], acc->value.paxos_value_val, len);
  }
  return buffer;
}

void
paxos_accepted_from_buffer(char* buffer, paxos_accepted* out)
{
  memcpy(out, buffer, sizeof(paxos_accepted));
  if (out->value.paxos_value_len > 0) {
    out->value.paxos_value_val = malloc(out->value.paxos_value_len);
    memcpy(out->value.paxos_value_val, &buffer[sizeof(paxos_accepted)],
           out->value.paxos_value_len);
  }
}

void lmdb_storage_close(struct lmdb_storage* s);

int
lmdb_compare_iid(const MDB_val* lhs, const MDB_val* rhs)
{
  iid_t lid, rid;
  assert(lhs->mv_size == sizeof(iid_t));
  assert(rhs->mv_size == sizeof(iid_t));
  lid = *((iid_t*)lhs->mv_data);
  rid = *((iid_t*)rhs->mv_data);
  return (lid == rid) ? 0 : (lid < rid) ? -1 : 1;
}

int
lmdb_storage_init(struct lmdb_storage* s, char* db_env_path)
{
  int      result;
  MDB_env* env = NULL;
  MDB_txn* txn = NULL;
  MDB_dbi  dbi = 0;

  if ((result = mdb_env_create(&env)) != 0) {
    printf("[storage_lmdb] Could not create lmdb environment. %s\n",
           mdb_strerror(result));
    goto error;
  }
  if ((result = mdb_env_set_mapsize(env, paxos_config.lmdb_mapsize)) != 0) {
    printf("[storage_lmdb] Could not set lmdb map size. %s\n",
           mdb_strerror(result));
    goto error;
  }
  if ((result = mdb_env_open(
         env, db_env_path, !paxos_config.lmdb_sync ? MDB_NOSYNC : 0,
         S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)) != 0) {
    printf("[storage_lmdb] Could not open lmdb environment at %s. %s\n",
           db_env_path, mdb_strerror(result));
    goto error;
  }
  if ((result = mdb_txn_begin(env, NULL, 0, &txn)) != 0) {
    printf("[storage_lmdb] Could not start txn on lmdb environment at %s. %s\n",
           db_env_path, mdb_strerror(result));
    goto error;
  }
  if ((result = mdb_open(txn, NULL, MDB_INTEGERKEY, &dbi)) != 0) {
    printf("[storage_lmdb] Could not open db on lmdb environment at %s. %s\n",
           db_env_path, mdb_strerror(result));
    goto error;
  }
  if ((result = mdb_set_compare(txn, dbi, lmdb_compare_iid)) != 0) {
    printf("[storage_lmdb] Could setup compare function on lmdb "
           "environment at %s. %s\n",
           db_env_path, mdb_strerror(result));
    goto error;
  }
  if ((result = mdb_txn_commit(txn)) != 0) {
    printf("[storage_lmdb] Could commit txn on lmdb environment at %s. %s\n",
           db_env_path, mdb_strerror(result));
    goto error;
  }

  s->env = env;
  s->dbi = dbi;

  return 0;
error:
  if (txn) {
    mdb_txn_abort(txn);
  }
  if (dbi) {
    mdb_close(env, dbi);
  }
  if (env) {
    mdb_env_close(env);
  }
  return -1;
}

struct lmdb_storage*
lmdb_storage_new(int acceptor_id)
{
  struct lmdb_storage* s = malloc(sizeof(struct lmdb_storage));
  memset(s, 0, sizeof(struct lmdb_storage));
  s->acceptor_id = acceptor_id;
  return s;
}

int
lmdb_storage_open(struct lmdb_storage* s)
{
  char*       lmdb_env_path = NULL;
  struct stat sb;
  int         dir_exists, result;
  size_t      lmdb_env_path_length = strlen(paxos_config.lmdb_env_path) + 16;

  lmdb_env_path = malloc(lmdb_env_path_length);
  snprintf(lmdb_env_path, lmdb_env_path_length, "%s_%d",
           paxos_config.lmdb_env_path, s->acceptor_id);

  // Trash files -- testing only
  if (paxos_config.trash_files) {
    char rm_command[600];
    sprintf(rm_command, "rm -r %s", lmdb_env_path);
    system(rm_command);
  }

  dir_exists = (stat(lmdb_env_path, &sb) == 0);

  if (!dir_exists && (mkdir(lmdb_env_path, S_IRWXU) != 0)) {
    printf("[storage_lmdb] Failed to create env dir %s: %s\n", lmdb_env_path,
           strerror(errno));
    result = -1;
    goto error;
  }

  if ((result = lmdb_storage_init(s, lmdb_env_path) != 0)) {
    printf("[storage_lmdb] Failed to open DB handle\n");
  } else {
    printf("[storage_lmdb] lmdb storage opened successfully\n");
    goto cleanup_exit;
  }

error:
  if (s) {
    lmdb_storage_close(s);
  }

cleanup_exit:
  free(lmdb_env_path);
  return result;
}

void
lmdb_storage_close(struct lmdb_storage* s)
{
  if (s->txn) {
    mdb_txn_abort(s->txn);
  }
  if (s->dbi) {
    mdb_close(s->env, s->dbi);
  }
  if (s->env) {
    mdb_env_close(s->env);
  }
  free(s);
  printf("[storage_lmdb] lmdb storage closed successfully\n");
}

int
lmdb_storage_tx_begin(struct lmdb_storage* s)
{
  assert(s->txn == NULL);
  return mdb_txn_begin(s->env, NULL, 0, &s->txn);
}

int
lmdb_storage_tx_commit(struct lmdb_storage* s)
{
  int result;
  assert(s->txn);
  result = mdb_txn_commit(s->txn);
  printf("lmdb_storage_tx_commit result %d\n", result);
  s->txn = NULL;
  return result;
}

void
lmdb_storage_tx_abort(struct lmdb_storage* s)
{
  if (s->txn) {
    mdb_txn_abort(s->txn);
    s->txn = NULL;
  }
}

int
lmdb_storage_get(struct lmdb_storage* s, iid_t iid, paxos_accepted* out)
{
  int     result;
  MDB_val key, data;

  memset(&data, 0, sizeof(data));

  key.mv_data = &iid;
  key.mv_size = sizeof(iid_t);

  if ((result = mdb_get(s->txn, s->dbi, &key, &data)) != 0) {
    if (result == MDB_NOTFOUND) {
      // printf("[storage_lmdb] There is no record for iid: %d\n", iid);
    } else {
      printf("[storage_lmdb] Could not find record for iid: %d : %s\n", iid,
             mdb_strerror(result));
    }
    return 0;
  }

  paxos_accepted_from_buffer(data.mv_data, out);
  assert(iid == out->iid);

  return 1;
}

int
lmdb_storage_put(struct lmdb_storage* s, paxos_accepted* acc)
{
  int     result;
  MDB_val key, data;
  char*   buffer = paxos_accepted_to_buffer(acc);

  key.mv_data = &acc->iid;
  key.mv_size = sizeof(iid_t);

  data.mv_data = buffer;
  data.mv_size = sizeof(paxos_accepted) + acc->value.paxos_value_len;

  result = mdb_put(s->txn, s->dbi, &key, &data, 0);
  free(buffer);
  return result;
}

iid_t
lmdb_storage_get_trim_instance(struct lmdb_storage* s)
{
  int     result;
  iid_t   iid = 0, k = 0;
  MDB_val key, data;

  key.mv_data = &k;
  key.mv_size = sizeof(iid_t);

  if ((result = mdb_get(s->txn, s->dbi, &key, &data)) != 0) {
    if (result != MDB_NOTFOUND) {
      printf("[storage_lmdb] mdb_get failed: %s\n", mdb_strerror(result));
      assert(result == 0);
    } else {
      iid = 0;
    }
  } else {
    iid = *(iid_t*)data.mv_data;
  }

  return iid;
}

int
lmdb_storage_put_trim_instance(struct lmdb_storage* s, iid_t iid)
{
  iid_t   k = 0;
  int     result;
  MDB_val key, data;

  key.mv_data = &k;
  key.mv_size = sizeof(iid_t);

  data.mv_data = &iid;
  data.mv_size = sizeof(iid_t);

  result = mdb_put(s->txn, s->dbi, &key, &data, 0);
  if (result != 0)
    printf("[storage_lmdb] \n%s\n", mdb_strerror(result));
  assert(result == 0);

  return 0;
}

int
lmdb_storage_trim(struct lmdb_storage* s, iid_t iid)
{
  int         result;
  iid_t       min = 0;
  MDB_cursor* cursor = NULL;
  MDB_val     key, data;

  if (iid == 0)
    return 0;

  lmdb_storage_put_trim_instance(s, iid);

  if ((result = mdb_cursor_open(s->txn, s->dbi, &cursor)) != 0) {
    printf("[storage_lmdb] Could not create cursor. %s\n",
           mdb_strerror(result));
    goto cleanup_exit;
  }

  key.mv_data = &min;
  key.mv_size = sizeof(iid_t);

  do {
    if ((result = mdb_cursor_get(cursor, &key, &data, MDB_NEXT)) == 0) {
      assert((key.mv_size = sizeof(iid_t)));
      min = *(iid_t*)key.mv_data;
    } else {
      goto cleanup_exit;
    }

    if (min != 0 && min <= iid) {
      if (mdb_cursor_del(cursor, 0) != 0) {
        printf("[storage_lmdb] mdb_cursor_del failed. %s\n",
               mdb_strerror(result));
        goto cleanup_exit;
      }
    }
  } while (min <= iid);

cleanup_exit:
  if (cursor) {
    mdb_cursor_close(cursor);
  }
  return 0;
}
