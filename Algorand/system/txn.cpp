/*
   Copyright 2016 Massachusetts Institute of Technology

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

#include "helper.h"
#include "txn.h"
#include "wl.h"
#include "query.h"
#include "thread.h"
#include "mem_alloc.h"
#include "msg_queue.h"
#include "pool.h"
#include "message.h"
#include "ycsb_query.h"
#include "array.h"


void TxnStats::init() {
    starttime = 0;
    wait_starttime = get_sys_clock();
    total_process_time = 0;
    process_time = 0;
    total_local_wait_time = 0;
    local_wait_time = 0;
    total_remote_wait_time = 0;
    remote_wait_time = 0;
    total_twopc_time = 0;
    twopc_time = 0;
    write_cnt = 0;
    abort_cnt = 0;

    total_work_queue_time = 0;
    work_queue_time = 0;
    total_cc_block_time = 0;
    cc_block_time = 0;
    total_cc_time = 0;
    cc_time = 0;
    total_work_queue_cnt = 0;
    work_queue_cnt = 0;
    total_msg_queue_time = 0;
    msg_queue_time = 0;
    total_abort_time = 0;

    //PBFT Stats
#if CONSENSUS == PBFT || CONSENSUS == DBFT
    time_start_pre_prepare = 0;
    time_start_prepare = 0;
    time_start_commit = 0;
    time_start_execute = 0;
#endif

    clear_short();
}

void TxnStats::clear_short() {

    work_queue_time_short = 0;
    cc_block_time_short = 0;
    cc_time_short = 0;
    msg_queue_time_short = 0;
    process_time_short = 0;
    network_time_short = 0;
}

void TxnStats::reset() {
    wait_starttime = get_sys_clock();
    total_process_time += process_time;
    process_time = 0;
    total_local_wait_time += local_wait_time;
    local_wait_time = 0;
    total_remote_wait_time += remote_wait_time;
    remote_wait_time = 0;
    total_twopc_time += twopc_time;
    twopc_time = 0;
    write_cnt = 0;

    total_work_queue_time += work_queue_time;
    work_queue_time = 0;
    total_cc_block_time += cc_block_time;
    cc_block_time = 0;
    total_cc_time += cc_time;
    cc_time = 0;
    total_work_queue_cnt += work_queue_cnt;
    work_queue_cnt = 0;
    total_msg_queue_time += msg_queue_time;
    msg_queue_time = 0;

    clear_short();

}

void TxnStats::abort_stats(uint64_t thd_id) {
    total_process_time += process_time;
    total_local_wait_time += local_wait_time;
    total_remote_wait_time += remote_wait_time;
    total_twopc_time += twopc_time;
    total_work_queue_time += work_queue_time;
    total_msg_queue_time += msg_queue_time;
    total_cc_block_time += cc_block_time;
    total_cc_time += cc_time;
    total_work_queue_cnt += work_queue_cnt;
    assert(total_process_time >= process_time);

    INC_STATS(thd_id, lat_s_rem_work_queue_time, total_work_queue_time);
    INC_STATS(thd_id, lat_s_rem_msg_queue_time, total_msg_queue_time);
    INC_STATS(thd_id, lat_s_rem_cc_block_time, total_cc_block_time);
    INC_STATS(thd_id, lat_s_rem_cc_time, total_cc_time);
    INC_STATS(thd_id, lat_s_rem_process_time, total_process_time);
}

void TxnStats::commit_stats(uint64_t thd_id, uint64_t txn_id, uint64_t batch_id, uint64_t timespan_long,
                            uint64_t timespan_short) {
    total_process_time += process_time;
    total_local_wait_time += local_wait_time;
    total_remote_wait_time += remote_wait_time;
    total_twopc_time += twopc_time;
    total_work_queue_time += work_queue_time;
    total_msg_queue_time += msg_queue_time;
    total_cc_block_time += cc_block_time;
    total_cc_time += cc_time;
    total_work_queue_cnt += work_queue_cnt;
    assert(total_process_time >= process_time);

    // latency from start of transaction
    if (IS_LOCAL(txn_id)) {
      INC_STATS(thd_id,lat_l_loc_work_queue_time,total_work_queue_time);
      INC_STATS(thd_id,lat_l_loc_msg_queue_time,total_msg_queue_time);
      INC_STATS(thd_id,lat_l_loc_cc_block_time,total_cc_block_time);
      INC_STATS(thd_id,lat_l_loc_cc_time,total_cc_time);
      INC_STATS(thd_id,lat_l_loc_process_time,total_process_time);
      INC_STATS(thd_id,lat_l_loc_abort_time,total_abort_time);

      INC_STATS(thd_id,lat_s_loc_work_queue_time,work_queue_time);
      INC_STATS(thd_id,lat_s_loc_msg_queue_time,msg_queue_time);
      INC_STATS(thd_id,lat_s_loc_cc_block_time,cc_block_time);
      INC_STATS(thd_id,lat_s_loc_cc_time,cc_time);
      INC_STATS(thd_id,lat_s_loc_process_time,process_time);

      INC_STATS(thd_id,lat_short_work_queue_time,work_queue_time_short);
      INC_STATS(thd_id,lat_short_msg_queue_time,msg_queue_time_short);
      INC_STATS(thd_id,lat_short_cc_block_time,cc_block_time_short);
      INC_STATS(thd_id,lat_short_cc_time,cc_time_short);
      INC_STATS(thd_id,lat_short_process_time,process_time_short);
      INC_STATS(thd_id,lat_short_network_time,network_time_short);


    }
    else {
      INC_STATS(thd_id,lat_l_rem_work_queue_time,total_work_queue_time);
      INC_STATS(thd_id,lat_l_rem_msg_queue_time,total_msg_queue_time);
      INC_STATS(thd_id,lat_l_rem_cc_block_time,total_cc_block_time);
      INC_STATS(thd_id,lat_l_rem_cc_time,total_cc_time);
      INC_STATS(thd_id,lat_l_rem_process_time,total_process_time);
    }
    if (IS_LOCAL(txn_id)) {
      PRINT_LATENCY("lat_s %ld %ld %f %f %f %f %f %f\n"
            , txn_id
            , work_queue_cnt
            , (double) timespan_short / BILLION
            , (double) work_queue_time / BILLION
            , (double) msg_queue_time / BILLION
            , (double) cc_block_time / BILLION
            , (double) cc_time / BILLION
            , (double) process_time / BILLION
            );
     /*
    PRINT_LATENCY("lat_l %ld %ld %ld %f %f %f %f %f %f %f\n"
            , txn_id
            , total_work_queue_cnt
            , abort_cnt
            , (double) timespan_long / BILLION
            , (double) total_work_queue_time / BILLION
            , (double) total_msg_queue_time / BILLION
            , (double) total_cc_block_time / BILLION
            , (double) total_cc_time / BILLION
            , (double) total_process_time / BILLION
            , (double) total_abort_time / BILLION
            );
            */
    }
    else {
      PRINT_LATENCY("lat_rs %ld %ld %f %f %f %f %f %f\n"
            , txn_id
            , work_queue_cnt
            , (double) timespan_short / BILLION
            , (double) total_work_queue_time / BILLION
            , (double) total_msg_queue_time / BILLION
            , (double) total_cc_block_time / BILLION
            , (double) total_cc_time / BILLION
            , (double) total_process_time / BILLION
            );
    }
    /*
    if (!IS_LOCAL(txn_id) || timespan_short < timespan_long) {
      // latency from most recent start or restart of transaction
      PRINT_LATENCY("lat_s %ld %ld %f %f %f %f %f %f\n"
            , txn_id
            , work_queue_cnt
            , (double) timespan_short / BILLION
            , (double) work_queue_time / BILLION
            , (double) msg_queue_time / BILLION
            , (double) cc_block_time / BILLION
            , (double) cc_time / BILLION
            , (double) process_time / BILLION
            );
    }
    */

    if (!IS_LOCAL(txn_id)) {
        return;
    }

    INC_STATS(thd_id, txn_total_process_time, total_process_time);
    INC_STATS(thd_id, txn_process_time, process_time);
    INC_STATS(thd_id, txn_total_local_wait_time, total_local_wait_time);
    INC_STATS(thd_id, txn_local_wait_time, local_wait_time);
    INC_STATS(thd_id, txn_total_remote_wait_time, total_remote_wait_time);
    INC_STATS(thd_id, txn_remote_wait_time, remote_wait_time);
    INC_STATS(thd_id, txn_total_twopc_time, total_twopc_time);
    INC_STATS(thd_id, txn_twopc_time, twopc_time);
    if (write_cnt > 0) {
        INC_STATS(thd_id, txn_write_cnt, 1);
    }
    if (abort_cnt > 0) {
        INC_STATS(thd_id, unique_txn_abort_cnt, 1);
    }

}


void Transaction::init() {
    timestamp = UINT64_MAX;
    start_timestamp = UINT64_MAX;
    end_timestamp = UINT64_MAX;
    txn_id = UINT64_MAX;
    batch_id = UINT64_MAX;

    reset(0);
}

void Transaction::reset(uint64_t thd_id) {
    write_cnt = 0;
    row_cnt = 0;
    twopc_state = START;
    rc = RCOK;
}

void Transaction::release(uint64_t thd_id) {
    DEBUG("Transaction release\n");
}

void TxnManager::init(uint64_t thd_id, Workload *h_wl) {
    uint64_t prof_starttime = get_sys_clock();
    if (!txn) {
        DEBUG_M("Transaction alloc\n");
        txn_pool.get(thd_id, txn);

    }
    INC_STATS(get_thd_id(), mtx[15], get_sys_clock() - prof_starttime);
    prof_starttime = get_sys_clock();
    
    if (!query) {
        DEBUG_M("TxnManager::init Query alloc\n");
        qry_pool.get(thd_id, query);
    }
    INC_STATS(get_thd_id(), mtx[16], get_sys_clock() - prof_starttime);
    
    sem_init(&rsp_mutex, 0, 1);
    return_id = UINT64_MAX;

    this->h_wl = h_wl;

    txn_ready = true;
    twopl_wait_start = 0;

#if CONSENSUS == PBFT
    prep_rsp_cnt = 2 * g_min_invalid_nodes;
    commit_rsp_cnt = prep_rsp_cnt + 1;
#endif

#if RBFT_ON
    allocated = false;
    prop_ready = false;
#endif

#if CONSENSUS == DBFT
    dbft_beg = false;
#endif

    txn_stats.init();
}

// reset after abort
void TxnManager::reset() {
    rsp_cnt = 0;
    aborted = false;
    return_id = UINT64_MAX;
    twopl_wait_start = 0;

    assert(txn);
    assert(query);
    txn->reset(get_thd_id());

    // Stats
    txn_stats.reset();

}

void TxnManager::release() {
    uint64_t prof_starttime = get_sys_clock();
    qry_pool.put(get_thd_id(), query);
    INC_STATS(get_thd_id(), mtx[0], get_sys_clock() - prof_starttime);
    query = NULL;
    prof_starttime = get_sys_clock();
    txn_pool.put(get_thd_id(), txn);
    INC_STATS(get_thd_id(), mtx[1], get_sys_clock() - prof_starttime);
    txn = NULL;

    txn_ready = true;
}

void TxnManager::reset_query() {
    ((YCSBQuery *) query)->reset();
}

RC TxnManager::commit() {
    DEBUG("Commit %ld\n", get_txn_id());

   // printf("Commit %ld\n", get_txn_id());
   // fflush(stdout);

   commit_stats();
   return Commit;
}


RC TxnManager::start_commit() {
    RC rc = RCOK;
    DEBUG("%ld start_commit RO?\n", get_txn_id());

#if CONSENSUS == PBFT
    #if BATCH_ENABLE == BUNSET
	send_pbft_pre_msgs();
    #else
	send_batch_msgs();
    #endif
#elif CONSENSUS == DBFT
    #if BATCH_ENABLE == BUNSET
	send_dbft_pre_msgs();
    #else
	send_dbft_batch_msgs();
    #endif
#endif

    return rc;
}


int TxnManager::received_response(RC rc) {
    assert(txn->rc == RCOK || txn->rc == Abort);
    if (txn->rc == RCOK)
        txn->rc = rc;

    --rsp_cnt;

    return rsp_cnt;
}

bool TxnManager::waiting_for_response() {
    return rsp_cnt > 0;
}


void TxnManager::commit_stats() {
    uint64_t commit_time = get_sys_clock();
    uint64_t timespan_short = commit_time - txn_stats.restart_starttime;
    uint64_t timespan_long = commit_time - txn_stats.starttime;
    INC_STATS(get_thd_id(), total_txn_commit_cnt, 1);

    if (!IS_LOCAL(get_txn_id()) && CC_ALG != CALVIN) {
        INC_STATS(get_thd_id(), remote_txn_commit_cnt, 1);
        txn_stats.commit_stats(get_thd_id(), get_txn_id(), get_batch_id(), timespan_long, timespan_short);
        return;
    }


    INC_STATS(get_thd_id(), txn_cnt, 1);
    INC_STATS(get_thd_id(), local_txn_commit_cnt, 1);
    INC_STATS(get_thd_id(), txn_run_time, timespan_long);
  
    INC_STATS(get_thd_id(), single_part_txn_cnt, 1);
    INC_STATS(get_thd_id(), single_part_txn_run_time, timespan_long);
  
    txn_stats.commit_stats(get_thd_id(), get_txn_id(), get_batch_id(), timespan_long, timespan_short);


    INC_STATS_ARR(get_thd_id(), start_abort_commit_latency, timespan_short);
    INC_STATS_ARR(get_thd_id(), last_start_commit_latency, timespan_short);
    INC_STATS_ARR(get_thd_id(), first_start_commit_latency, timespan_long);

}

void TxnManager::register_thread(Thread *h_thd) {
    this->h_thd = h_thd;
}

void TxnManager::set_txn_id(txnid_t txn_id) {
    txn->txn_id = txn_id;
}

txnid_t TxnManager::get_txn_id() {
    return txn->txn_id;
}

Workload *TxnManager::get_wl() {
    return h_wl;
}

uint64_t TxnManager::get_thd_id() {
    if (h_thd)
        return h_thd->get_thd_id();
    else
        return 0;
}

BaseQuery *TxnManager::get_query() {
    return query;
}

void TxnManager::set_query(BaseQuery *qry) {
    query = qry;
}

void TxnManager::set_timestamp(ts_t timestamp) {
    txn->timestamp = timestamp;
}

ts_t TxnManager::get_timestamp() {
    return txn->timestamp;
}

void TxnManager::set_start_timestamp(uint64_t start_timestamp) {
    txn->start_timestamp = start_timestamp;
}

ts_t TxnManager::get_start_timestamp() {
    return txn->start_timestamp;
}


uint64_t TxnManager::incr_rsp(int i) {
    //ATOM_ADD(this->rsp_cnt,i);
    uint64_t result;
    sem_wait(&rsp_mutex);
    result = ++this->rsp_cnt;
    sem_post(&rsp_mutex);
    return result;
}

uint64_t TxnManager::decr_rsp(int i) {
    //ATOM_SUB(this->rsp_cnt,i);
    uint64_t result;
    sem_wait(&rsp_mutex);
    result = --this->rsp_cnt;
    sem_post(&rsp_mutex);
    return result;
}


RC TxnManager::validate() {
    return RCOK;
}



/* METHODS FOR DBFT AND PBFT */

#if CONSENSUS == PBFT || CONSENSUS == DBFT
#if BATCH_ENABLE == BSET
// Broadcasts a batch of requests to all nodes
void TxnManager::send_batch_msgs() {
   // printf("%ld Send PBFT_PRE_MSG messages %d\n nodes", get_txn_id(), g_node_cnt - 1);
   // fflush(stdout);
	
    Message * msg = Message::create_message(this, BATCH_REQ);
    BatchRequests *breq = (BatchRequests *)msg;

    char * buf = (char*)malloc(breq->get_size() + 1);
    breq->copy_to_buf(buf);

    for (uint64_t i = 0; i < g_node_cnt; i++) {
        if (i == g_node_id) {
            continue;
        }
		
	Message * deepCopyMsg = Message::create_message(buf);
	msg_queue.enqueue(get_thd_id(), deepCopyMsg, i);
    }
}

#endif
#endif


#if CONSENSUS == PBFT
#if BATCH_ENABLE == BUNSET
//broadcasts pre-prepare message to all nodes
void TxnManager::send_pbft_pre_msgs() {
    DEBUG("%ld Send PBFT_PRE_MSG messages to %d\n nodes", get_txn_id(), g_node_cnt - 1);

   // printf("%ld Send PBFT_PRE_MSG messages to %d\n nodes", get_txn_id(), g_node_cnt - 1);
   // fflush(stdout);
	
    Message * msg = Message::create_message(this, PBFT_PRE_MSG);
    PBFTPreMessage *premsg = (PBFTPreMessage *)msg;

    uint64_t relIndex = premsg->index % indexSize;
    preStore[relIndex] = *premsg;

   // // For failures.
   // uint currentIndex = premsg->index;
   // uint sentCnt = 0;

    char * buf = (char*)malloc(premsg->get_size() + 1);
    premsg->copy_to_buf(buf);

    for (uint64_t i = 0; i < g_node_cnt; i++) {
        if (i == g_node_id) {//avoid sending msg to yourself
            continue;
        }
		
	   Message * deepCopyMsg = Message::create_message(buf);
	   msg_queue.enqueue(get_thd_id(), deepCopyMsg, i);
    }
}

#endif


//broadcasts prepare message to all nodes
void TxnManager::send_pbft_prep_msgs() {
    DEBUG("%ld Send PBFT_PREP_MSG messages to %d\n nodes", get_txn_id(), g_node_cnt - 1);

//  printf("%ld Send PBFT_PREP_MSG messages to %d nodes\n", get_txn_id(), g_node_cnt - 1);
//  fflush(stdout);
	
    Message * msg = Message::create_message(this, PBFT_PREP_MSG);
    PBFTPrepMessage * pmsg = (PBFTPrepMessage *)msg;
#if RBFT_ON
    pmsg->instance_id = this->instance_id;
#endif

#if BATCH_ENABLE == BUNSET
    prepPStore[pmsg->index % indexSize][g_node_id] = *pmsg;
#else
    prepPStore[pmsg->end_index % indexSize][g_node_id] = *pmsg;
#endif    

#if LOCAL_FAULT == true || VIEW_CHANGES == true
	this->prep_rsp_cnt--;
#endif

#if ALGORAND == true
    dataPackage temp;
    temp = g_algorand.sortition(20,0,g_algorand.m_wallet,g_algorand.sum_w,m_dp.value);
    m_dp.seed = g_algorand.seed;
    m_dp.proof = temp.proof;
    m_dp.hash = temp.hash;
    m_dp.j = temp.j;
    m_dp.node_id = g_node_id;
    g_algorand.countVotes(0.68,20,&(m_dp));
#endif

    char * buf = (char*)malloc(((PBFTPrepMessage*)msg)->get_size() + 1);
    ((PBFTPrepMessage*)msg)->copy_to_buf(buf);
    for (uint64_t i = 0; i < g_node_cnt; i++) {
        if (i == g_node_id) { 
            continue;
        }

	   Message * deepCopyMsg = Message::create_message(buf);

        #if ALGORAND == true
            deepCopyMsg->dp = m_dp;
            deepCopyMsg->dp.type = ALGORAND_SORT;
        #endif

	   msg_queue.enqueue(get_thd_id(), deepCopyMsg, i);
    } 

}


//broadcasts commit message to all nodes
void TxnManager::send_pbft_commit_msgs() 
{
    DEBUG("%ld Send PBFT_COMMIT_MSG messages to %d\n nodes",get_txn_id(), g_node_cnt-1);
    cout << "Send PBFT_COMMIT_MSG messages " << get_txn_id() << "\n";
    fflush(stdout);
    
    Message * msg = Message::create_message(this, PBFT_COMMIT_MSG);
    PBFTCommitMessage *cmsg = (PBFTCommitMessage *)msg;

#if BATCH_ENABLE == BUNSET
    commStore[cmsg->index % indexSize][g_node_id] = *cmsg;
#else
    commStore[cmsg->end_index % indexSize][g_node_id] = *cmsg;
#endif
    
#if LOCAL_FAULT == true || VIEW_CHANGES == true
	if(this->commit_rsp_cnt > 0) {
		this->commit_rsp_cnt--;
	}
#endif

    char * buf = (char*)malloc(((PBFTCommitMessage*)msg)->get_size() + 1);
    ((PBFTCommitMessage*)msg)->copy_to_buf(buf);
    for (uint64_t i = 0; i < g_node_cnt; i++) {
        if (i == g_node_id) {
            continue;
        }

	   Message * deepCopyMsg = Message::create_message(buf);

        #if ALGORAND == true
            deepCopyMsg->dp = m_dp;
            //cout << "Send ALGORAND COMMIT messages" << i << "\n";
        #endif

	   msg_queue.enqueue(get_thd_id(), deepCopyMsg, i);
    }
}


//broadcasts checkpoint message to all nodes
void TxnManager::send_checkpoint_msgs() 
{
    DEBUG("%ld Send PBFT_CHKPT_MSG messages to %d\n nodes", get_txn_id(), g_node_cnt - 1);
    
    Message * msg = Message::create_message(this, PBFT_CHKPT_MSG);
    CheckpointMessage *ckmsg = (CheckpointMessage*)msg;
    //g_pbft_chkpt_msgs.push_back(*ckmsg);
	
    char * buf = (char*)malloc(ckmsg->get_size() + 1);
    ckmsg->copy_to_buf(buf);
    for (uint64_t i = 0; i < g_node_cnt; i++) {
	if (i == g_node_id) {
            continue;
        }
		
	Message * deepCopyMsg = Message::create_message(buf);
	msg_queue.enqueue(get_thd_id(), deepCopyMsg, i);
    }
	
}

#endif


#if CONSENSUS == DBFT
#if BATCH_ENABLE == BSET
// Broadcasts a batch of requests to all nodes
void TxnManager::send_dbft_batch_msgs() {
   // printf("%ld Send DBFT_BATCH messages to %d\n nodes", get_txn_id(), g_node_cnt - 1);
   // fflush(stdout);

    // Creating a batch and sending requests.
    send_batch_msgs();

    this->istate = lastStateRep;
    this->fstate = stateRep;
 
    // Reset for next batch
    lastStateRep = stateRep;
    stateRep = "";
}

//broadcasts prepare message to all nodes
void TxnManager::send_dbft_execute_msgs() {
    uint64_t tid = get_txn_id();
#if RBFT_ON
    if((int)g_instance_id == this->instance_id) {
#else
    if(g_node_id == g_view) {
#endif
   	tid += 3;
    }

   // printf("Send DBFT_PREP_MSG messages--  Txn:%ld\n",tid);
   // fflush(stdout);
#if RBFT_ON
    if((int)g_instance_id != this->instance_id && this->instance_id == (int)g_master_instance) {
#else
    if(g_node_id != g_view) {	// For primary this is done while making batches.
#endif
#if RBFT_ON
        string mtriple = mystate[tid % indexSize][this->instance_id];
#else
    	string mtriple = mystate[tid % indexSize];
#endif
    	string hashedTriple = std::to_string(hashGen(mtriple));
        //cout << "mstate: " << mystate[tid % indexSize][this->instance_id] << endl;
        //cout << "storing mytriple[" << tid % indexSize << "]\n";
    	mytriple[tid % indexSize] = hashedTriple;
    }

    this->tripleSign = mytriple[tid % indexSize];
		
    Message *msg = Message::create_message(this, DBFT_PREP_MSG);
    DBFTPrepMessage *dmsg = (DBFTPrepMessage *)msg;
    //cout << "dbft prep msg to be sent: index: " << dmsg->index << ", instance: " << dmsg->instance_id << endl;
    // Adding message for self.
    prepStore[dmsg->index % indexSize][dmsg->return_node] = *dmsg;

    char * buf = (char*)malloc(dmsg->get_size() + 1);
    dmsg->copy_to_buf(buf);

    for (uint64_t i = 0; i < g_node_cnt; i++) {
        if (i == g_node_id) 
        	continue;

        Message * deepCopyMsg = Message::create_message(buf);
        msg_queue.enqueue(get_thd_id(), deepCopyMsg, i);
    }

//    // Reset for next batch
//    if(g_node_id != g_view) {
//   	 lastStateRep = stateRep;
//   	 stateRep = "";
//    }
}

#endif
#endif

