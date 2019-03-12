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

#ifndef _GLOBAL_H_
#define _GLOBAL_H_

#define __STDC_LIMIT_MACROS
#include <stdint.h>
#include <unistd.h>
#include <cstddef>
#include <cstdlib>
#include <cassert>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string.h>
#include <typeinfo>
#include <list>
#include <map>
#include <set>
#include <queue>
#include <string>
#include <vector>
#include <sstream>
#include <time.h> 
#include <sys/time.h>
#include <math.h>

#include "pthread.h"
#include "config.h"
#include "stats.h"
//#include "work_queue.h"
#include "pool.h"
#include "txn_table.h"
//#include "logger.h"
#include "sim_manager.h"
//#include "maat.h"
#include <mutex>

#if ALGORAND == true

#include "Algorand.h"
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#endif

#include<unordered_map>

using namespace std;

class mem_alloc;
class Stats;
class SimManager;
//class Manager;
class Query_queue;
//class OptCC;
//class Maat;
class Transport;
class Remote_query;
class TxnManPool;
class TxnPool;
//class AccessPool;
class TxnTablePool;
class MsgPool;
//class RowPool;
class QryPool;
class TxnTable;
class QWorkQueue;
class AbortQueue;
class MessageQueue;
class Client_query_queue;
class Client_txn;

#if ALGORAND == true
class Algorand;
struct dataPackage;
#endif

//class Sequencer;
//class Logger;
//class TimeTable;

typedef uint32_t UInt32;
typedef int32_t SInt32;
typedef uint64_t UInt64;
typedef int64_t SInt64;

typedef uint64_t ts_t; // time stamp type

/******************************************/
// Global Data Structure 
/******************************************/
extern mem_alloc mem_allocator;
extern Stats stats;
extern SimManager * simulation;
//extern Manager glob_manager;
extern Query_queue query_queue;
extern Client_query_queue client_query_queue;
//extern OptCC occ_man;
//extern Maat maat_man;
extern Transport tport_man;
extern TxnManPool txn_man_pool;
extern TxnPool txn_pool;
//extern AccessPool access_pool;
extern TxnTablePool txn_table_pool;
extern MsgPool msg_pool;
//extern RowPool row_pool;
extern QryPool qry_pool;
extern TxnTable txn_table;
extern QWorkQueue work_queue;
extern AbortQueue abort_queue;
extern MessageQueue msg_queue;
extern Client_txn client_man;
//extern Sequencer seq_man;
//extern Logger logger;
//extern TimeTable time_table;

extern bool volatile warmup_done;
extern bool volatile enable_thread_mem_pool;
extern pthread_barrier_t warmup_bar;

/******************************************/
// Client Global Params 
/******************************************/
extern UInt32 g_client_thread_cnt;
extern UInt32 g_client_rem_thread_cnt;
extern UInt32 g_client_send_thread_cnt;
extern UInt32 g_client_node_cnt;
extern UInt32 g_servers_per_client;
extern UInt32 g_clients_per_server;
extern UInt32 g_server_start_node;

/******************************************/
// Global Parameter
/******************************************/
extern volatile UInt64 g_row_id;
extern bool g_part_alloc;
extern bool g_mem_pad;
extern bool g_prt_lat_distr;
extern UInt32 g_node_id;
extern UInt32 g_node_cnt;
extern UInt32 g_part_cnt;
extern UInt32 g_virtual_part_cnt;
extern UInt32 g_core_cnt;
extern UInt32 g_total_node_cnt;
extern UInt32 g_total_thread_cnt;
extern UInt32 g_total_client_thread_cnt;
extern UInt32 g_this_thread_cnt;
extern UInt32 g_this_rem_thread_cnt;
extern UInt32 g_this_send_thread_cnt;
extern UInt32 g_this_total_thread_cnt;
extern UInt32 g_thread_cnt;
extern UInt32 g_execute_thd;
extern UInt32 g_sign_thd;
extern UInt32 g_abort_thread_cnt;
extern UInt32 g_send_thread_cnt;
extern UInt32 g_rem_thread_cnt;
extern ts_t g_abort_penalty; 
extern ts_t g_abort_penalty_max; 
extern bool g_central_man;
extern UInt32 g_ts_alloc;
extern bool g_key_order;
extern bool g_ts_batch_alloc;
extern UInt32 g_ts_batch_num;
extern int32_t g_inflight_max;
extern uint64_t g_msg_size;

extern UInt32 g_max_txn_per_part;
extern int32_t g_load_per_server;

extern bool g_hw_migrate;
extern UInt32 g_network_delay;
extern UInt64 g_done_timer;
extern UInt64 g_batch_time_limit;
extern UInt64 g_seq_batch_time_limit;
extern UInt64 g_prog_timer;
extern UInt64 g_warmup_timer;
extern UInt64 g_msg_time_limit;

// YCSB
extern UInt32 g_cc_alg;
extern ts_t g_query_intvl;
extern UInt32 g_part_per_txn;
extern double g_perc_multi_part;
extern double g_txn_read_perc;
extern double g_txn_write_perc;
extern double g_tup_read_perc;
extern double g_tup_write_perc;
extern double g_zipf_theta;
extern double g_data_perc;
extern double g_access_perc;
extern UInt64 g_synth_table_size;
extern UInt32 g_req_per_query;
extern bool g_strict_ppt;
extern UInt32 g_field_per_tuple;
extern UInt32 g_init_parallelism;
extern double g_mpr;
extern double g_mpitem;

// Replication
extern UInt32 g_repl_type;
extern UInt32 g_repl_cnt;

enum RC { RCOK=0, Commit, Abort, WAIT, WAIT_REM, ERROR, FINISH, NONE };
enum RemReqType {INIT_DONE=0,
    KEYEX,
    READY,
    RLK,
    RULK,
    CL_QRY, //TQ: Message type for Client Query or transaction
    RQRY, //TQ: Remote Query
    RQRY_CONT, //7
    RFIN,
    RLK_RSP,
    RULK_RSP,
    RQRY_RSP,
    RACK,//12
    RACK_PREP,
    RACK_FIN,
    RTXN,
    RTXN_CONT,
    RINIT,//17
    RPREPARE,
    RPASS,
    RFWD,
    RDONE,
    CL_RSP, //TQ: Client response //22
#if CLIENT_BATCH
    CL_BATCH,
#endif
    NO_MSG,  //24 

  #if ZYZZYVA == true && LOCAL_FAULT
    CL_CC,
    CL_CAck,
  #endif


#if CONSENSUS == PBFT || CONSENSUS == DBFT
    EXECUTE_MSG,
    PBFT_PRE_MSG,
  
  #if BATCH_ENABLE == BSET
    BATCH_REQ,	// 25
  #endif

  #if RBFT_ON
    PROP_BATCH,
  #endif

  #if VIEW_CHANGES == true
	PBFT_VIEW_CHANGE,
	PBFT_NEW_VIEW,
  #endif
#endif

#if CONSENSUS == PBFT
    PBFT_PREP_MSG, //29 ?
    PBFT_COMMIT_MSG,
    PBFT_CHKPT_MSG
#endif

#if CONSENSUS == DBFT
    DBFT_PREP_MSG, //32
    PP_MSG
#endif

};

// Calvin
enum CALVIN_PHASE {CALVIN_RW_ANALYSIS=0,CALVIN_LOC_RD,CALVIN_SERVE_RD,CALVIN_COLLECT_RD,CALVIN_EXEC_WR,CALVIN_DONE};

/* Thread */
typedef uint64_t txnid_t;

/* Txn */
typedef uint64_t txn_t;

/* Table and Row */
typedef uint64_t rid_t; // row id
typedef uint64_t pgid_t; // page id



/* INDEX */
enum latch_t {LATCH_EX, LATCH_SH, LATCH_NONE};
// accessing type determines the latch type on nodes
enum idx_acc_t {INDEX_INSERT, INDEX_READ, INDEX_NONE};
typedef uint64_t idx_key_t; // key id for index
typedef uint64_t (*func_ptr)(idx_key_t);	// part_id func_ptr(index_key);

/* general concurrency control */
enum access_t {RD, WR, XP, SCAN};
/* LOCK */
enum lock_t {LOCK_EX = 0, LOCK_SH, LOCK_NONE };
/* TIMESTAMP */
enum TsType {R_REQ = 0, W_REQ, P_REQ, XP_REQ}; 

#define GET_THREAD_ID(id)	(id % g_thread_cnt)
#define GET_NODE_ID(id)	(id % g_node_cnt)
#define GET_PART_ID(t,n)	(n) 
#define GET_PART_ID_FROM_IDX(idx)	(g_node_id + idx * g_node_cnt) 
#define GET_PART_ID_IDX(p)	(p / g_node_cnt) 
#define ISSERVER (g_node_id < g_node_cnt)
#define ISSERVERN(id) (id < g_node_cnt)
#define ISCLIENT (g_node_id >= g_node_cnt && g_node_id < g_node_cnt + g_client_node_cnt)
#define ISREPLICA (g_node_id >= g_node_cnt + g_client_node_cnt && g_node_id < g_node_cnt + g_client_node_cnt + g_repl_cnt * g_node_cnt)
#define ISREPLICAN(id) (id >= g_node_cnt + g_client_node_cnt && id < g_node_cnt + g_client_node_cnt + g_repl_cnt * g_node_cnt)
#define ISCLIENTN(id) (id >= g_node_cnt && id < g_node_cnt + g_client_node_cnt)

//@Suyash
#if DBTYPE != REPLICATED
#define IS_LOCAL(tid) (tid % g_node_cnt == g_node_id || CC_ALG == CALVIN)
#else
#define IS_LOCAL(tid) true
#endif

#define IS_REMOTE(tid) (tid % g_node_cnt != g_node_id || CC_ALG == CALVIN)
#define IS_LOCAL_KEY(key) (key % g_node_cnt == g_node_id)

/*
#define GET_THREAD_ID(id)	(id % g_thread_cnt)
#define GET_NODE_ID(id)	(id / g_thread_cnt)
#define GET_PART_ID(t,n)	(n*g_thread_cnt + t) 
*/

#define MSG(str, args...) { \
	printf("[%s : %d] " str, __FILE__, __LINE__, args); } \
//	printf(args); }

// principal index structure. The workload may decide to use a different 
// index structure for specific purposes. (e.g. non-primary key access should use hash)
#if (INDEX_STRUCT == IDX_BTREE)
#define INDEX		index_btree
#else  // IDX_HASH
#define INDEX		IndexHash
#endif

/************************************************/
// constants
/************************************************/
#ifndef UINT64_MAX
#define UINT64_MAX 		18446744073709551615UL
#endif // UINT64_MAX

#if CONSENSUS == PBFT || CONSENSUS == DBFT
extern string g_priv_key;//stores this node's private key
extern string g_public_key;
extern string g_pub_keys[NODE_CNT + CLIENT_NODE_CNT];	//stores public keys
extern std::mutex keyMTX;
extern bool keyAvail;
extern uint64_t totKey;

#if ALGORAND == true
//resources for algorand

extern Algorand g_algorand; //global Algorand object
extern dataPackage m_dp; //data to send
extern dataPackage g_dp; //data received

#endif


extern uint64_t indexSize;
extern int g_min_invalid_nodes;
extern uint64_t g_next_index; // stores index of the next transaction to be executed
extern uint32_t g_last_stable_chkpt;  //index of the last stable checkpoint
extern uint32_t g_txn_per_chkpt;
extern uint64_t lastDeletedTxnMan;
extern uint g_batch_threads;
extern uint g_btorder_thd;
extern uint64_t expectedBatchCount;

extern std::mutex batchMTX;
extern uint commonVar;	// variable which all thread increment on initialize
extern uint64_t nextSetId;

extern double rsatime;
extern double minrsa;
extern double maxrsa;
extern double largersa;
extern uint64_t rsacount;
extern bool flagrsa;

// STORAGE OF CLIENT DATA
extern uint64_t ClientDataStore[SYNTH_TABLE_SIZE];


#if RBFT_ON
extern uint32_t g_master_instance;
extern uint32_t g_instance_offset;
extern uint32_t g_instance_id;
#endif

extern uint32_t local_view[THREAD_CNT + REM_THREAD_CNT];

#if VIEW_CHANGES == true

extern std::mutex inputMTX; // for updating view for input thread.
extern bool inputView;

extern std::mutex btsendMTX; // for updating view for batch ordering thread.
extern bool btsendView;

extern std::mutex ctbtchMTX[BATCH_THREADS]; // updating view for batch creating thread.
extern bool ctbtchView[BATCH_THREADS];

#if CONSENSUS == DBFT
extern std::mutex prepMTX; // for updating view for prepare thread.
extern bool prepView;
#endif

extern bool g_changing_view;
extern uint64_t reCount;

#endif

#if BATCH_ENABLE == BSET
  extern uint64_t g_batch_size;
  extern uint64_t batchSet[2 * CLIENT_NODE_CNT * MAX_TXN_IN_FLIGHT];
#endif
#endif

#if CONSENSUS == DBFT
  #if BATCH_ENABLE == BSET
	extern string stateRep;
	extern string lastStateRep;
	extern std::hash<std::string> hashGen;
  #endif
	extern int req_no_prep_msgs;
#if RBFT_ON
    extern string mystate[2 * CLIENT_NODE_CNT * MAX_TXN_IN_FLIGHT][((NODE_CNT - 1) / 3) + 1];
#else
	extern string mystate[2 * CLIENT_NODE_CNT * MAX_TXN_IN_FLIGHT];
#endif
	extern string mytriple[2 * CLIENT_NODE_CNT * MAX_TXN_IN_FLIGHT];
	extern uint64_t expectedPrepCount;
	extern uint g_prep_thd;
#endif

extern uint32_t g_view; // id of current primary

#if ZYZZYVA == true
  extern string istate;
  extern std::hash<std::string> hashGen;
#endif

#if LOCAL_FAULT || VIEW_CHANGES
extern vector<uint64_t> stop_nodes; // List of nodes that have stopped.
#endif

#endif
