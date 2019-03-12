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
#include "message.h"
#include "thread.h"
#include "worker_thread.h"
#include "txn.h"
#include "wl.h"
#include "query.h"
#include "ycsb_query.h"
#include "math.h"
#include "helper.h"
#include "msg_thread.h"
#include "msg_queue.h"
#include "work_queue.h"
//#include "logger.h"
#include "message.h"
#include "abort_queue.h"
#include "picosha2.h"

#if ALGORAND == true
#include "Algorand.h"
#endif

#if ALGORAND == true
//resources for algorand

#include <openssl/err.h>

static MUTEX_TYPE *mutex_buf = (MUTEX_TYPE *) malloc(CRYPTO_num_locks() * sizeof(MUTEX_TYPE));

void handle_error(const char *file, int lineno, const char *msg)
{
  fprintf(stderr, "** %s:%d %s\n", file, lineno, msg);
  ERR_print_errors_fp(stderr);
  /* exit(-1); */ 
}

unsigned long WorkerThread::id_function(void)
{
  return ((unsigned long)THREAD_ID);
}

//static void cleanup(void)
//{
//  EVP_cleanup();
//  CRYPTO_cleanup_all_ex_data();
//}

void WorkerThread::locking_function(int mode, int n, const char *file, int line)
{
  if(mode & CRYPTO_LOCK)
    MUTEX_LOCK(mutex_buf[n]);
  else
    MUTEX_UNLOCK(mutex_buf[n]);
}

#endif

void WorkerThread::send_key() {
	// Send everyone the public key.
	for(uint64_t i = 0; i < g_node_cnt + g_client_node_cnt; i++) {
		if(i == g_node_id) {
			continue;
		}

		Message *msg = Message::create_message(KEYEX);
    KeyExchange *keyex = (KeyExchange *)msg;

#if ALGORAND == true
    printf("Send keys\n");
    fflush(stdout);
    keyex->dp = m_dp;
    keyex->dp.type = ALGORAND_GETKEY;
#endif
    keyex->pkey = g_public_key;
    keyex->pkeySz = keyex->pkey.size();
    keyex->return_node = g_node_id;

		msg_queue.enqueue(get_thd_id(), keyex, i);
	}
}


void WorkerThread::setup() {

  // Increment commonVar.
  batchMTX.lock();
  commonVar++;
  batchMTX.unlock();

  if( get_thd_id() == 0) {
    while(commonVar < g_thread_cnt + g_rem_thread_cnt + g_send_thread_cnt);

    send_init_done_to_all_nodes();

    send_key();
  }
  _thd_txn_id = 0;

#if ALGORAND == true
  if(!mutex_buf)
    return;
  for(int i = 0;  i < CRYPTO_num_locks();  i++)
    MUTEX_SETUP(mutex_buf[i]);
  CRYPTO_set_id_callback(id_function);
  CRYPTO_set_locking_callback(locking_function);
#endif

}

void WorkerThread::process(Message * msg) {
  RC rc __attribute__ ((unused));

  DEBUG("%ld Processing %ld %d\n",get_thd_id(),msg->get_txn_id(),msg->get_rtype());
  //assert(msg->get_rtype() == CL_QRY || msg->get_txn_id() != UINT64_MAX);
  uint64_t starttime = get_sys_clock();
		switch(msg->get_rtype()) {
			case KEYEX:
	rc = process_key_exchange(msg);
				break;

#if CLIENT_BATCH
			case CL_BATCH:
        rc = process_client_batch(msg);
#else
      case CL_QRY:
	      rc = start_pbft(msg);
#endif
				break;

#if BATCH_ENABLE == BUNSET
			case PBFT_PRE_MSG:
	rc = process_pbft_pre_msg(msg);
				break;
#else
      #if RBFT_ON
            		case PROP_BATCH:
    	rc = process_batch_propagate_msg(msg);
        			break;
      #endif
			case BATCH_REQ:
	rc = process_batch(msg);
				break;
#endif

    #if EXECUTION_THREAD
			case EXECUTE_MSG:
	rc = process_execute_msg(msg);
				break;
    #endif

    #if VIEW_CHANGES == true
			case PBFT_VIEW_CHANGE:
	rc = process_pbft_view_change_msg(msg);
				break;
			case PBFT_NEW_VIEW:
	rc = process_pbft_new_view_msg(msg);
				break;
    #endif


#if CONSENSUS == PBFT
			case PBFT_CHKPT_MSG:
	rc = process_pbft_chkpt_msg(msg);
				break;
			case PBFT_PREP_MSG:
	rc = process_pbft_prep_msg(msg);
				break;
			case PBFT_COMMIT_MSG:
	rc = process_pbft_commit_msg(msg);
				break;
#endif

#if ZYZZYVA && LOCAL_FAULT
			case CL_CC:
	rc = process_client_certificate(msg);
				break;
#endif

#if CONSENSUS == DBFT
			case DBFT_PREP_MSG:
	rc = process_dbft_prep_msg(msg);
				break;
			case PP_MSG:
	rc = primary_send_prepare(msg);
				break;
#endif
			default:
        printf("Msg: %d\n",msg->get_rtype());
        fflush(stdout);
			assert(false);
			break;
		}
  uint64_t timespan = get_sys_clock() - starttime;
  INC_STATS(get_thd_id(),worker_process_cnt,1);
  INC_STATS(get_thd_id(),worker_process_time,timespan);
  INC_STATS(get_thd_id(),worker_process_cnt_by_type[msg->rtype],1);
  INC_STATS(get_thd_id(),worker_process_time_by_type[msg->rtype],timespan);
  DEBUG("%ld EndProcessing %d %ld\n",get_thd_id(),msg->get_rtype(),msg->get_txn_id());
}

RC WorkerThread::process_key_exchange(Message *msg) {
	KeyExchange *keyex = (KeyExchange *)msg;
  uint64_t totnodes = g_node_cnt + g_client_node_cnt;

#if ALGORAND == true

  printf("Receive keys\n");
  fflush(stdout);

  if(keyex->return_node != 4) {
     g_algorand.getkey(keyex->dp.pk,keyex->dp.node_id); 
  }
  totKey++;

  if(totKey+1 == totnodes) { 
      for(uint64_t i=g_node_cnt; i<totnodes; i++) {
    Message *rdy = Message::create_message(READY);
    msg_queue.enqueue(get_thd_id(), rdy, i);
      } }

  return RCOK;
#else

	g_pub_keys[keyex->return_node] = keyex->pkey;
	totKey++;

	if(totKey+1 == totnodes) { 
	    for(uint64_t i=g_node_cnt; i<totnodes; i++) {
		Message *rdy = Message::create_message(READY);
		msg_queue.enqueue(get_thd_id(), rdy, i);
	    } }

	return RCOK;
#endif
}


void WorkerThread::check_if_done(RC rc) {
  if(txn_man->waiting_for_response())
    return;
  if(rc == Commit)
    commit();
  if(rc == Abort)
    abort();
}

void WorkerThread::release_txn_man() {
  txn_table.release_transaction_manager(get_thd_id(),txn_man->get_txn_id(),txn_man->get_batch_id());
  txn_man = NULL;
}


// Can't use txn_man after this function
void WorkerThread::commit() {
  //TxnManager * txn_man = txn_table.get_transaction_manager(txn_id,0);
  //txn_man->release_locks(RCOK);
  //        txn_man->commit_stats();
  assert(txn_man);
  assert(IS_LOCAL(txn_man->get_txn_id()));

  uint64_t timespan = get_sys_clock() - txn_man->txn_stats.starttime;
  DEBUG("COMMIT %ld %f -- %f\n",txn_man->get_txn_id(),simulation->seconds_from_start(get_sys_clock()),(double)timespan/ BILLION);

  // Send result back to client
#if !SERVER_GENERATE_QUERIES
if(g_node_id == g_view) {
  msg_queue.enqueue(get_thd_id(),Message::create_message(txn_man,CL_RSP),txn_man->client_id);
}
#endif
  // remove txn from pool
  release_txn_man();
  // Do not use txn_man after this

}

void WorkerThread::abort() {

  DEBUG("ABORT %ld -- %f\n",txn_man->get_txn_id(),(double)get_sys_clock() - run_starttime/ BILLION);
  // TODO: TPCC Rollback here

  ++txn_man->abort_cnt;
  txn_man->reset();

  uint64_t penalty = abort_queue.enqueue(get_thd_id(), txn_man->get_txn_id(),txn_man->get_abort_cnt());

  txn_man->txn_stats.total_abort_time += penalty;

}

TxnManager * WorkerThread::get_transaction_manager(Message * msg) {
#if RBFT_ON
   TxnManager * local_txn_man = txn_table.get_transaction_manager(get_thd_id(), msg->get_txn_id(), msg->instance_id);
#else
   TxnManager * local_txn_man = txn_table.get_transaction_manager(get_thd_id(),msg->get_txn_id(),0);
#endif
  return local_txn_man;
}

RC WorkerThread::run() {
  tsetup();
  printf("Running WorkerThread %ld\n",_thd_id);

  uint64_t agCount=0;
  uint64_t ready_starttime;
  uint64_t idle_starttime = 0;

#if CONSENSUS == PBFT || CONSENSUS == DBFT
    // Setting batch (only relevant for batching threads).
    next_set = 0;
 #if RBFT_ON
    g_instance_id = (g_node_id + g_instance_offset) % g_node_cnt;
 #endif
#endif

  while(!simulation->is_done()) {
    txn_man = NULL;
    heartbeat();

    progress_stats();
#if CONSENSUS == PBFT || CONSENSUS == DBFT
  #if VIEW_CHANGES == true
    //did timer run out(execution taking too long)
    if(get_thd_id() == 0) {
     if (g_node_id != g_view && g_server_timer.checkTimer()) {
      if(!g_changing_view) {
        g_changing_view = true;
    	g_server_timer.deleteTimers();
    	
    	cout << "begin changing view" << endl << endl << endl;
    	if(g_node_id == ((g_view + 1) % g_node_cnt)) {
    		continue; // new primary does not send view change messages
    	}
    	
    	Message * msg = Message::create_message(PBFT_VIEW_CHANGE);
    	((PBFTViewChangeMsg*)msg)->init();
    	
    	for (uint64_t i = 0; i < g_node_cnt; i++) {//send view change messages
    	  if (i == g_view) {
    	  	continue;//avoid sending msg to primary
    	  }
    	  else if(i == g_node_id){//process a message from itself 
    	  	char * buf = (char*)malloc(((PBFTViewChangeMsg*)msg)->get_size() + 1);
    	  	((PBFTViewChangeMsg*)msg)->copy_to_buf(buf);
    	  	Message * deepCopyMsg = Message::create_message(buf);
    	  	process_pbft_view_change_msg(deepCopyMsg);
    	  }
    	  else{
    	  	char * buf = (char*)malloc(((PBFTViewChangeMsg*)msg)->get_size() + 1);
    	  	((PBFTViewChangeMsg*)msg)->copy_to_buf(buf);
    	  	Message * deepCopyMsg = Message::create_message(buf);
    	  	msg_queue.enqueue(get_thd_id(), deepCopyMsg , i);
    	  } }
      } } }
  #endif
#endif
    Message * msg = work_queue.dequeue(get_thd_id());
    if(!msg) {
      if(idle_starttime ==0)
        idle_starttime = get_sys_clock();
      continue;
    }
    if(idle_starttime > 0) {
      INC_STATS(_thd_id,worker_idle_time,get_sys_clock() - idle_starttime);
      idle_starttime = 0;
    }
    //uint64_t starttime = get_sys_clock();

#if VIEW_CHANGES == true
    // Only next primary.
    if(g_node_id == (local_view[get_thd_id()] + 1) % g_node_cnt) {
      UInt32 tcount;
      uint32_t nchange = false;
	
    #if CONSENSUS == PBFT
      tcount = g_thread_cnt - g_btorder_thd - g_execute_thd;
    #else
      tcount = g_thread_cnt - g_btorder_thd - g_prep_thd - g_execute_thd;
    #endif

      if(get_thd_id() > 0 && get_thd_id() < tcount) {
	cout << "YES \n";
	fflush(stdout);
         ctbtchMTX[get_thd_id()-1].lock();
         nchange = ctbtchView[get_thd_id()-1];
         ctbtchMTX[get_thd_id()-1].unlock();

         if(nchange) {
         	local_view[get_thd_id()] = g_node_id;
         	ctbtchView[get_thd_id()-1] = false;
         }
      } else if(get_thd_id() >= tcount && get_thd_id() < (tcount + g_btorder_thd)) {
	  cout << "SET \n";
	  fflush(stdout);

          btsendMTX.lock();
          nchange = btsendView;
          btsendMTX.unlock();

          if(nchange) {
          	local_view[get_thd_id()] = g_node_id;
          	btsendView = false;
          }
      } else if(get_thd_id() >= (tcount + g_btorder_thd) && 
			get_thd_id() < (tcount + g_btorder_thd + g_prep_thd)) {
	  cout << "GO \n";
	  fflush(stdout);

          prepMTX.lock();
          nchange = prepView;
          prepMTX.unlock();

          if(nchange) {
          	local_view[get_thd_id()] = g_node_id;
          	prepView = false;
          } 
      } }

    // Client batch should not be processed by thd_0 in primary.
    if(g_node_id == local_view[get_thd_id()]) {
	if(msg->rtype == CL_BATCH) {
	  if(get_thd_id() == 0) {
	   // cout << "HERE! \n";
	   // fflush(stdout);

	    work_queue.enqueue(get_thd_id(),msg,true);   
	    continue;	
	  } } } 
#endif


#if CONSENSUS == DBFT
  #if RBFT_ON
    if(msg->rtype != CL_QRY && msg->rtype != BATCH_REQ
		&& msg->rtype != CL_BATCH && msg->rtype != PP_MSG 
		&& msg->rtype != PROP_BATCH) {
  #else
    if(msg->rtype != CL_QRY && msg->rtype != BATCH_REQ 
		&& msg->rtype != CL_BATCH && msg->rtype != PP_MSG) {
  #endif
#elif CONSENSUS == PBFT
  #if RBFT_ON
    if(msg->rtype != CL_QRY && msg->rtype != BATCH_REQ
		&& msg->rtype != CL_BATCH && msg->rtype != PROP_BATCH) {
  #else
   #if ZYZZYVA == true && LOCAL_FAULT == true
    if(msg->rtype != CL_QRY && msg->rtype != BATCH_REQ 
		&& msg->rtype != CL_BATCH && msg->rtype != CL_CC) {
   #else
    if(msg->rtype != CL_QRY && msg->rtype != BATCH_REQ && msg->rtype != CL_BATCH) {
   #endif // LOCAL_FAULT
  #endif // RBFT_ON
#endif // CONSENSUS
      txn_man = get_transaction_manager(msg);

      txn_man->txn_stats.work_queue_time_short += msg->lat_work_queue_time;
      txn_man->txn_stats.cc_block_time_short += msg->lat_cc_block_time;
      txn_man->txn_stats.cc_time_short += msg->lat_cc_time;
      txn_man->txn_stats.msg_queue_time_short += msg->lat_msg_queue_time;
      txn_man->txn_stats.process_time_short += msg->lat_process_time;
      txn_man->txn_stats.network_time_short += msg->lat_network_time;

      txn_man->txn_stats.lat_network_time_start = msg->lat_network_time;
      txn_man->txn_stats.lat_other_time_start = msg->lat_other_time;
      
      txn_man->txn_stats.msg_queue_time += msg->mq_time;
      txn_man->txn_stats.msg_queue_time_short += msg->mq_time;
      msg->mq_time = 0;
      txn_man->txn_stats.work_queue_time += msg->wq_time;
      txn_man->txn_stats.work_queue_time_short += msg->wq_time;
      //txn_man->txn_stats.network_time += msg->ntwk_time;
      msg->wq_time = 0;
      txn_man->txn_stats.work_queue_cnt += 1;


      ready_starttime = get_sys_clock();
      bool ready = txn_man->unset_ready();
      INC_STATS(get_thd_id(),worker_activate_txn_time,get_sys_clock() - ready_starttime);
      if(!ready) {
	//cout << "Placing back: Txn: " << msg->txn_id << " Type: " << msg->rtype << "Thd: " << get_thd_id() << "\n";
	//fflush(stdout);
        // Return to work queue, end processing
        work_queue.enqueue(get_thd_id(),msg,true);
        continue;
      }
      txn_man->register_thread(this);
    }

#if CONSENSUS == DBFT
    // Message of type PP_MSG
    if(msg->rtype == PP_MSG) {
	if(msg->txn_id != expectedPrepCount) {
	  // Return to work queue.
	  work_queue.enqueue(get_thd_id(),msg,true);
	  continue;
	} }     
#endif

#if CONSENSUS == PBFT || CONSENSUS == DBFT
  #if RBFT_ON
    if(g_instance_id == msg->instance_id) {
  #else
    if(g_node_id == local_view[get_thd_id()]) {
  #endif
   	// Message of type BATCH_REQ
   	if(msg->rtype == BATCH_REQ) {
   	    if(msg->txn_id != expectedBatchCount) {
   	      // Return to work queue.
	      agCount++;
   	      work_queue.enqueue(get_thd_id(),msg,true);
   	      continue;
   	    } } }
#endif

    process(msg);

    ready_starttime = get_sys_clock();
    if(txn_man) {
      bool ready = txn_man->set_ready();
      assert(ready);
    }
    INC_STATS(get_thd_id(),worker_deactivate_txn_time,get_sys_clock() -ready_starttime);

    // delete message
    ready_starttime = get_sys_clock();
    msg->release();
    INC_STATS(get_thd_id(),worker_release_msg_time,get_sys_clock() - ready_starttime);

  }
  printf("Again: %ld\n",agCount);
  fflush(stdout);
  printf("FINISH %ld:%ld\n",_node_id,_thd_id);
  fflush(stdout);
  
  if(get_thd_id() == 0) {
	printf("RSA: %f\n",((rsatime / rsacount) / BILLION));
	fflush(stdout);

	printf("LRSA:  %f\n",(largersa / BILLION));
	fflush(stdout);

	printf("MXRSA:  %f\n",(maxrsa / BILLION));
	fflush(stdout);

	printf("MNRSA:  %f\n",(minrsa / BILLION));
	fflush(stdout);	

	printf("AVRSA:  %ld\n",rsacount);
	fflush(stdout);
  }

#if ALGORAND == true
  if(!mutex_buf)
    return FINISH;
  CRYPTO_set_id_callback(NULL);
  CRYPTO_set_locking_callback(NULL);
  for(int i = 0;  i < CRYPTO_num_locks();  i++)
    MUTEX_CLEANUP(mutex_buf[i]);
  //free(mutex_buf);
  mutex_buf = NULL;
  ERR_remove_state(0);
#endif

  return FINISH;
}


uint64_t WorkerThread::get_next_txn_id() {
  #if CONSENSUS != DBFT && CONSENSUS != PBFT
  	uint64_t txn_id = ( get_node_id() + get_thd_id() * g_node_cnt) 
					+ (g_thread_cnt * g_node_cnt * _thd_txn_id);
	++_thd_txn_id;
  #else
//    	uint64_t txn_id = global_txn_id;
//	global_txn_id++;
	uint64_t txn_id = g_batch_size * next_set; //(next_set + get_thd_id() - 1);
  #endif
  
  return txn_id;
}

#if CLIENT_BATCH
RC WorkerThread::process_client_batch(Message * msg)
{
	printf("Batch PBFT: %ld, THD: %ld\n",msg->txn_id, get_thd_id());
	fflush(stdout);

	RC rc = RCOK;
	ClientQueryBatch *btch = (ClientQueryBatch*)msg;
	if(!btch->validate()) {
		assert(0);
	} 

#if VIEW_CHANGES == true
	//make sure message was sent to primary
	if(g_node_id != local_view[get_thd_id()]) {
		cout << "REQUEST\n";
		//start timer when client broadcasts an unexecuted message
		g_server_timer.startTimer(picosha2::hash256_hex_string(btch->cqrySet[BATCH_SIZE-1].getString())); 
		
		char * buf1 = (char*)malloc(btch->get_size() + 1);
		btch->copy_to_buf(buf1);
		Message * deepCopyMsg1 = Message::create_message(buf1);
		
		//add message to message log, may be needed and executed 
		//in view change
		deepCopyMsg1->txn_id = 0;
		g_redirected_requests[reCount] = *((ClientQueryBatch*)deepCopyMsg1);
		reCount++;
		
		char * buf2 = (char*)malloc(btch->get_size() + 1);
		btch->copy_to_buf(buf2);
		Message * deepCopyMsg2 = Message::create_message(buf2);
		
		msg_queue.enqueue(get_thd_id(), deepCopyMsg2, local_view[get_thd_id()]);
		return WAIT;
	}
#endif

	//batchMTX.lock();
	next_set = msg->txn_id; // nextSetId++;
	//batchMTX.unlock();

#if ALGORAND == true
  msg->dp = m_dp;
  msg->dp.value = picosha2::hash256_hex_string(btch->cqrySet[BATCH_SIZE-1].getString());
  m_dp.value = msg->dp.value;
  m_dp.seed = g_algorand.seed;

  //test 
  //cout << "NEW BLOCK\n:";
  //fflush(stdout);
  //g_algorand.show_dp(&(m_dp));
  //cout << m_dp.value <<endl;
#endif

	char * buf = (char*)malloc(btch->get_size() + 1);
    	btch->copy_to_buf(buf);
	Message * bmsg = Message::create_message(buf);
	ClientQueryBatch *clbtch = (ClientQueryBatch*)bmsg;



	// Allocate transaction manager for all other requests in batch.
	uint i;
	for(i=0; i<g_batch_size-1; i++) {
	   uint64_t txn_id = get_next_txn_id() + i;

	   //cout << "Txn: " << txn_id << " :: Thd: " << get_thd_id() << "\n";
	   //fflush(stdout);
#if RBFT_ON
           TxnManager * tman =
		txn_table.get_transaction_manager(get_thd_id(), txn_id, g_instance_id);
#else
	   TxnManager * tman = 
		txn_table.get_transaction_manager(get_thd_id(), txn_id, 0);
#endif

           clbtch->cqrySet[i].txn_id = txn_id;
#if RBFT_ON
           if(!tman->allocated) {
        	tman->allocated = true;
#endif
	   	tman->return_id = clbtch->return_node;
	   	tman->client_id = clbtch->cqrySet[i].return_node;
  	   	tman->client_startts = clbtch->cqrySet[i].client_startts;
  	   	((YCSBQuery*)(tman->query))->requests.append(clbtch->cqrySet[i].requests);

#if RBFT_ON
           }
#endif

	   uint64_t relIndex = txn_id % indexSize;
	   cqryStore[relIndex] = clbtch->cqrySet[i];

	   if(txn_id % g_batch_size == 0) {
		// Adding the first transaction of the batch
		batchSet[next_set % indexSize] = txn_id;
	   }
	}

	// Allocating data for the txn marking the batch.	
	uint64_t cindex = get_next_txn_id() + i;

#if RBFT_ON
        txn_man = txn_table.get_transaction_manager(get_thd_id(), cindex, (int)g_instance_id);
#else
        txn_man = txn_table.get_transaction_manager(get_thd_id(), cindex, 0);
#endif
    	txn_man->register_thread(this);
	uint64_t ready_starttime = get_sys_clock();
	bool ready = txn_man->unset_ready();
	assert(ready);
    	INC_STATS(get_thd_id(),worker_activate_txn_time,get_sys_clock() - ready_starttime);
	clbtch->cqrySet[i].txn_id = cindex;

#if RBFT_ON
    	if(!txn_man->allocated) {
    		txn_man->allocated = true;
    		txn_man->register_thread(this);
#endif
		txn_man->return_id = clbtch->return_node;
		txn_man->client_id = clbtch->cqrySet[i].return_node;
  		txn_man->client_startts = clbtch->cqrySet[i].client_startts;
  		((YCSBQuery*)(txn_man->query))->requests.append(clbtch->cqrySet[i].requests);
#if RBFT_ON
    	}
#endif
    	INC_STATS(get_thd_id(),worker_activate_txn_time,get_sys_clock() - ready_starttime);

	uint64_t relIndex = cindex % indexSize;
	cqryStore[relIndex] = clbtch->cqrySet[i];


	//cout << "INSIDE Req: " << clbtch->cqrySet[i].requests[clbtch->cqrySet[i].requests.size()-1]->key << " :: Client: " << txn_man->client_id << "\n";
    	//fflush(stdout);

   	if((cindex + 1) % g_batch_size == 0) {
		// Forwarding message for ordered transmission.
		//cout << "Going to add \n";
		//fflush(stdout);
		txn_man->cbatch = next_set;
#if RBFT_ON
        TxnManager* prop_txn_man;
        while(true) { // TODO: Temp to test for idle time bug
            prop_txn_man = txn_table.get_transaction_manager(get_thd_id(), cindex - 7, 0);
            bool ready = prop_txn_man->unset_ready();
            if(!ready) continue;
            else {
               prop_txn_man->register_thread(this);
               break;
            }
        }

        /*prop_txn_man = txn_table.get_transaction_manager(get_thd_id(), cindex - 7, 0);
        bool ready = prop_txn_man->unset_ready();
        assert(ready);
        prop_txn_man->register_thread(this);
        */
        //cout << "Prop txn man txn id: " << cindex - 7 << endl;
        //fflush(stdout);
        prop_txn_man->prop_rsp_cnt++;
        txn_man->cbatch = next_set;

        // forward propagate msgs
       // cout << "Forwarding Prop msgs of batch: " << next_set << std::endl;
       Message * propMsg = Message::create_message(txn_man, PROP_BATCH);
       propMsg->instance_id = 0; // so proper txn_man is gotten
       char * buf3 = (char*)malloc(propMsg->get_size() + 1);
		propMsg->copy_to_buf(buf3);
       for (uint64_t i = 0; i < g_node_cnt; i++) {
   		if (i == g_node_id) {
   		    continue;
   		}

   		Message * deepCpyMsg = Message::create_message(buf3);
   		msg_queue.enqueue(get_thd_id(), deepCpyMsg, i);
       }
       ready = prop_txn_man->set_ready();
       assert(ready);

       // only order messages based on the last prop msg required to be received
       if(prop_txn_man->prop_rsp_cnt >= 0 || prop_txn_man->prop_ready) { // TODO: Temp to test for idle
       //if(prop_txn_man->prop_rsp_cnt == (int)(g_min_invalid_nodes + 1) || prop_txn_man->prop_ready) {

       //if(g_instance_id <= 0) {
       	if(g_instance_id <= (uint)g_min_invalid_nodes) { // TODO: Temp to test for idle time
           g_view = g_instance_id;
           local_view[get_thd_id()] = g_instance_id;
#endif
        //cout << "About to make BR msg of txn_id: " << cindex << endl;
        Message * deepCMsg = Message::create_message(txn_man, BATCH_REQ);

#if ALGORAND == true
        deepCMsg->dp = m_dp;
#endif

#if RBFT_ON
        deepCMsg->instance_id = g_instance_id;
#endif
	work_queue.enqueue(get_thd_id(), deepCMsg, false);
	//rc = txn_man->start_commit();

#if CONSENSUS == DBFT
	// Sending message to itself for transmitting prepare msgs.
    	Message * deepCopyMsg = Message::create_message(txn_man, PP_MSG);
  #if RBFT_ON
        deepCopyMsg->instance_id = g_instance_id;
  #endif
   	work_queue.enqueue(get_thd_id(), deepCopyMsg, false);
#endif

#if RBFT_ON
	} }
#endif
      }

      return rc;
}

#if RBFT_ON
RC WorkerThread::process_batch_propagate_msg(Message * msg) {

    if( !((PropagateBatch*)msg)->validate()) {
        assert(0);
    }

    while(true) {
        txn_man = txn_table.get_transaction_manager(get_thd_id(), msg->txn_id, 0);
        bool ready = txn_man->unset_ready();
        if(!ready) continue;
        else {
           break;
        }
    }

    txn_man->prop_rsp_cnt++;
    //printf("Propagate Batch: TXN_ID: %ld, THD: %ld\n", txn_man->get_txn_id(), get_thd_id());
    //fflush(stdout);

    if(txn_man->prop_rsp_cnt == 0) {
    //if(txn_man->prop_rsp_cnt == (int)(g_min_invalid_nodes + 1)) { // TODO: Temp to find idle time bug
        //if(g_instance_id <= 0) {
        if(g_instance_id <= (uint)g_min_invalid_nodes) { // TODO: Temporary to find idle time bug
            // In case client batch has not been received yet
            uint64_t relIndex = txn_man->get_txn_id() % indexSize;
            //cout << "Propagate Batch: relIndex: " << relIndex << endl;
            txn_man->cbatch = (txn_man->get_txn_id() - g_batch_size + 8) / g_batch_size;
            if(cqryStore[relIndex].txn_id != txn_man->get_txn_id()) {
                txn_man->prop_ready = true;
            }else {
                g_view = g_instance_id;
                local_view[get_thd_id()] = g_instance_id;

                Message * deepCMsg = Message::create_message(txn_man, BATCH_REQ);
                deepCMsg->instance_id = g_instance_id;
		        work_queue.enqueue(get_thd_id(), deepCMsg, false);

		        #if CONSENSUS == DBFT
		        // Sending message to itself for transmitting prepare msgs.
   	    	    Message * deepCopyMsg = Message::create_message(txn_man, PP_MSG);
   	    	    deepCopyMsg->instance_id = g_instance_id;
   	    	    work_queue.enqueue(get_thd_id(), deepCopyMsg, false);
                #endif
		    }
		}
    }

    return RCOK;
}
#endif

#else // If not client batching

RC WorkerThread::start_pbft(Message * msg)
{
	printf("Start PBFT: %ld\n",msg->txn_id);
	fflush(stdout);

	RC rc = RCOK;
	uint64_t txn_id = UINT64_MAX;

	txn_id = get_next_txn_id();
	msg->txn_id = txn_id;
	
	//add accepted message to message log
	YCSBClientQueryMessage *ycreq = (YCSBClientQueryMessage*)msg;
	char * buf = (char*)malloc(ycreq->get_size() + 1);
    	ycreq->copy_to_buf(buf);
	Message * deepCopyMsg = Message::create_message(buf);

	uint64_t relIndex = txn_id % indexSize;
	cqryStore[relIndex] = *((YCSBClientQueryMessage *)deepCopyMsg);
	
//	cout << "INSIDE Req: " << ycreq->requests[ycreq->requests.size()-1]->key << " :: Index: " << relIndex << "\n";
//    	fflush(stdout);


	// Put txn in txn_table
	txn_man = txn_table.get_transaction_manager(get_thd_id(),txn_id,0);
	txn_man->register_thread(this);
	uint64_t ready_starttime = get_sys_clock();
	bool ready = txn_man->unset_ready();
	INC_STATS(get_thd_id(),worker_activate_txn_time,get_sys_clock() - ready_starttime);
	assert(ready);
	txn_man->txn_stats.starttime = get_sys_clock();
	txn_man->txn_stats.restart_starttime = txn_man->txn_stats.starttime;
	msg->copy_to_txn(txn_man);
	DEBUG("START %ld %f %lu\n",txn_man->get_txn_id(),simulation->seconds_from_start(get_sys_clock()),txn_man->txn_stats.starttime);
	INC_STATS(get_thd_id(),local_txn_start_cnt,1);

	// PBFT Pre-Prepare time
	//txn_man->txn_stats.time_start_pre_prepare = get_sys_clock();

	//cout << "Client Id at start: " << ((SYCSBClientQueryMessage*)msg)->return_node << "\n";
	//fflush(stdout);
	
	rc = init_phase();
	if(rc != RCOK)
	    	return rc;

   #if BATCH_ENABLE == BUNSET
	rc = txn_man->start_commit();
   #else
	if((txn_id + 1) % g_batch_size == 0) {
		rc = txn_man->start_commit();
		//currentBatch++;
	} else {
		if(txn_id % g_batch_size == 0) {
			// Adding the first transaction of the batch
			//batchSet[currentBatch] = txn_id;
		}
		//return rc;
	}
   #endif

	return rc;

}

#endif



#if BATCH_ENABLE == BUNSET
//process incoming pre-prepare messages
RC WorkerThread::process_pbft_pre_msg(Message * msg)
{
	//txn_man->txn_stats.time_start_pre_prepare = get_sys_clock();

	PBFTPreMessage *premsg = (PBFTPreMessage*)msg;
	if(premsg->index > 0) {
		return RCOK;
	}

	printf("PBFTPreMessage Received : Index:%ld : View:%ld txn:%ld\n",premsg->index, premsg->view, premsg->get_txn_id());
	fflush(stdout);

	if ( !premsg->addAndValidate() ) {
		return ERROR; 
	}

   #if CONSENSUS == PBFT
	premsg->copy_to_txn(txn_man);

	g_server_timer.startTimer(premsg->hash); 

	//txn_man->send_pbft_prep_msgs();

	// Only when pre-prepare comes after prepare.
	if(txn_man->prep_rsp_cnt == 0) {
		if(prepared()) {
			txn_man->send_pbft_commit_msgs();
		} }
   #endif	

	return RCOK; 
}

#else
//process incoming batch
RC WorkerThread::process_batch(Message * msg)
{
    #if LOCAL_FAULT 
      if(g_node_id > (uint64_t)g_min_invalid_nodes) {
        if(g_node_id - NODE_FAIL_CNT <= (uint64_t)g_min_invalid_nodes) {
           cout << "FAILING!!!";
           fflush(stdout);
           assert(0);
        } } 
    #endif


#if (CONSENSUS == PBFT || CONSENSUS == DBFT) && !ZYZZYVA
    uint64_t ctime = get_sys_clock();
#endif
    
    BatchRequests *breq = (BatchRequests*)msg;
    
    printf("BatchRequests Received: Index:%ld : View: %ld : Thd: %ld\n",breq->txn_id, breq->view, get_thd_id());
    fflush(stdout);

    // ONLY NON PRIMARY NODES.
#if RBFT_ON
    if(g_instance_id != msg->instance_id) {
#else

    if(g_node_id != g_view) {
#endif

	if ( !breq->addAndValidate() ) {
		return ERROR; 
	}

	// Allocate transaction manager for all other requests in batch.
	uint i;
	for(i=0; i<g_batch_size-1; i++) {
#if RBFT_ON
           TxnManager * tman =
           txn_table.get_transaction_manager(get_thd_id(), breq->index[i], msg->instance_id);

         if(!tman->allocated) {
           tman->allocated = true;
#else
	   TxnManager * tman =
		txn_table.get_transaction_manager(get_thd_id(), breq->index[i], 0);
#endif

	   tman->return_id = breq->return_node_id;
	   tman->client_id = breq->requestMsg[i].return_node;
  	   tman->client_startts = breq->requestMsg[i].client_startts;
  	   ((YCSBQuery*)(tman->query))->requests.append(breq->requestMsg[i].requests);

	  // cout << "INSIDE Req: " << breq->requestMsg[i].requests[breq->requestMsg[i].requests.size()-1]->key << " :: Client: " << tman->client_id << "\n";
    	  // fflush(stdout);

#if RBFT_ON
         }
#endif
        }

	// Allocating data for the txn marking the batch.
	uint64_t cindex = breq->index[i];
#if RBFT_ON
        txn_man = txn_table.get_transaction_manager(get_thd_id(), cindex, msg->instance_id);
        uint64_t ready_starttime = get_sys_clock();
	bool ready = txn_man->unset_ready();
	INC_STATS(get_thd_id(),worker_activate_txn_time,get_sys_clock() - ready_starttime);
	assert(ready);

        txn_man->instance_id = msg->instance_id;

       if(!txn_man->allocated) {
        txn_man->allocated = true;
#else
	txn_man = txn_table.get_transaction_manager(get_thd_id(), cindex, 0);
	uint64_t ready_starttime = get_sys_clock();
	bool ready = txn_man->unset_ready();
	INC_STATS(get_thd_id(), worker_activate_txn_time, get_sys_clock() - ready_starttime);
	assert(ready);
#endif
	txn_man->register_thread(this);
	txn_man->return_id = breq->return_node_id;
	txn_man->client_id = breq->requestMsg[i].return_node;
  	txn_man->client_startts = breq->requestMsg[i].client_startts;
  	((YCSBQuery*)(txn_man->query))->requests.append(breq->requestMsg[i].requests);

//	cout << "INSIDE Req: " << breq->requestMsg[i].requests[breq->requestMsg[i].requests.size()-1]->key << " :: Client: " << txn_man->client_id << "\n";
//    	fflush(stdout);

#if RBFT_ON
       }
#endif

   #if CONSENSUS == PBFT && !ZYZZYVA
        // Send Prepare messages.

#if ALGORAND == true
       //get batch
       g_dp = msg->dp;
       m_dp.value = msg->dp.value;
       if(m_dp.seed != msg->dp.seed) {
          cout<< "WRONG SEED: \n";
          g_algorand.show_dp(&(msg->dp));
          fflush(stdout);
       }

//verify and count votes from primary node
    int votes = g_algorand.verify(&(msg->dp),20,0,g_algorand.sum_w);
    if(votes != -1) {
        int ba_result = g_algorand.countVotes(0.68,20,&(msg->dp));
        if(ba_result != -1){
          cout<<"BA result: "<<ba_result<<" FOR: "<<msg->dp.seed[0]<<'\n';
          fflush(stdout);
        }
        else {
          cout<<"ERROR: double voting"<<'\n';
          fflush(stdout);
        }
    }
    else {
      if(msg->dp.j != 0){
        cout<<"Vote fail: "<<msg->dp.node_id<<" WITH: "<<msg->dp.j<<'\n';
        fflush(stdout);
        g_algorand.show_dp(&(msg->dp));
        fflush(stdout);
      }
    }
#endif

        txn_man->send_pbft_prep_msgs();

        // End the pre-prepare counter, as the prepare phase tasks next.
     #if RBFT_ON
        double timepre = 0;
        if(msg->instance_id == g_master_instance) {
          timepre = get_sys_clock() - ctime;
          INC_STATS(get_thd_id(), time_pre_prepare, timepre);
        }
     #else
        double timepre = get_sys_clock() - ctime;
        INC_STATS(get_thd_id(), time_pre_prepare, timepre);
     #endif // RBFT_ON

	// Only when pre-prepare comes after prepare.
	if(txn_man->prep_rsp_cnt <= 0) {
	   if(prepared()) {
	     // Send Commit messages.
	     cout << "Send commit from batch request" << endl;
	     txn_man->send_pbft_commit_msgs();

	     // End the prepare counter.
     #if RBFT_ON
             double timeprep = 0;
             if(msg->instance_id == g_master_instance) {
               timeprep = get_sys_clock() - txn_man->txn_stats.time_start_prepare - timepre;
               INC_STATS(get_thd_id(), time_prepare, timeprep);
             }
     #else
	     double timeprep = get_sys_clock() - txn_man->txn_stats.time_start_prepare - timepre;
	     INC_STATS(get_thd_id(), time_prepare, timeprep);
     #endif
	     double timediff = get_sys_clock()- ctime;

	     // Sufficient number of commit messages would have come.
             if(txn_man->commit_rsp_cnt <= 0) {
	       uint64_t relIndex = cindex % indexSize;
               PBFTCommitMessage pcmsg;
               for(i=0; i<g_node_cnt; i++) {
                 pcmsg = commStore[relIndex][i];
                 if(pcmsg.txn_id == cindex) {
                   break;
                 } }

               if(committed_local(&pcmsg)) {
		// cout << "INSIDE EXECUTION: " << pcmsg.txn_id << "\n";
		// fflush(stdout);

     #if RBFT_ON
                if(msg->instance_id == g_master_instance) {
     #endif
		 Message * tmsg = Message::create_message(txn_man, EXECUTE_MSG);
		 work_queue.enqueue(get_thd_id(),tmsg,false);
     #if RBFT_ON
         	}
     #endif

		 // End the commit counter.
		 INC_STATS(get_thd_id(), time_commit, get_sys_clock() - txn_man->txn_stats.time_start_commit - timediff);
 	       } } } }

   #elif CONSENSUS == DBFT
	// Send Prepare messages.
	//cout << "Sending DBFT PREP: " << txn_man->get_txn_id() << endl;
	txn_man->send_dbft_execute_msgs();

	INC_STATS(_thd_id,msg_node_out,g_node_cnt-1);

	// End the pre-prepare counter, as the prepare phase tasks next.
    #if RBFT_ON
      if(msg->instance_id == g_master_instance) {
    #endif
	double timepre = get_sys_clock() - ctime;
	INC_STATS(get_thd_id(), time_pre_prepare, timepre);

	//string mtriple = mytriple[cindex];
	string mtriple = mytriple[cindex % indexSize];
     	auto it = txn_man->prepCount.find(mtriple);
     	if(it != txn_man->prepCount.end()) {
     		//cout << "Would compare now: " << cindex << "\n";
     		if(it->second >= req_no_prep_msgs) {
     			txn_man->prepared = true;

		        //cout << "send execute from BR - Txn: " << txn_man->get_txn_id() << endl;
			Message * msg = Message::create_message(txn_man, EXECUTE_MSG);
			work_queue.enqueue(get_thd_id(),msg,false);
	
			// End the prepare counter.
	     		double timeprep = get_sys_clock() - txn_man->txn_stats.time_start_prepare - timepre;
	     		INC_STATS(get_thd_id(), time_prepare, timeprep);
		} }
    #if RBFT_ON
      }
    #endif
   #elif ZYZZYVA == true
	// Send requests to Execute thread to run.
	Message *exeMsg = Message::create_message(txn_man, EXECUTE_MSG);
        work_queue.enqueue(get_thd_id(), exeMsg, false);
   #endif  // CONSENSUS
   } else {
   	txn_man =
    #if RBFT_ON
	  txn_table.get_transaction_manager(get_thd_id(), msg->txn_id, msg->instance_id);
    #else
          txn_table.get_transaction_manager(get_thd_id(), msg->txn_id, 0);
    #endif
	bool ready = txn_man->unset_ready();
	assert(ready);

    #if ZYZZYVA == true	
	for(uint64_t i=0; i<g_batch_size; i++) {
	    istate = std::to_string(hashGen((istate + breq->hash[i])));
	    // Storing history in BatchRequests message.
	    breq->stateHash[i] = istate;
	    breq->stateHashSize[i] = istate.length();
	 
	    // Storing history in preStore.
	    preStore[breq->index[i] % indexSize].stateHash = istate;
	}
    #endif
	
	char * buf = (char*)malloc(breq->get_size() + 1);
	breq->copy_to_buf(buf);
	
	INC_STATS(_thd_id,msg_node_out,g_node_cnt-1);
	
#if ALGORAND == true
    cout<<"NEW BATCH: "<<m_dp.seed<<'\n';
    fflush(stdout);
    dataPackage temp;
    temp = g_algorand.sortition(20,0,g_algorand.m_wallet,g_algorand.sum_w,m_dp.value);
    m_dp.seed = g_algorand.seed;
    m_dp.proof = temp.proof;
    m_dp.hash = temp.hash;
    m_dp.j = temp.j;
    m_dp.node_id = g_node_id;
    g_algorand.countVotes(0.68,20,&(m_dp));
#endif

  //broadcast the batch
	for (uint64_t i = 0; i < g_node_cnt; i++) {
	    if (i == g_node_id) {
	        continue;
	    }
	    	
	    Message * deepCopyMsg = Message::create_message(buf);
#if ALGORAND == true
      //broadcast the block
      deepCopyMsg->dp = m_dp;
      deepCopyMsg->dp.type = ALGORAND_INIT;
#endif
	    msg_queue.enqueue(get_thd_id(), deepCopyMsg, i);
	}

    #if ZYZZYVA == true
	   // Resetting txn_man
	   TxnManager *local_txn_man = 
	   	txn_table.get_transaction_manager(get_thd_id(), msg->txn_id+2, 0);

	   // Send requests to Execute thread to run.
	   Message *exeMsg = Message::create_message(local_txn_man, EXECUTE_MSG);
           work_queue.enqueue(get_thd_id(), exeMsg, false);
    #endif
   
	   // Setting the next expected prepare message id.
	   expectedBatchCount = g_batch_size + msg->txn_id;
   } 
	return RCOK;
}

#endif


#if EXECUTION_THREAD
// execute messages and sends client response message
RC WorkerThread::process_execute_msg(Message * msg)
{
    	cout << "PROCESS EXECUTE " << msg->txn_id << "\n";
    	fflush(stdout);
    	
    	uint64_t ctime = get_sys_clock();

     #if CLIENT_RESPONSE_BATCH == true
	// This message uses txn_man of index calling process_execute.
	Message *rsp = Message::create_message(txn_man,CL_RSP);
	ClientResponseMessage *crsp = (ClientResponseMessage *)rsp;
	crsp->index.init(g_batch_size);
	crsp->client_ts.init(g_batch_size);
	//uint64_t lcount = 0;
     #endif
    	
    	ExecuteMessage *pcmsg = (ExecuteMessage*)msg;
    	
     #if RBFT_ON
    	TxnManager* prop_txn_man;
    	bool ready; // checking propagate msg quota is delayed until this phase
    	while(true) { // TODO: Temp to test for idle time bug
    	    prop_txn_man = txn_table.get_transaction_manager(get_thd_id(), pcmsg->txn_id - 6, 0);
    	    ready = prop_txn_man->unset_ready();
    	    if(!ready) continue;
    	    else {
    	       prop_txn_man->register_thread(this);
    	       break;
    	    }
    	}
    	assert(prop_txn_man->prop_rsp_cnt >= 1); // TODO: Temp hack
    	ready = prop_txn_man->set_ready();
    	assert(ready);
     #endif // RBFT_ON

	// Execute transactions in a shot
	uint64_t i;
	for(i = pcmsg->index; i<=pcmsg->end_index; i++) {
		//cout << "i: " << i << " :: next index: " << g_next_index << "\n";
		//fflush(stdout);

		TxnManager * local_txn_man = 
		txn_table.get_transaction_manager(get_thd_id(), i,0);
		
		if(i != g_next_index) {
			cout << "i: "<< i << " :: g_next_index: " << g_next_index << "\n";
			fflush(stdout);
			assert(0);
		}
		g_next_index++;
		
		local_txn_man->run_txn();
		local_txn_man->commit();

	     #if CLIENT_RESPONSE_BATCH == true
		crsp->index.add(i);
		crsp->client_ts.add(local_txn_man->client_startts);
		//lcount++;

		#if ZYZZYVA == true
	        	uint64_t ploc = i % indexSize;
	        	crsp->clreq =  cqryStore[ploc];
	        	crsp->stateHash = preStore[ploc].stateHash;
	        	crsp->stateHashSize = crsp->stateHash.size();
	   	#endif
	     #else // Not CLIENT_RESPONSE_BATCH
			Message *rsp = Message::create_message(local_txn_man,CL_RSP);

		#if ZYZZYVA == true
	        	ClientResponseMessage *clrsp = (ClientResponseMessage *)rsp;
	        	uint64_t ploc = i % indexSize;
	        	clrsp->clreq =  cqryStore[ploc];
	        	clrsp->stateHash = preStore[ploc].stateHash;
	        	clrsp->stateHashSize = clrsp->stateHash.size();
	   	#endif

		   	//cout << "Client: " << local_txn_man->client_id << "\n";
		   	//fflush(stdout);
		   	msg_queue.enqueue(get_thd_id(), rsp, local_txn_man->client_id);

			INC_STATS(_thd_id,tput_msg,1);
			INC_STATS(_thd_id,msg_cl_out,1);
	     #endif // Client_Response_Batch
	}

	// Last Batch Transaction.

	while(true) {
	  TxnManager * local_txn_man = 
		txn_table.get_transaction_manager(get_thd_id(), i,0);
	  bool ready = local_txn_man->unset_ready();	
	  if(!ready) {
	    continue;
	  } else {
	    local_txn_man->register_thread(this);
	    assert(i == g_next_index);
	    g_next_index++;
	    
	    local_txn_man->run_txn();
	    local_txn_man->commit();

	#if CLIENT_RESPONSE_BATCH == true
	    crsp->index.add(i);
	    crsp->client_ts.add(local_txn_man->client_startts);

	    #if ZYZZYVA == true
	   	uint64_t ploc = i % indexSize;
	   	crsp->clreq =  cqryStore[ploc];
	   	crsp->stateHash = preStore[ploc].stateHash;
	   	crsp->stateHashSize = crsp->stateHash.size();
	    #endif

	    msg_queue.enqueue(get_thd_id(), crsp, local_txn_man->client_id);
	#else // Not CLIENT_RESPONSE_BATCH
	    
	    Message *rsp = Message::create_message(local_txn_man,CL_RSP);

	    #if ZYZZYVA == true
	   	ClientResponseMessage *clrsp = (ClientResponseMessage *)rsp;
	   	uint64_t ploc = i % indexSize;
	   	clrsp->clreq =  cqryStore[ploc];
	   	clrsp->stateHash = preStore[ploc].stateHash;
	   	clrsp->stateHashSize = clrsp->stateHash.size();
	    #endif

	    //cout << "Client: " << local_txn_man->client_id << "\n";
	    //fflush(stdout);
	    msg_queue.enqueue(get_thd_id(), rsp, local_txn_man->client_id);
    	#endif

	    INC_STATS(_thd_id,tput_msg,1);
	    INC_STATS(_thd_id,msg_cl_out,1);

	    // Send checkpoint messages.
	    if((i+1) % g_txn_per_chkpt == 0){
	        #if CONSENSUS == PBFT
	            local_txn_man->send_checkpoint_msgs();
	        #elif CONSENSUS == DBFT
		   g_last_stable_chkpt = i;
		   DBFTPrepMessage dmsg;
		   for(uint j=0; j< g_node_cnt; j++) {
			if(j == g_node_id) {
			   continue; 	   }
	
			dmsg = prepStore[i % indexSize][g_node_id];
		  	if(dmsg.index == i) {
		   	   g_dbft_chkpt_msgs.push_back(dmsg);
			} }

		   // Delete previous checkpoint messages.
		   for(auto it=g_dbft_chkpt_msgs.begin(); it!=g_dbft_chkpt_msgs.end();) {
		   	if(it->index < g_last_stable_chkpt) {
		   	    it = g_dbft_chkpt_msgs.erase(it);
		   	} else {
		   	    ++it;
		   	} }

	           // for(uint64_t i = lastDeletedTxnMan; i < lastDeletedTxnMan + 100; i++){
	           //     txn_table.release_transaction_manager(get_thd_id(), i,0);
	           // }	
	           // lastDeletedTxnMan = lastDeletedTxnMan + 100;
	        #endif
	    } else {
		#if CONSENSUS == PBFT
			if(local_txn_man->commit_rsp_cnt == (g_min_invalid_nodes - 1)) {
		#endif
			   //cout << "Deleting: " << i << "\n";
			   for(uint64_t j = (i+1-g_batch_size); j < g_batch_size-5; j++) {
				txn_table.release_transaction_manager(get_thd_id(), j,0);
			   } 
		#if CONSENSUS == PBFT
			}
		#endif
	    }

	    if(local_txn_man) {
  	      ready = local_txn_man->set_ready();
  	      assert(ready);
  	    }

	    break;
	  } }

	// End the execute counter.
	INC_STATS(get_thd_id(), time_execute, get_sys_clock() - ctime);
	return RCOK;		
}
#endif


#if CONSENSUS == PBFT

RC WorkerThread::process_pbft_chkpt_msg(Message * msg)
{
//	printf("Process_pbft_chkpt: Txn: %ld :: Node: %ld\n",msg->get_txn_id(), ((CheckpointMessage *)msg)->return_node);
//	fflush(stdout);

	if ( !((CheckpointMessage*)msg)->addAndValidate() ) {
		return ERROR; 
	}

	g_last_stable_chkpt = ((CheckpointMessage*)msg)->index+4;

//	cout << "LastDeleted: " << lastDeletedTxnMan << " :: g_last_stable: " << g_last_stable_chkpt << "\n"; 
//	fflush(stdout);
	
   #if BATCH_ENABLE == BUNSET
	for(uint64_t i = lastDeletedTxnMan; i < lastDeletedTxnMan + 100; i++) {
		txn_table.release_transaction_manager(get_thd_id(), i,0);
	}	
	lastDeletedTxnMan = lastDeletedTxnMan + 100;
   #else
//	for(uint64_t i = lastDeletedTxnMan; i < lastDeletedTxnMan + 100; i++) {
//		txn_table.release_transaction_manager(get_thd_id(), i,0);
//	}	
//	lastDeletedTxnMan = lastDeletedTxnMan + 100;
   #endif
	   
	for(auto it = g_pbft_chkpt_msgs.begin(); it != g_pbft_chkpt_msgs.end(); ) {
		if(it->index+4 < g_last_stable_chkpt) {
			it = g_pbft_chkpt_msgs.erase(it);
		} else {
			++it;
		}
	}

#if VIEW_CHANGES == true
	//g_msg_wait.clear();
#endif

	return RCOK;
}


#if ZYZZYVA == true && LOCAL_FAULT

RC WorkerThread::process_client_certificate(Message *msg) {
	//cout << "Client Certificate: " << msg->txn_id << "\n";
	//fflush(stdout);

	ClientCommitCertificate *clcert = (ClientCommitCertificate *)msg;
	if(!clcert->validate()) {
		return RCOK;
	}
	

	Message *mssg = Message::create_message(CL_CAck);
	ClientCertificateAck *cack = (ClientCertificateAck *)mssg;
	cack->txn_id = cltmap[clcert->client_startts];
	cack->client_startts = clcert->client_startts;
	cack->view = g_view;
	cack->sign();

	msg_queue.enqueue(get_thd_id(), cack, msg->return_node_id);
	
	return RCOK;
}

#endif // Zyzzyva

//process incoming prepare message
RC WorkerThread::process_pbft_prep_msg(Message * msg)
{
#if RBFT_ON
    	txn_man->instance_id = msg->instance_id;
#endif

	cout << "PREPARE " << msg->txn_id << " FROM: " << msg->return_node_id << endl;
  fflush(stdout);

	if(txn_man->prep_rsp_cnt == 2 * g_min_invalid_nodes) {
		txn_man->txn_stats.time_start_prepare = get_sys_clock();
	}

	if ( !((PBFTPrepMessage*)msg)->addAndValidate() ) {
		return ERROR;
	}

#if VIEW_CHANGES == true
	//was this txn already executed (for example, in view changes)
	if(msg->txn_id < g_next_index) {
		return RCOK;
	}
#endif
	
	//check if enough prepare messages have been recieved

#if ALGORAND == true
//verify and count votes
    g_dp = msg->dp;
    m_dp.value = msg->dp.value;
    if(m_dp.seed != msg->dp.seed) {
        cout<< "WRONG SEED: \n";
        g_algorand.show_dp(&(msg->dp));
        fflush(stdout);
    }

  int votes = g_algorand.verify(&(msg->dp),20,0,g_algorand.sum_w);
  if(votes) {
      int ba_result = g_algorand.countVotes(0.68,20,&(msg->dp));
      if(ba_result != -1){
        cout<<"BA result: "<<ba_result<<" FOR: "<<msg->dp.seed[0]<<endl;
        fflush(stdout);
      }
      else {
        cout<<"ERROR: double voting"<<endl;
        fflush(stdout);
      }
  }
  else{
    if(msg->dp.j != 0) {
    cout<<"Vote fail: "<<msg->dp.node_id<<" WITH: "<<msg->dp.j<<endl;
    fflush(stdout);
    g_algorand.show_dp(&(msg->dp));
    fflush(stdout);
    }
  }
#endif


	int responses_left = --(txn_man->prep_rsp_cnt);
	cout << "Responses left: " << responses_left << "\n";
	fflush(stdout);

	if(responses_left == 0 && !txn_man->prepared) {
		if (prepared()) {
			cout << "SENDING \n";
			fflush(stdout);

			// Send Commit messages.
			txn_man->send_pbft_commit_msgs();

			// End the prepare counter.
		   	INC_STATS(get_thd_id(), time_prepare, get_sys_clock() - txn_man->txn_stats.time_start_prepare);
		}
	}
	
	return RCOK; 
}

bool WorkerThread::prepared()
{
#if RBFT_ON
    	if(txn_man->instance_id != (int)g_master_instance) {
        	txn_man->prepared = true;
    	}
#endif
	if(txn_man->prepared) {
		return true;
	}

	cout << "Inside PREPARED: " << txn_man->get_txn_id() << "\n";
	fflush(stdout);
	
	int j = 0;
	uint64_t txn_id = txn_man->get_txn_id();
	
#if BATCH_ENABLE == BUNSET

	PBFTPreMessage preMsg;

	// Checking existence of client query
	uint64_t relIndex = txn_id % indexSize;
	if(cqryStore[relIndex].txn_id != txn_id) {
		if(g_node_id == g_view) {
			assert(0); 
		}
		cout << "FALSE: " << txn_man->get_txn_id();
		return false;
	}

	// Checking existence of pre-prepare message, 
	// As client query exists so needs to be true.
	preMsg = preStore[relIndex];
	if(preMsg.txn_id != txn_id) {
		assert(0);
	}

	//counts number of accepted prepare messages with matching hash, view, index
	PBFTPrepMessage pmsg; 
	for(uint64_t i=0; i<g_node_cnt; i++) {
		pmsg = prepPStore[relIndex][i];
		if(pmsg.txn_id == txn_id) {
			j++;
			if(pmsg.hash != preMsg.hash) {
				assert(0);		
			} } }

	//make sure enough prep msgs were found
	if (j < floor(2 * g_min_invalid_nodes)) {
		cout << "HERE \n";
		return false; //not enough prepare messages
	}
	
	txn_man->prepared = true;
	return true;

#else
	//counts number of accepted prepare messages with matching hash, view, index
	j = 0;
	//bool flag = false, 
	bool sflag = false;
	//bool found[g_batch_size] = {0};
	PBFTPreMessage premsg;
	PBFTPrepMessage pmsg;
	uint64_t bidx, eidx;

	for(uint64_t i=0; i<g_node_cnt; i++) {
		pmsg = prepPStore[txn_id % indexSize][i];
		if(!(pmsg.txn_id == txn_id)) {
			//cout << "DO AGAIN -- Prep Index: " << pmsg.index << "\n";
			//fflush(stdout);
			continue;
		}

		if(!sflag) {
			bidx = pmsg.index;
			eidx = pmsg.end_index;
			sflag = true;
	
			//cout << "Bidx: " << bidx << " :: eidx: " << eidx << "\n";
			//fflush(stdout);

		} else {
			if(pmsg.end_index != eidx || pmsg.index != bidx) {
				assert(0);
			} }

		
		// Check if client query exists for every txn id in range bidx to eidx
		for(uint64_t i=bidx; i<=eidx; i++) {
			uint64_t relIndex = i % indexSize;
			if(cqryStore[relIndex].txn_id != i) {
				if(g_node_id == g_view) {
		 	    #if !RBFT_ON
					assert(0);
			    #endif
				}
				// cout << "PROBLEM! \n";
				// fflush(stdout);
			    #if !RBFT_ON
				cout << "HELL!";
				fflush(stdout);
				return false;
			    #endif
			} }

		// Ensure that pre-prepare message exists.
		for(uint64_t i=bidx; i<=eidx; i++) {
			uint64_t relIndex = i % indexSize;
			if(preStore[relIndex].txn_id != i) {
#if RBFT_ON
                		return false;
#else
				assert(0);
#endif
			}
#if RBFT_ON
        		// Ensure that pre-prepares for THIS instance are stored
        		if((int)preStore[relIndex].instance_id != txn_man->instance_id
        			&& txn_man->instance_id == (int)g_master_instance) {
        		    	return false;
        		}
#endif
		}

		j++;
		if (j >= floor(2 * g_min_invalid_nodes)) {
			txn_man->prepared = true;
			return true;
		} 
	} 

    cout << "not enough prep msgs" << endl;
    fflush(stdout);
	return false;
#endif

}


//processes incoming commit message, 
RC WorkerThread::process_pbft_commit_msg(Message * msg)
{
	cout << "PROCESS COMMIT " << msg->txn_id << " FROM: " << msg->return_node_id << "\n";
  	fflush(stdout);
#if RBFT_ON
    	txn_man->instance_id = msg->instance_id;
#endif

	if(txn_man->commit_rsp_cnt == 2 * g_min_invalid_nodes + 1) {
		txn_man->txn_stats.time_start_commit = get_sys_clock();
	}

	PBFTCommitMessage *pcmsg = (PBFTCommitMessage*)msg;
	if ( !pcmsg->addAndValidate() ) {
		return ERROR;
	}

#if VIEW_CHANGES == true
	//was this txn already executed (for example, in view changes)
	if(pcmsg->index < g_next_index) {
		g_server_timer.endTimer(pcmsg->hash);
		return WAIT;
	}
#endif
	
	int responses_left = --(txn_man->commit_rsp_cnt);


	cout << "Com Response: " <<responses_left << " :: Txn: " << msg->txn_id << "\n";
	fflush(stdout);

	if(responses_left == 0 && !txn_man->committed_local) {
	    if (committed_local(pcmsg)) {
		  cout << "TO EXECUTE: " << msg->txn_id << "\n";
		  fflush(stdout);

#if VIEW_CHANGES == true

		//debug, if view changes are on, crash so you can see a view change
		if(g_node_id == 0) {
			if(msg->txn_id > (g_txn_per_chkpt + g_batch_size)) {
				assert(0);
			} }

		g_server_timer.endTimer(pcmsg->hash); 
#endif

		// Add this message to execute thread's queue.
#if RBFT_ON
              if(msg->instance_id == g_master_instance) {
#endif
            	Message *msg = Message::create_message(txn_man, EXECUTE_MSG);
            	work_queue.enqueue(get_thd_id(), msg, false);

#if ALGORAND == true
//execute the txns and end this round
              g_algorand.execute((int)m_dp.seed[0]); //do execution simulation
              g_algorand.reset(); //end this round
              m_dp.seed = g_algorand.seed;
              m_dp.type = ALGORAND_RESET;
#endif

#if RBFT_ON
              }
#endif

        	// End the commit counter.
        	INC_STATS(get_thd_id(), time_commit, get_sys_clock() - txn_man->txn_stats.time_start_commit);
         } }

    return RCOK;
}

//checks if bool committed_local should be true, returns result
bool WorkerThread::committed_local(PBFTCommitMessage * msg)
{
    //is the node prepared (is bool prepared true)
    if (!txn_man->prepared) {
        cout << "Committed local: not prepared" << endl;
        return false;
    }

#if RBFT_ON
    if(txn_man->instance_id != (int)g_master_instance) {
        txn_man->committed_local = true;
        return true;
    }
#endif

    PBFTPrepMessage pmsg;
    uint64_t relIndex = msg->txn_id % indexSize;
    for(uint64_t i=0; i<g_node_cnt; i++) {
        pmsg = prepPStore[relIndex][i];
        if(pmsg.index == msg->index) {
            break;
        } }

    //count number of accepted commit messages that match pre-prepare
    int k = 0;
    bool sflag = false;
    uint64_t bidx, eidx;
    PBFTCommitMessage cmsg;

    for(uint64_t i=0; i<g_node_cnt; i++) {
        cmsg = commStore[relIndex][i];

        if (cmsg.index == pmsg.index) {
            if(!sflag) {
                bidx = cmsg.index;
                eidx = cmsg.end_index;
                sflag = true;
            } else {
                if(cmsg.end_index != eidx || cmsg.index != bidx) {
                    assert(0);
                } }

            k++;
#if !RBFT_ON
            assert(pmsg.hash == cmsg.hash);
            assert(pmsg.view == cmsg.view);
#endif

#if RBFT_ON
           if(txn_man->instance_id == (int)g_master_instance)
#endif
            assert(pmsg.end_index == cmsg.end_index);
        } }

#if RBFT_ON
    if(txn_man->instance_id == (int)g_master_instance) { // other instances complete before master instance
#endif
    if (k < (2 * g_min_invalid_nodes + 1)) {
        return false;
    }
#if RBFT_ON
    }
#endif

    txn_man->committed_local = true;
    return true;
}

#endif

#if CONSENSUS == DBFT

RC WorkerThread::process_dbft_prep_msg(Message * msg)
{
	if(!txn_man->dbft_beg) {
		txn_man->txn_stats.time_start_prepare = get_sys_clock();
		txn_man->dbft_beg = true;
	}

#if VIEW_CHANGES == true
	//was this txn already executed (for example, in view changes)
	if(msg->txn_id < g_next_index) {
		return RCOK;
	}
#endif

	DBFTPrepMessage* dmsg = (DBFTPrepMessage*)msg;
#if RBFT_ON
    	//printf("process_DBFTPrepMessage: Index:%ld : Instance:%ld Txn:%ld Return Node:%ld\n",dmsg->index, dmsg->instance_id, dmsg->get_txn_id(), dmsg->return_node);
#else
	printf("process_DBFTPrepMessage: Index:%ld : View:%ld Txn:%ld Return Node:%ld\n",dmsg->index, dmsg->view, dmsg->get_txn_id(), dmsg->return_node);
#endif
    	//fflush(stdout);

#if RBFT_ON
      if(dmsg->instance_id == g_master_instance)
#endif
	prepStore[dmsg->index % indexSize][dmsg->return_node] = *dmsg;

#if RBFT_ON
    	string mtriple = mytriple[(dmsg->txn_id + 6) % indexSize];
#else
	string mtriple = mytriple[dmsg->txn_id % indexSize];
#endif
	string ntriple = dmsg->tripleSign;

//	cout << "Triple local: " << ntriple << "\n";
//	fflush(stdout);

	auto it = txn_man->prepCount.find(ntriple);
	if(it != txn_man->prepCount.end()) {
		it->second = it->second + 1;
		//cout << "mtriple: " << mtriple << "\n";
		//cout << "ntriple: " << ntriple << "\n";
		if(mtriple.compare(ntriple) == 0) {
			//cout << "Inside first\n";
			if(it->second >= req_no_prep_msgs) { 
				//cout << "Inside second\n";
			if(!txn_man->prepared) { 
				//cout << "inside third\n";
			if(prepared(dmsg)) {
			   //cout << "inside fourth\n";

		      #if VIEW_CHANGES == true

			   //If view changes are on, crash so you can see a view change
			   if(g_node_id == 0) {
			   	if(msg->txn_id > (g_txn_per_chkpt + g_batch_size)) {
			   		assert(0);
			   	} }

			   g_server_timer.endTimer(dmsg->hash); 
              	      #endif

		      #if RBFT_ON
                	 if(msg->instance_id == g_master_instance) {
		      #endif

			   // Add this message to execute thread's queue.
			   Message * msg = Message::create_message(txn_man, EXECUTE_MSG);
			   work_queue.enqueue(get_thd_id(),msg,false);

			   // End the prepare counter.
		   	   INC_STATS(get_thd_id(), time_prepare, get_sys_clock() - txn_man->txn_stats.time_start_prepare);
#if RBFT_ON
                	 }
#endif
			} else {
#if RBFT_ON
                		assert(msg->instance_id != g_master_instance);
#else
				assert(0);
#endif
			} } 
			} } else {
			//cout << "Cannot compare now\n";
		}
	} else { // Triple not found.
		//cout << "New Triple: " << ntriple << "\n";
		txn_man->prepCount[ntriple] = 1;
	}
      
	return RCOK;
}

//checks if bool prepared should be true, returns result
bool WorkerThread::prepared(DBFTPrepMessage * msg)
{
#if RBFT_ON // only primary instance matters
    	if(msg->instance_id != g_master_instance)
    		return false;
#endif
	int j = 0;
	PBFTPreMessage preMsg;

#if RBFT_ON
    	uint64_t sidx = (msg->txn_id + 7) - g_batch_size;
    	for(uint64_t i = sidx; i<=msg->txn_id + 6; i++) {
#else
	uint64_t sidx = (msg->txn_id + 1) - g_batch_size;
	for(uint64_t i = sidx; i<=msg->txn_id; i++) {
#endif
		uint64_t relIndex = i % indexSize;
		preMsg = preStore[relIndex];
		if(preMsg.txn_id != i) {
			assert(0);
		}
	}

#if RBFT_ON
    	uint64_t ind = msg->txn_id % indexSize;
    	preMsg = preStore[ind];
#endif

	j = 0;
	DBFTPrepMessage dmsg; 
	uint64_t relIndex = msg->txn_id % indexSize;
	for(uint64_t i=0; i<g_node_cnt; i++) {
		dmsg = prepStore[relIndex][i];
#if RBFT_ON
              	if(dmsg.txn_id == msg->txn_id && dmsg.instance_id == msg->instance_id) {
#else
		if(dmsg.txn_id == msg->txn_id) {
#endif
			j++;
			if(dmsg.hash != preMsg.hash) {
#if RBFT_ON
                		//ignore if not master instance
                		cout << "For txn: " << dmsg.txn_id << " dmsg hash: " << dmsg.hash << " vs. " << preMsg.hash << endl;
                		fflush(stdout);
                		if(dmsg.instance_id == g_master_instance) {
#endif
                		  assert(0);
#if RBFT_ON
                		}
#endif
            		} } }

    	// Check 2f+1 prepare messages.
    	//cout << "j: " << j << "req_no_prep: " << req_no_prep_msgs << "\n";
    	//fflush(stdout);

    	if (j < req_no_prep_msgs)
    	    return false;

    	txn_man->prepared = true;
    	return true;
}


RC WorkerThread::primary_send_prepare(Message *msg) {
    //cout << "Primary send prepare: " << msg->txn_id << " Thd: " << get_thd_id() << "\n";
    //fflush(stdout);

#if VIEW_CHANGES == true
    //was this txn already executed (for example, in view changes)
    if(msg->txn_id < g_next_index) {
        cout << "Returning: " << msg->txn_id << "\n";
        fflush(stdout);
        return RCOK;
    }
#endif

    while(true) {
       txn_man =
            txn_table.get_transaction_manager(get_thd_id(), msg->txn_id, 0);
       bool ready = txn_man->unset_ready();
       if(!ready) {
            continue;
       } else {
            txn_man->register_thread(this);
#if RBFT_ON
            txn_man->instance_id = msg->instance_id;
#endif
            //cout << "Sending DBFT PREP: " << txn_man->get_txn_id() << endl;
            //fflush(stdout);
	    INC_STATS(_thd_id,msg_node_out,g_node_cnt-1);
            txn_man->send_dbft_execute_msgs();
            break;
       } }

    // Setting the next expected prepare message id.
    expectedPrepCount = g_batch_size + msg->txn_id;

    return RCOK;
}
#endif


RC WorkerThread::init_phase() {
  RC rc = RCOK;
  //m_query->part_touched[m_query->part_touched_cnt++] = m_query->part_to_access[0];
  return rc;
}


bool WorkerThread::is_cc_new_timestamp() {
  return (CC_ALG == MVCC || CC_ALG == TIMESTAMP);
}


#if VIEW_CHANGES == true
RC WorkerThread::process_pbft_view_change_msg(Message * msg)
{
	cout << "PROCESS VIEW CHANGE " << "\n";

	//if primary recieved message 
	if(g_node_id == g_view) {
		return RCOK; // primary ignores message
	}

	// Old View messages ignored.
	if(((PBFTViewChangeMsg*)msg)->view <= g_view) {
		return RCOK;
	}
	
	//assert view is correct
	assert( ((PBFTViewChangeMsg*)msg)->view == ((g_view + 1) % g_node_cnt)); 
	
	cout << "validating view change message" << endl;
	
	assert(msg->rtype == PBFT_VIEW_CHANGE);
	if ( !((PBFTViewChangeMsg*)msg)->addAndValidate() ) {
//		cout << "waiting for more view change messages" << endl;
		return RCOK; //ignores message basically
	}
	
	cout << "executing view change message" << endl;
	
	//if this node is going to be the new primary
	if(g_node_id == ((g_view + 1) % g_node_cnt)) {
		g_view = g_node_id; //move to new view
		local_view[get_thd_id()] = g_view;
				
		while (!g_msg_wait.empty())
		{
			cout << "Shouldn't enter here! \n";
			fflush(stdout);

			work_queue.enqueue(get_thd_id(),g_msg_wait.back(),false);
			g_msg_wait.pop_back();
		}
		
		Message * newViewMsg = Message::create_message(PBFT_NEW_VIEW);
		PBFTNewViewMsg *nvmsg = (PBFTNewViewMsg *)newViewMsg;
		nvmsg->init(get_thd_id());
		
		this->reset();
		
		BatchRequests bmsg;
		for(uint i = 0; i < nvmsg->prePrepareMessages.size(); i++) {
			bmsg = nvmsg->prePrepareMessages[i];

			//add preprepare messages to log
			uint64_t idx = bmsg.index[g_batch_size-1];
			breqStore[idx % indexSize] = bmsg;
			
			for(uint j = 0; j < g_batch_size; j++) {
				Message * msg = Message::create_message(PBFT_PRE_MSG);
				PBFTPreMessage * premsg = (PBFTPreMessage *)msg;
				premsg->txn_id 		= bmsg.index[j];
				premsg->index 		= bmsg.index[j];
				premsg->hashSize 	= bmsg.hashSize[j];
				premsg->hash 		= bmsg.hash[j];
				premsg->requestMsg 	= bmsg.requestMsg[j];
				// We should be storing the premsg.
				preStore[premsg->txn_id % indexSize] = *premsg;
			} }

		for (uint64_t i = 0; i < g_node_cnt; i++) {//send new view messages
			if(i == g_node_id){
				continue;
			}
			char * buf = (char*)malloc(nvmsg->get_size() + 1);
			nvmsg->copy_to_buf(buf);
			Message * deepCopyMsg = Message::create_message(buf);
			
			msg_queue.enqueue(get_thd_id(), deepCopyMsg, i);
		}
		
		// now accepting messages other than view change related stuff
		g_changing_view = false;

		// Changing view for input thread.
		inputMTX.lock();
		inputView = true;
		inputMTX.unlock();

		// Changing view for batch ordering thread.
		btsendMTX.lock();
		btsendView = true;
		btsendMTX.unlock();

	     #if CONSENSUS == DBFT
		prepMTX.lock();
		prepView = true;
		prepMTX.unlock();
	     #endif

		// Changing view for batch creation thread.
		for(uint i=0; i < g_batch_threads; i++) {
			ctbtchMTX[i].lock();
			ctbtchView[i] = true;
			ctbtchMTX[i].unlock();
		}

//		cout << "new primary changed view" << endl;
		
		g_pbft_view_change_msgs.clear();  
		g_server_timer.deleteTimers(); 
		
		bool found = false;
		vector<Message*> msgs_to_execute;
		
		// erase redirected requests already in g_client_req_msgs queue 
		// and add redirected requests if not in g_client_req_msgs
		for(uint64_t i = 0; i < reCount; i++) {
			found = false;
			for(uint64_t j=0; j < indexSize; j++) {
				if(g_redirected_requests[i].cqrySet[0].client_startts == cqryStore[j].client_startts)
				{
					cout << "Deleting req \n";
					fflush(stdout);

					found = true;
				}
			}
			if(!found) {
				cout << "found a message" << endl;
				ClientQueryBatch reqMsg = g_redirected_requests[i];
				
				char * buf = (char*)malloc(reqMsg.get_size() + 1);
				reqMsg.copy_to_buf(buf);
				Message * deepCopyMsg = Message::create_message(buf);
				
				msgs_to_execute.push_back(deepCopyMsg);
			}
		}

		reCount = 0;
		
		expectedBatchCount = g_next_index + g_batch_size - 3;

	    #if CONSENSUS == DBFT
		expectedPrepCount = g_next_index + g_batch_size - 4;
	    #endif

		cout << "expectedBatchCount = " << g_next_index << " + " << g_batch_size << endl;
		fflush(stdout);
		
		//send pre messages for requests in work queue but not in new view message.
		for(uint i = 0; i < msgs_to_execute.size(); i++) {
//			cout << "adding ClientQueryBatch to work queue" << endl;
			work_queue.enqueue(get_thd_id(),msgs_to_execute[i],false);
		}
		
	}
	else { ; } //do nothing

	return RCOK;
}

RC WorkerThread::process_pbft_new_view_msg(Message * msg)
{
	cout << "PROCESS NEW VIEW " << msg->txn_id << "\n";
	if ( !((PBFTNewViewMsg*)msg)->validate() )
	{
		assert(0);
		return RCOK; //ignores message basically
	}

	this->reset();
	g_view = ((PBFTNewViewMsg*)msg)->view; //accept new view
	local_view[get_thd_id()] = g_view;
	g_changing_view = false; //begin accepting non-viewchange related messages
	
   #if CONSENSUS == PBFT
	//send prepare messages for each preprepare
	for(uint i = 0; i < ((PBFTNewViewMsg*)msg)->prePrepareMessages.size(); i++)//for every preprepare message
	{
		BatchRequests breq = ((PBFTNewViewMsg*)msg)->prePrepareMessages[i];
		
		char * buf = (char*)malloc(breq.get_size() + 1);
		breq.copy_to_buf(buf);
		Message * deepCopyMsg = Message::create_message(buf);
		
		process_batch(deepCopyMsg);
	}
   #endif
	
	g_server_timer.deleteTimers(); //delete timers now that view has changed
	
	return RCOK;
}

//reset things after a view change
//used when changing view, since nodes need to walk through the steps for committing
// every txn since the last chkpt.
void WorkerThread::reset()
{
	//reset txn managers (or else prepare and other variables will interfere)
	for(uint i = 0; i < 2 * CLIENT_NODE_CNT * MAX_TXN_IN_FLIGHT; i++){
		//use txn_id in pre-prepare message to find and reset txn_mans
		uint64_t id = preStore[i].txn_id;
		if(id != UINT64_MAX) {
			//cout << "resetting txn_man with id " << id << endl;
			TxnManager * local_txn_man = txn_table.get_transaction_manager(get_thd_id(),id,0);
			local_txn_man->prepared = false;
		    #if CONSENSUS == PBFT
			local_txn_man->prep_rsp_cnt = 2 * g_min_invalid_nodes;
			local_txn_man->commit_rsp_cnt = local_txn_man->prep_rsp_cnt + 1;
			local_txn_man->committed_local = false;
		    #endif
			((YCSBQuery*)(local_txn_man->query))->requests.clear();
		}
	}
	
	/*Message * cmsg = Message::create_message(CL_QRY);
	for(uint64_t i=0; i<2 * g_client_node_cnt * g_inflight_max; i++) {
		((YCSBClientQueryMessage *)cmsg)->txn_id = UINT64_MAX;
		cqryStore[i] = *((YCSBClientQueryMessage *)cmsg);
	}*/

	Message * msg = Message::create_message(PBFT_PRE_MSG);
	for(uint64_t i=0; i<2 * g_client_node_cnt * g_inflight_max; i++) {
		preStore[i] = *((PBFTPreMessage *)msg);
	}

    #if CONSENSUS == PBFT
	Message * pmsg = Message::create_message(PBFT_PREP_MSG);
	for(uint64_t i=0; i<2 * g_client_node_cnt * g_inflight_max; i++) {
		for(uint64_t j=0; j<g_node_cnt; j++) {
			((PBFTPrepMessage *)pmsg)->index = UINT64_MAX;
			prepPStore[i][j] = *((PBFTPrepMessage *)pmsg);
		} }

	Message * comsg = Message::create_message(PBFT_COMMIT_MSG);
	for(uint64_t i=0; i<2 * g_client_node_cnt * g_inflight_max; i++) {
		for(uint64_t j=0; j<g_node_cnt; j++) {
			((PBFTCommitMessage *)comsg)->index = UINT64_MAX;
			commStore[i][j] = *((PBFTCommitMessage *)comsg);
		} }

    #endif
	
	#if CONSENSUS == PBFT || CONSENSUS == DBFT
	#if BATCH_ENABLE == BSET
	Message * bmsg = Message::create_message(BATCH_REQ);
	for(uint i = 0; i < indexSize; i++) {
		breqStore[i] = *((BatchRequests *)bmsg);
	}
	#endif
	#endif
	
    #if CONSENSUS == DBFT
	Message * dmsg = Message::create_message(DBFT_PREP_MSG);
	for(uint64_t i=0; i<2 * g_client_node_cnt * g_inflight_max; i++) {
		for(uint64_t j=0; j<g_node_cnt; j++) {
			((DBFTPrepMessage *)dmsg)->index = UINT64_MAX;
			prepStore[i][j] = *((DBFTPrepMessage *)dmsg);
		}

		// Initializing mytriple entries.
		mytriple[i] = "0";
	}

    #endif

}
#endif // view changes

