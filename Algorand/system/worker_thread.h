/*
   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#ifndef _WORKERTHREAD_H_
#define _WORKERTHREAD_H_

#include "global.h"
#include "message.h"

#if ALGORAND == true
#define MUTEX_TYPE       pthread_mutex_t
#define MUTEX_SETUP(x)   pthread_mutex_init(&(x), NULL)
#define MUTEX_CLEANUP(x) pthread_mutex_destroy(&(x))
#define MUTEX_LOCK(x)    pthread_mutex_lock(&(x))
#define MUTEX_UNLOCK(x)  pthread_mutex_unlock(&(x))
#define THREAD_ID        pthread_self()
#endif


class Workload;
class Message;

class WorkerThread : public Thread {
public:
    RC run();
    void setup();
    void send_key();
    void process(Message * msg);
    void check_if_done(RC rc);
    void release_txn_man();
    void commit();
    void abort();
    TxnManager * get_transaction_manager(Message * msg);
    RC process_rinit(Message * msg);
    RC init_phase();
    uint64_t get_next_txn_id();
    bool is_cc_new_timestamp();
    RC process_key_exchange(Message *msg);

#if CLIENT_BATCH
    RC process_client_batch(Message * msg);
#else
    RC start_pbft(Message * msg);
#endif

#if RBFT_ON
    RC process_batch_propagate_msg(Message * msg);
#endif

#if BATCH_ENABLE == BUNSET
    RC process_pbft_pre_msg(Message * msg);
#else
    RC process_batch(Message * msg);
#endif

#if EXECUTION_THREAD
    RC process_execute_msg(Message * msg);
#endif

    uint64_t next_set;

#if VIEW_CHANGES == true
	RC process_pbft_view_change_msg(Message * msg);
	RC process_pbft_new_view_msg(Message * msg);
	void reset();
#endif

#if ZYZZYVA == true && LOCAL_FAULT
    RC process_client_certificate(Message *msg);
#endif

#if CONSENSUS == PBFT
    bool prepared();
    RC process_pbft_prep_msg(Message * msg);
    bool committed_local(PBFTCommitMessage * msg);
    RC process_pbft_commit_msg(Message * msg);
    RC process_pbft_chkpt_msg(Message * msg);

#if ALGORAND == true
    /* This array will store all of the mutexes available to OpenSSL. */ 
    //constexpr static MUTEX_TYPE *mutex_buf = (MUTEX_TYPE *) malloc(CRYPTO_num_locks() * sizeof(MUTEX_TYPE));
    static unsigned long id_function();
    static void locking_function(int mode, int n, const char *file, int line);
#endif


#endif

#if CONSENSUS == DBFT
    RC process_dbft_prep_msg(Message * msg);
    bool prepared(DBFTPrepMessage * msg);
    RC primary_send_prepare(Message *msg);
#endif

private:
    uint64_t _thd_txn_id;
    ts_t        _curr_ts;
    TxnManager * txn_man;


};

#endif
