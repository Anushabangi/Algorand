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

#include "global.h"
#include "mem_alloc.h"
#include "stats.h"
#include "sim_manager.h"
//#include "manager.h"
#include "query.h"
#include "client_query.h"
//#include "occ.h"
#include "transport.h"
#include "work_queue.h"
#include "abort_queue.h"
#include "msg_queue.h"
#include "pool.h"
#include "txn_table.h"
#include "client_txn.h"
//#include "sequencer.h"
//#include "logger.h"
//#include "maat.h"
#include "../config.h"

mem_alloc mem_allocator;
Stats stats;
SimManager * simulation;
//Manager glob_manager;
Query_queue query_queue;
Client_query_queue client_query_queue;
//OptCC occ_man;
//Maat maat_man;
Transport tport_man;
TxnManPool txn_man_pool;
TxnPool txn_pool;
//AccessPool access_pool;
TxnTablePool txn_table_pool;
MsgPool msg_pool;
//RowPool row_pool;
QryPool qry_pool;
TxnTable txn_table;
QWorkQueue work_queue;
AbortQueue abort_queue;
MessageQueue msg_queue;
Client_txn client_man;
//Sequencer seq_man;
//Logger logger;
//TimeTable time_table;

bool volatile warmup_done = false;
bool volatile enable_thread_mem_pool = false;
pthread_barrier_t warmup_bar;

ts_t g_abort_penalty = ABORT_PENALTY;
ts_t g_abort_penalty_max = ABORT_PENALTY_MAX;
bool g_central_man = CENTRAL_MAN;
UInt32 g_ts_alloc = TS_ALLOC;
bool g_key_order = KEY_ORDER;
bool g_ts_batch_alloc = TS_BATCH_ALLOC;
UInt32 g_ts_batch_num = TS_BATCH_NUM;
int32_t g_inflight_max = MAX_TXN_IN_FLIGHT;
//int32_t g_inflight_max = MAX_TXN_IN_FLIGHT/NODE_CNT;
uint64_t g_msg_size = MSG_SIZE_MAX;
int32_t g_load_per_server = LOAD_PER_SERVER;

bool g_hw_migrate = HW_MIGRATE;

volatile UInt64 g_row_id = 0;
bool g_part_alloc = PART_ALLOC;
bool g_mem_pad = MEM_PAD;
UInt32 g_cc_alg = CC_ALG;
ts_t g_query_intvl = QUERY_INTVL;
UInt32 g_part_per_txn = PART_PER_TXN;
double g_perc_multi_part = PERC_MULTI_PART;
double g_txn_read_perc = 1.0 - TXN_WRITE_PERC;
double g_txn_write_perc = TXN_WRITE_PERC;
double g_tup_read_perc = 1.0 - TUP_WRITE_PERC;
double g_tup_write_perc = TUP_WRITE_PERC;
double g_zipf_theta = ZIPF_THETA;
double g_data_perc = DATA_PERC;
double g_access_perc = ACCESS_PERC;
bool g_prt_lat_distr = PRT_LAT_DISTR;
UInt32 g_node_id = 0;
UInt32 g_node_cnt = NODE_CNT;
UInt32 g_part_cnt = PART_CNT;
UInt32 g_virtual_part_cnt = VIRTUAL_PART_CNT;
UInt32 g_core_cnt = CORE_CNT;

#if CC_ALG == HSTORE || CC_ALG == HSTORE_SPEC
UInt32 g_thread_cnt = PART_CNT/NODE_CNT;
#else
UInt32 g_thread_cnt = THREAD_CNT;

   #if EXECUTION_THREAD 
   UInt32 g_execute_thd = EXECUTE_THD_CNT;
   #else
   UInt32 g_execute_thd = 0;
   #endif
   
   #if SIGN_THREADS
   UInt32 g_sign_thd = SIGN_THD_CNT;
   #else
   UInt32 g_sign_thd = 0;
   #endif

#endif
UInt32 g_rem_thread_cnt = REM_THREAD_CNT;
UInt32 g_abort_thread_cnt = 1;
UInt32 g_send_thread_cnt = SEND_THREAD_CNT;
UInt32 g_total_thread_cnt = g_thread_cnt + g_rem_thread_cnt + g_send_thread_cnt + g_abort_thread_cnt;
UInt32 g_total_client_thread_cnt = g_client_thread_cnt + g_client_rem_thread_cnt + g_client_send_thread_cnt;
UInt32 g_total_node_cnt = g_node_cnt + g_client_node_cnt + g_repl_cnt*g_node_cnt;
UInt64 g_synth_table_size = SYNTH_TABLE_SIZE;
UInt32 g_req_per_query = REQ_PER_QUERY;
bool g_strict_ppt = STRICT_PPT == 1;
UInt32 g_field_per_tuple = FIELD_PER_TUPLE;
UInt32 g_init_parallelism = INIT_PARALLELISM;
UInt32 g_client_node_cnt = CLIENT_NODE_CNT;
UInt32 g_client_thread_cnt = CLIENT_THREAD_CNT;
UInt32 g_client_rem_thread_cnt = CLIENT_REM_THREAD_CNT;
UInt32 g_client_send_thread_cnt = CLIENT_SEND_THREAD_CNT;
UInt32 g_servers_per_client = 0;
UInt32 g_clients_per_server = 0;
UInt32 g_server_start_node = 0;

UInt32 g_this_thread_cnt = ISCLIENT ? g_client_thread_cnt : g_thread_cnt;
UInt32 g_this_rem_thread_cnt = ISCLIENT ? g_client_rem_thread_cnt : g_rem_thread_cnt;
UInt32 g_this_send_thread_cnt = ISCLIENT ? g_client_send_thread_cnt : g_send_thread_cnt;
UInt32 g_this_total_thread_cnt = ISCLIENT ? g_total_client_thread_cnt : g_total_thread_cnt;

UInt32 g_max_txn_per_part = MAX_TXN_PER_PART;
UInt32 g_network_delay = NETWORK_DELAY;
UInt64 g_done_timer = DONE_TIMER;
UInt64 g_batch_time_limit = BATCH_TIMER;
UInt64 g_seq_batch_time_limit = SEQ_BATCH_TIMER;
UInt64 g_prog_timer = PROG_TIMER;
UInt64 g_warmup_timer = WARMUP_TIMER;
UInt64 g_msg_time_limit = MSG_TIME_LIMIT;

double g_mpr = MPR;
double g_mpitem = MPIR;

UInt32 g_max_items_per_txn = MAX_ITEMS_PER_TXN;
UInt32 g_dist_per_wh = DIST_PER_WH;

UInt32 g_repl_type = REPL_TYPE;
UInt32 g_repl_cnt = REPLICA_CNT;

#if CONSENSUS == PBFT || CONSENSUS == DBFT
string g_priv_key;		//stores this node's private key
string g_public_key;
string g_pub_keys[NODE_CNT + CLIENT_NODE_CNT];	//stores public keys
std::mutex keyMTX;
bool keyAvail = false;
uint64_t totKey = 0;

#if ALGORAND == true
//resources for algorand

Algorand g_algorand;
dataPackage m_dp; //data to send
dataPackage g_dp; //data received

#endif

uint64_t indexSize = 2 * g_client_node_cnt * g_inflight_max;
int g_min_invalid_nodes = (g_node_cnt - 1) / 3; //min number of valid nodes
uint64_t g_next_index = 0;	//the index of the next process to be executed
uint32_t g_last_stable_chkpt = 0;  //index of the last stable checkpoint
uint32_t g_txn_per_chkpt = TXN_PER_CHKPT;
uint64_t lastDeletedTxnMan = 0;
uint g_batch_threads = BATCH_THREADS;
uint g_btorder_thd = 1;
uint64_t expectedBatchCount = g_batch_size - 3;

std::mutex batchMTX;
uint commonVar = 0;	// variable which all thread increment on initialize
uint64_t nextSetId = 0;

double rsatime = 0;
double minrsa = 0;
double maxrsa = 0;
double largersa = 0;
uint64_t rsacount = 0;
bool flagrsa = false;

// STORAGE OF CLIENT DATA
uint64_t ClientDataStore[SYNTH_TABLE_SIZE] = {0};

#if RBFT_ON
uint32_t g_master_instance = 0;
uint32_t g_instance_offset = 0;
uint32_t g_instance_id = 0;
#endif

uint32_t local_view[THREAD_CNT + REM_THREAD_CNT] = {0};

#if VIEW_CHANGES == true

std::mutex inputMTX; // for updating view for input thread.
bool inputView = false;

std::mutex btsendMTX; // for updating view for batch ordering thread.
bool btsendView = false;

std::mutex ctbtchMTX[BATCH_THREADS]; // updating view for batch creating thread.
bool ctbtchView[BATCH_THREADS] = {false};

#if CONSENSUS == DBFT
std::mutex prepMTX; // for updating view for prepare thread.
bool prepView;
#endif

bool g_changing_view;
uint64_t reCount = 0;

#endif

#if BATCH_ENABLE == BSET
  uint64_t g_batch_size = BATCH_SIZE;
  uint64_t batchSet[2 * CLIENT_NODE_CNT * MAX_TXN_IN_FLIGHT];
#endif

#endif

#if CONSENSUS == DBFT
  #if BATCH_ENABLE == BSET
	string stateRep;
	string lastStateRep;
	std::hash<std::string> hashGen;
  #endif
	int req_no_prep_msgs = 2 * g_min_invalid_nodes;
#if RBFT_ON
    string mystate[2 * CLIENT_NODE_CNT * MAX_TXN_IN_FLIGHT][((NODE_CNT - 1) / 3) + 1];
#else
	string mystate[2 * CLIENT_NODE_CNT * MAX_TXN_IN_FLIGHT];
#endif
	string mytriple[2 * CLIENT_NODE_CNT * MAX_TXN_IN_FLIGHT];
	uint64_t expectedPrepCount = g_batch_size - 4;
	uint g_prep_thd = 1;

#endif

#if ZYZZYVA == true
  string istate = "0";
  std::hash<std::string> hashGen;
#endif

uint32_t g_view = 0; //current leader node.

#if LOCAL_FAULT || VIEW_CHANGES
vector<uint64_t> stop_nodes; // List of nodes that have stopped.
#endif



