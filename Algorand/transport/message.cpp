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

#include "mem_alloc.h"
#include "query.h"
#include "ycsb_query.h"
#include "ycsb.h"
//#include "tpcc_query.h"
//#include "tpcc.h"
//#include "pps_query.h"
//#include "pps.h"
#include "global.h"
#include "message.h"
//#include "maat.h"
#include "crypto.h"
#include <fstream>
#include <ctime>
#include "picosha2.h"
#include <string>

std::vector<Message*> * Message::create_messages(char * buf) {
  std::vector<Message*> * all_msgs = new std::vector<Message*>;
  char * data = buf;
	uint64_t ptr = 0;
  uint32_t dest_id;
  uint32_t return_id;
  uint32_t txn_cnt;
  COPY_VAL(dest_id,data,ptr);
  COPY_VAL(return_id,data,ptr);
  COPY_VAL(txn_cnt,data,ptr);
  assert(dest_id == g_node_id);
  assert(return_id != g_node_id);
  assert(ISCLIENTN(return_id) || ISSERVERN(return_id) || ISREPLICAN(return_id));
  while(txn_cnt > 0) {
    Message * msg = create_message(&data[ptr]);
    msg->return_node_id = return_id;
    ptr += msg->get_size();
    all_msgs->push_back(msg);
    --txn_cnt;
  }
  return all_msgs;
}

Message * Message::create_message(char * buf) {
 RemReqType rtype = NO_MSG;
 uint64_t ptr = 0;
 COPY_VAL(rtype,buf,ptr);
 Message * msg = create_message(rtype);
 msg->copy_from_buf(buf);
 return msg;
}

Message * Message::create_message(TxnManager * txn, RemReqType rtype) {
 Message * msg = create_message(rtype);
 msg->mcopy_from_txn(txn);
 msg->copy_from_txn(txn);

 // copy latency here
 msg->lat_work_queue_time = txn->txn_stats.work_queue_time_short;
 msg->lat_msg_queue_time = txn->txn_stats.msg_queue_time_short;
 msg->lat_cc_block_time = txn->txn_stats.cc_block_time_short;
 msg->lat_cc_time = txn->txn_stats.cc_time_short;
 msg->lat_process_time = txn->txn_stats.process_time_short;
 msg->lat_network_time = txn->txn_stats.lat_network_time_start;
 msg->lat_other_time = txn->txn_stats.lat_other_time_start;

 return msg;
}

Message * Message::create_message(BaseQuery * query, RemReqType rtype) {
 assert(rtype == RQRY || rtype == CL_QRY);
 Message * msg = create_message(rtype);
 ((YCSBClientQueryMessage*)msg)->copy_from_query(query);
 return msg;
}

Message * Message::create_message(uint64_t txn_id, RemReqType rtype) {
 Message * msg = create_message(rtype);
 msg->txn_id = txn_id;
 return msg;
}

Message * Message::create_message(uint64_t txn_id, uint64_t batch_id, RemReqType rtype) {
 Message * msg = create_message(rtype);
 msg->txn_id = txn_id;
 msg->batch_id = batch_id;
 return msg;
}

Message * Message::create_message(RemReqType rtype) {
  Message * msg;
  switch(rtype) {
    case INIT_DONE:
      msg = new InitDoneMessage;
      break;
    case KEYEX:
      msg = new KeyExchange;
      break;
    case READY:
      msg = new ReadyServer;
      break;
    case RQRY:
    case RQRY_CONT:
      msg = new YCSBQueryMessage;
      msg->init();
      break;
    case RFIN:
      msg = new FinishMessage;
      break;
    case RQRY_RSP:
      msg = new QueryResponseMessage;
      break;
    case RACK_PREP:
    case RACK_FIN:
      msg = new AckMessage;
      break;
    case CL_QRY:
    case RTXN:
    case RTXN_CONT:
      msg = new YCSBClientQueryMessage;
      msg->init();
      break;
#if CLIENT_BATCH
    case CL_BATCH:
      msg = new ClientQueryBatch;
      break;
#endif
    case RPREPARE:
      msg = new PrepareMessage;
      break;
    case RFWD:
      msg = new ForwardMessage;
      break;
    case RDONE:
      msg = new DoneMessage;
      break;
    case CL_RSP:
      msg = new ClientResponseMessage;
      break;
#if CONSENSUS == PBFT || CONSENSUS == DBFT
#if EXECUTION_THREAD
    case EXECUTE_MSG:
      msg = new ExecuteMessage;
      break;
#endif
    case PBFT_PRE_MSG:
      msg = new PBFTPreMessage;
      break;
#if BATCH_ENABLE == BSET
    case BATCH_REQ:
      msg = new BatchRequests;
      break;

#if RBFT_ON
      case PROP_BATCH:
          msg = new PropagateBatch;
          break;
#endif
#endif

#if LOCAL_FAULT == true && ZYZZYVA == true
    case CL_CC:
      msg = new ClientCommitCertificate;
      break;
    case CL_CAck:
      msg = new ClientCertificateAck;
      break;
#endif

#if VIEW_CHANGES == true
	case PBFT_VIEW_CHANGE:
	  msg = new PBFTViewChangeMsg;
	  break;
	case PBFT_NEW_VIEW:
	  msg = new PBFTNewViewMsg;
	  break;
#endif
#endif

#if CONSENSUS == PBFT
    case PBFT_CHKPT_MSG:
      msg = new CheckpointMessage;
      break;  
    case PBFT_PREP_MSG:
      msg = new PBFTPrepMessage;
      break;
    case PBFT_COMMIT_MSG:
      msg = new PBFTCommitMessage;
      break;
#endif

#if CONSENSUS == DBFT
    case DBFT_PREP_MSG:
      msg = new DBFTPrepMessage;
      break;
    case PP_MSG:
      msg = new PrimaryPrepMessage;
      break;
#endif
    default: 
	cout << "FALSE TYPE: " << rtype << "\n";
	fflush(stdout);
	assert(false);
  }
  assert(msg);
  msg->rtype = rtype;
  msg->txn_id = UINT64_MAX;
  msg->batch_id = UINT64_MAX;
  msg->return_node_id = g_node_id;
  msg->wq_time = 0;
  msg->mq_time = 0;
  msg->ntwk_time = 0;

  msg->lat_work_queue_time = 0;
  msg->lat_msg_queue_time = 0;
  msg->lat_cc_block_time = 0;
  msg->lat_cc_time = 0;
  msg->lat_process_time = 0;
  msg->lat_network_time = 0;
  msg->lat_other_time = 0;

  return msg;
}

uint64_t Message::mget_size() {
  uint64_t size = 0;
  size += sizeof(RemReqType);
  size += sizeof(uint64_t);
#if CC_ALG == CALVIN
  size += sizeof(uint64_t);
#endif
  // for stats, send message queue time
  size += sizeof(uint64_t);
#if CONSENSUS == PBFT || CONSENSUS == DBFT
  size += signature.size();
  size += pubKey.size();
  size += sizeof(sigSize);
  size += sizeof(keySize);

#if ALGORAND == true
  size += ALGORAND_SEED_SIZE;
  size += ALGORAND_HASH_SIZE;
  size += ALGORAND_PROOF_SIZE;
  size += ALGORAND_VALUE_SIZE;
  size += sizeof(dp.j);
  size += sizeof(dp.node_id);
  size += ALGORAND_PK_SIZE;
  size += sizeof(dp.type);
#endif 

#if RBFT_ON
size += sizeof(instance_id);
#endif
#endif

  // for stats, latency
  size += sizeof(uint64_t) * 7;
  return size;
}

void Message::mcopy_from_txn(TxnManager * txn) {
  //rtype = query->rtype;
  txn_id = txn->get_txn_id();
#if CC_ALG == CALVIN
  batch_id = txn->get_batch_id();
#endif

#if RBFT_ON
    instance_id = txn->instance_id;
#endif

//#if CONSENSUS == PBFT || CONSENSUS == DBFT
//  signature = txn->signature;
//  pubKey = txn->pubKey;
//#endif
}

void Message::mcopy_to_txn(TxnManager * txn) {
  txn->return_id = return_node_id;

#if RBFT_ON
    txn->instance_id = instance_id;
#endif

//#if CONSENSUS == PBFT || CONSENSUS == DBFT
//  txn->set_txn_id(this->txn_id);
//
//  txn->signature = signature;
//  txn->pubKey = pubKey;
//#endif
}


void Message::mcopy_from_buf(char * buf) {
  uint64_t ptr = 0;
  COPY_VAL(rtype,buf,ptr);
  COPY_VAL(txn_id,buf,ptr);
  COPY_VAL(mq_time,buf,ptr);

  COPY_VAL(lat_work_queue_time,buf,ptr);
  COPY_VAL(lat_msg_queue_time,buf,ptr);
  COPY_VAL(lat_cc_block_time,buf,ptr);
  COPY_VAL(lat_cc_time,buf,ptr);
  COPY_VAL(lat_process_time,buf,ptr);
  COPY_VAL(lat_network_time,buf,ptr);
  COPY_VAL(lat_other_time,buf,ptr);
  if (IS_LOCAL(txn_id)) {
    lat_network_time = (get_sys_clock() - lat_network_time) - lat_other_time;
  } else {
    lat_other_time = get_sys_clock();
  }
  //printf("buftot %ld: %f, %f\n",txn_id,lat_network_time,lat_other_time);

#if CONSENSUS == PBFT || CONSENSUS == DBFT
	
	COPY_VAL(sigSize,buf,ptr);
	COPY_VAL(keySize,buf,ptr);
	signature.pop_back(); // remove placeholder value for signaure and pubKey
	pubKey.pop_back();
	
	char v;
	for(uint64_t i = 0; i < sigSize; i++){
		COPY_VAL(v, buf, ptr);
		signature += v;
	}
	
	for(uint64_t j = 0; j < keySize; j++){
		COPY_VAL(v, buf, ptr);
		pubKey += v;
	}

#if ALGORAND == true
  for(uint64_t i = 0; i < ALGORAND_SEED_SIZE; i++){
    COPY_VAL(v, buf, ptr);
    dp.seed[i] = v;
  }

  for(uint64_t i = 0; i < ALGORAND_HASH_SIZE; i++){
    COPY_VAL(v, buf, ptr);
    dp.hash[i] = v;
  }
    for(uint64_t i = 0; i < ALGORAND_PROOF_SIZE; i++){
    COPY_VAL(v, buf, ptr);
    dp.proof[i] = v;
  }

  for(uint64_t i = 0; i < ALGORAND_VALUE_SIZE; i++){
    COPY_VAL(v, buf, ptr);
    dp.value[i] = v;
  }

  COPY_VAL(dp.j,buf,ptr);
  COPY_VAL(dp.node_id,buf,ptr);
  for(uint64_t i = 0; i < ALGORAND_PK_SIZE; i++){
    COPY_VAL(v, buf, ptr);
    dp.pk[i] = v;
  }
  COPY_VAL(dp.type,buf,ptr);
#endif

  #if RBFT_ON
    	COPY_VAL(instance_id, buf, ptr);
  #endif
#endif
}

void Message::mcopy_to_buf(char * buf) {
  uint64_t ptr = 0;
  COPY_BUF(buf,rtype,ptr);
  COPY_BUF(buf,txn_id,ptr);
#if CC_ALG == CALVIN
  COPY_BUF(buf,batch_id,ptr);
#endif
  COPY_BUF(buf,mq_time,ptr);

  COPY_BUF(buf,lat_work_queue_time,ptr);
  COPY_BUF(buf,lat_msg_queue_time,ptr);
  COPY_BUF(buf,lat_cc_block_time,ptr);
  COPY_BUF(buf,lat_cc_time,ptr);
  COPY_BUF(buf,lat_process_time,ptr);
  if ((CC_ALG == CALVIN && rtype == CL_QRY && txn_id % g_node_cnt == g_node_id) || (CC_ALG != CALVIN && IS_LOCAL(txn_id))) {
    lat_network_time = get_sys_clock();
  } else {
    lat_other_time = get_sys_clock() - lat_other_time;
  }
  //printf("mtobuf %ld: %f, %f\n",txn_id,lat_network_time,lat_other_time);
  COPY_BUF(buf,lat_network_time,ptr);
  COPY_BUF(buf,lat_other_time,ptr);

#if CONSENSUS == PBFT || CONSENSUS == DBFT
	COPY_BUF(buf,sigSize,ptr);
	COPY_BUF(buf,keySize,ptr);
	
	//COPY_BUF(buf,signature,ptr);
	char v;
	for(uint64_t i = 0; i < sigSize; i++){
		v = signature[i];
		COPY_BUF(buf, v, ptr);
	}
	//COPY_BUF(buf,pubKey,ptr);
	for(uint64_t j = 0; j < keySize; j++){
		v = pubKey[j];
		COPY_BUF(buf, v, ptr);
	}

#if ALGORAND == true
  //copy to buffer
  for(uint64_t i = 0; i < ALGORAND_SEED_SIZE; i++){
    if(i < dp.seed.size()){
      v = dp.seed[i];
    }
    COPY_BUF(buf, v, ptr);
  }
  for(uint64_t i = 0; i < ALGORAND_HASH_SIZE; i++){
    if(i < dp.hash.size()){
      v = dp.hash[i];
    }
    COPY_BUF(buf, v, ptr);
  }
  for(uint64_t i = 0; i < ALGORAND_PROOF_SIZE; i++){
    if(i < dp.proof.size()){
      v = dp.proof[i];
    }
    COPY_BUF(buf, v, ptr);
  }
  for(uint64_t i = 0; i < ALGORAND_VALUE_SIZE; i++){
    if(i < dp.value.size()){
      v = dp.value[i];
    }
    COPY_BUF(buf, v, ptr);
  }
  COPY_BUF(buf,dp.j,ptr);
  COPY_BUF(buf,dp.node_id,ptr);
  for(uint64_t i = 0; i < ALGORAND_PK_SIZE; i++){
    if(i < dp.pk.size()){
      v = dp.pk[i];
    }
    COPY_BUF(buf, v, ptr);
  } 
  COPY_BUF(buf,dp.type,ptr);
#endif

#if RBFT_ON
    	COPY_BUF(buf, instance_id, ptr);
#endif
	
#endif
}

void Message::release_message(Message * msg) {
  switch(msg->rtype) {
    case INIT_DONE: {
      InitDoneMessage * m_msg = (InitDoneMessage*)msg;
      m_msg->release();
      delete m_msg;
      break;
                    }
    case KEYEX: {
      KeyExchange * m_msg = (KeyExchange*)msg;
      m_msg->release();
      delete m_msg;
      break;
                    }
    case READY: {
      ReadyServer* m_msg = (ReadyServer*)msg;
      m_msg->release();
      delete m_msg;
      break;
                    }
    case RQRY:
    case RQRY_CONT: {
      YCSBQueryMessage * m_msg = (YCSBQueryMessage*)msg;
      m_msg->release();
      delete m_msg;
      break;
                    }
    case RFIN: {
      FinishMessage * m_msg = (FinishMessage*)msg;
      m_msg->release();
      delete m_msg;
      break;
               }
    case RQRY_RSP: {
      QueryResponseMessage * m_msg = (QueryResponseMessage*)msg;
      m_msg->release();
      delete m_msg;
      break;
                   }
    case RACK_PREP:
    case RACK_FIN: {
      AckMessage * m_msg = (AckMessage*)msg;
      m_msg->release();
      delete m_msg;
      break;
                   }
    case CL_QRY:
    case RTXN:
    case RTXN_CONT: {
      YCSBClientQueryMessage * m_msg = (YCSBClientQueryMessage*)msg;
      m_msg->release();
      delete m_msg;
      break;
                    }
#if CLIENT_BATCH
    case CL_BATCH: {
      ClientQueryBatch * m_msg = (ClientQueryBatch*)msg;
      m_msg->release();
      delete m_msg;
      break;
                   }
#endif
    case RPREPARE: {
      PrepareMessage * m_msg = (PrepareMessage*)msg;
      m_msg->release();
      delete m_msg;
      break;
                   }
    case RFWD: {
      ForwardMessage * m_msg = (ForwardMessage*)msg;
      m_msg->release();
      delete m_msg;
      break;
               }
    case RDONE: {
      DoneMessage * m_msg = (DoneMessage*)msg;
      m_msg->release();
      delete m_msg;
      break;
                }
    case CL_RSP: {
      ClientResponseMessage * m_msg = (ClientResponseMessage*)msg;
      m_msg->release();
      delete m_msg;
      break;
                 }
#if CONSENSUS == PBFT || CONSENSUS == DBFT
#if EXECUTION_THREAD
    case EXECUTE_MSG: {
      ExecuteMessage * m_msg = (ExecuteMessage*)msg;
      m_msg->release();
      delete m_msg;
      break;
                  }
#endif
    case PBFT_PRE_MSG: {
      PBFTPreMessage * m_msg = (PBFTPreMessage*)msg;
      m_msg->release();
      delete m_msg;
      break;
                  }
#if BATCH_ENABLE == BSET
    case BATCH_REQ: {
      BatchRequests * m_msg = (BatchRequests*)msg;
      m_msg->release();
      delete m_msg;
      break;
                  }
#endif

#if LOCAL_FAULT == true && ZYZZYVA == true
    case CL_CC: {
      ClientCommitCertificate * m_msg = (ClientCommitCertificate*)msg;
      m_msg->release();
      delete m_msg;
      break;
                 }
    case CL_CAck: {
      ClientCertificateAck * m_msg = (ClientCertificateAck*)msg;
      m_msg->release();
      delete m_msg;
      break;
                 }
#endif

#if VIEW_CHANGES == true
    case PBFT_VIEW_CHANGE: {
      PBFTViewChangeMsg * m_msg = (PBFTViewChangeMsg*)msg;
      m_msg->release();
      delete m_msg;
      break;
                      }
    case PBFT_NEW_VIEW: {
      PBFTNewViewMsg * m_msg = (PBFTNewViewMsg*)msg;
      m_msg->release();
      delete m_msg;
      break;
                      }
#endif
#endif

#if CONSENSUS == PBFT
    case PBFT_CHKPT_MSG: {
      CheckpointMessage * m_msg = (CheckpointMessage*)msg;
      m_msg->release();
      delete m_msg;
      break;
                      }
    case PBFT_PREP_MSG: {
      PBFTPrepMessage * m_msg = (PBFTPrepMessage*)msg;
      m_msg->release();
      delete m_msg;
      break;
                      }
    case PBFT_COMMIT_MSG: {
      PBFTCommitMessage * m_msg = (PBFTCommitMessage*)msg;
      m_msg->release();
      delete m_msg;
      break;
                      }
#endif

#if RBFT_ON
      case PROP_BATCH: {
         PropagateBatch * m_msg = (PropagateBatch*)msg;
         m_msg->release();
         delete m_msg;
         break;
      }
#endif

#if CONSENSUS == DBFT
    case DBFT_PREP_MSG: {
      DBFTPrepMessage * m_msg = (DBFTPrepMessage*)msg;
      m_msg->release();
      delete m_msg;
      break;
		     }
    case PP_MSG: {
      PrimaryPrepMessage * m_msg = (PrimaryPrepMessage*)msg;
      m_msg->release();
      delete m_msg;
      break;
		     }
#endif
    default: { assert(false); }
  }
}
/************************/

uint64_t QueryMessage::get_size() {
  uint64_t size = Message::mget_size();
#if CC_ALG == WAIT_DIE || CC_ALG == TIMESTAMP || CC_ALG == MVCC
  size += sizeof(ts);
#endif
#if CC_ALG == OCC 
  size += sizeof(start_ts);
#endif  
  return size;
}

void QueryMessage::copy_from_txn(TxnManager * txn) {
  Message::mcopy_from_txn(txn);
#if CC_ALG == WAIT_DIE || CC_ALG == TIMESTAMP || CC_ALG == MVCC
  ts = txn->get_timestamp();
  assert(ts != 0);
#endif
#if CC_ALG == OCC 
  start_ts = txn->get_start_timestamp();
#endif
}

void QueryMessage::copy_to_txn(TxnManager * txn) {
  Message::mcopy_to_txn(txn);
//#if CC_ALG == WAIT_DIE || CC_ALG == TIMESTAMP || CC_ALG == MVCC
//  assert(ts != 0);
//  txn->set_timestamp(ts);
//#endif
//#if CC_ALG == OCC 
//  txn->set_start_timestamp(start_ts);
//#endif

}

void QueryMessage::copy_from_buf(char * buf) {
  Message::mcopy_from_buf(buf);
  uint64_t ptr __attribute__ ((unused));
  ptr = Message::mget_size();
#if CC_ALG == WAIT_DIE || CC_ALG == TIMESTAMP || CC_ALG == MVCC
 COPY_VAL(ts,buf,ptr);
  assert(ts != 0);
#endif
#if CC_ALG == OCC 
 COPY_VAL(start_ts,buf,ptr);
#endif
}

void QueryMessage::copy_to_buf(char * buf) {
  Message::mcopy_to_buf(buf);
  uint64_t ptr __attribute__ ((unused));
  ptr = Message::mget_size();
#if CC_ALG == WAIT_DIE || CC_ALG == TIMESTAMP || CC_ALG == MVCC
 COPY_BUF(buf,ts,ptr);
  assert(ts != 0);
#endif
#if CC_ALG == OCC 
 COPY_BUF(buf,start_ts,ptr);
#endif
}

/************************/

void YCSBClientQueryMessage::init() {
}

void YCSBClientQueryMessage::release() {
  ClientQueryMessage::release();
  // Freeing requests is the responsibility of txn at commit time
/*
  for(uint64_t i = 0; i < requests.size(); i++) {
    DEBUG_M("YCSBClientQueryMessage::release ycsb_request free\n");
    mem_allocator.free(requests[i],sizeof(ycsb_request));
  }
*/
  requests.release();
}

uint64_t YCSBClientQueryMessage::get_size() {
  uint64_t size = ClientQueryMessage::get_size();
  size += sizeof(size_t);
  size += sizeof(ycsb_request) * requests.size();

#if CONSENSUS == PBFT || CONSENSUS == DBFT
  size += sizeof(return_node);
#endif

  return size;
}

void YCSBClientQueryMessage::copy_from_query(BaseQuery * query) {
  ClientQueryMessage::copy_from_query(query);
/*
  requests.init(g_req_per_query);
  for(uint64_t i = 0; i < ((YCSBQuery*)(query))->requests.size(); i++) {
      YCSBQuery::copy_request_to_msg(((YCSBQuery*)(query)),this,i);
  }
*/
  requests.copy(((YCSBQuery*)(query))->requests);
}


void YCSBClientQueryMessage::copy_from_txn(TxnManager * txn) {
  ClientQueryMessage::mcopy_from_txn(txn);
/*
  requests.init(g_req_per_query);
  for(uint64_t i = 0; i < ((YCSBQuery*)(txn->query))->requests.size(); i++) {
      YCSBQuery::copy_request_to_msg(((YCSBQuery*)(txn->query)),this,i);
  }
*/
  requests.copy(((YCSBQuery*)(txn->query))->requests);
}

void YCSBClientQueryMessage::copy_to_txn(TxnManager * txn) {
  // this only copies over the pointers, so if requests are freed, we'll lose the request data
  ClientQueryMessage::copy_to_txn(txn);

#if CONSENSUS == PBFT || CONSENSUS == DBFT
  txn->client_id = return_node;
#endif
  // Copies pointers to txn
  ((YCSBQuery*)(txn->query))->requests.append(requests);
/*
  for(uint64_t i = 0; i < requests.size(); i++) {
      YCSBQuery::copy_request_to_qry(((YCSBQuery*)(txn->query)),this,i);
  }
*/
}

void YCSBClientQueryMessage::copy_from_buf(char * buf) {
  ClientQueryMessage::copy_from_buf(buf);
  uint64_t ptr = ClientQueryMessage::get_size();
  size_t size;
  //DEBUG("1YCSBClientQuery %ld\n",ptr);
  COPY_VAL(size,buf,ptr);
  requests.init(size);
  //DEBUG("2YCSBClientQuery %ld\n",ptr);
  for(uint64_t i = 0 ; i < size;i++) {
    DEBUG_M("YCSBClientQueryMessage::copy ycsb_request alloc\n");
    ycsb_request * req = (ycsb_request*)mem_allocator.alloc(sizeof(ycsb_request));
    COPY_VAL(*req,buf,ptr);
    //DEBUG("3YCSBClientQuery %ld\n",ptr);
    assert(req->key < g_synth_table_size);
    requests.add(req);
  }

#if CONSENSUS == PBFT || CONSENSUS == DBFT
    COPY_VAL(return_node,buf,ptr);
#endif

 assert(ptr == get_size());
}

void YCSBClientQueryMessage::copy_to_buf(char * buf) {
  ClientQueryMessage::copy_to_buf(buf);
  uint64_t ptr = ClientQueryMessage::get_size();
  //DEBUG("1YCSBClientQuery %ld\n",ptr);
  size_t size = requests.size();
  COPY_BUF(buf,size,ptr);
  //DEBUG("2YCSBClientQuery %ld\n",ptr);
  for(uint64_t i = 0; i < requests.size(); i++) {
    ycsb_request * req = requests[i];

    //cout << "Req key: " << req->key << "  :: synth_table: " << g_synth_table_size << "\n";
    //fflush(stdout);

    assert(req->key < g_synth_table_size);
    COPY_BUF(buf,*req,ptr);
    //DEBUG("3YCSBClientQuery %ld\n",ptr);
  }

#if CONSENSUS == PBFT || CONSENSUS == DBFT
  COPY_BUF(buf,return_node,ptr);
#endif

 assert(ptr == get_size());
}


//returns a string representation of the requests in this message
string YCSBClientQueryMessage::getRequestString()
{
	string message;
	for(uint64_t i = 0; i < requests.size(); i++) {
		message += std::to_string(requests[i]->key);
		message += " ";
		message += requests[i]->value;
		message += " ";
	}
	
	return message;
}

//returns the string that needs to be signed/verified for this message
string YCSBClientQueryMessage::getString()
{
	string message = this->getRequestString();
	message += " ";
	message += to_string(this->client_startts);
	
	
	return message;
}


/************************/

void ClientQueryMessage::init() {
    first_startts = 0;
}

void ClientQueryMessage::release() {
  partitions.release();
  first_startts = 0;
}

uint64_t ClientQueryMessage::get_size() {
  uint64_t size = Message::mget_size();
  size += sizeof(client_startts);
  /*
  uint64_t size = sizeof(ClientQueryMessage);
  */
  size += sizeof(size_t);
  size += sizeof(uint64_t) * partitions.size();
  return size;
}

void ClientQueryMessage::copy_from_query(BaseQuery * query) {
  partitions.clear();
}

void ClientQueryMessage::copy_from_txn(TxnManager * txn) {
  Message::mcopy_from_txn(txn);
  //ts = txn->txn->timestamp;
  partitions.clear();
  client_startts = txn->client_startts;
}

void ClientQueryMessage::copy_to_txn(TxnManager * txn) {
  Message::mcopy_to_txn(txn);
  //txn->txn->timestamp = ts;
  txn->client_startts = client_startts;
  txn->client_id = return_node_id;
}

void ClientQueryMessage::copy_from_buf(char * buf) {
  Message::mcopy_from_buf(buf);
  uint64_t ptr = Message::mget_size();
  //COPY_VAL(ts,buf,ptr);
  COPY_VAL(client_startts,buf,ptr);
  size_t size;
  COPY_VAL(size,buf,ptr);
  partitions.init(size);
  for(uint64_t i = 0; i < size; i++) {
    //COPY_VAL(partitions[i],buf,ptr);
    uint64_t part;
    COPY_VAL(part,buf,ptr);
    partitions.add(part);
  }
}

void ClientQueryMessage::copy_to_buf(char * buf) {
  Message::mcopy_to_buf(buf);
  uint64_t ptr = Message::mget_size();
  //COPY_BUF(buf,ts,ptr);
  COPY_BUF(buf,client_startts,ptr);
  size_t size = partitions.size();
  COPY_BUF(buf,size,ptr);
  for(uint64_t i = 0; i < size; i++) {
    uint64_t part = partitions[i];
    COPY_BUF(buf,part,ptr);
  }
}

/************************/


uint64_t ClientResponseMessage::get_size() {
  uint64_t size = Message::mget_size();
  size += sizeof(uint64_t);

#if CLIENT_RESPONSE_BATCH == true
  size += sizeof(uint64_t) * index.size();
  size += sizeof(uint64_t) * client_ts.size();
#else
  size += sizeof(uint64_t);
#endif

#if ZYZZYVA == true
  size += clreq.get_size();
  size += sizeof(uint64_t);
  size += stateHashSize;
#endif

  return size;
}

void ClientResponseMessage::release() {
  #if CLIENT_RESPONSE_BATCH == true
	index.release();
	client_ts.release();
  #endif 

}


void ClientResponseMessage::copy_from_txn(TxnManager * txn) {
  Message::mcopy_from_txn(txn);

#if !CLIENT_RESPONSE_BATCH 
    client_startts = txn->client_startts;
#endif

#if CONSENSUS == PBFT || CONSENSUS == DBFT
  view = g_view;
#endif
}

void ClientResponseMessage::copy_to_txn(TxnManager * txn) {
  Message::mcopy_to_txn(txn);

#if !CLIENT_RESPONSE_BATCH
  txn->client_startts = client_startts;
#endif
}

void ClientResponseMessage::copy_from_buf(char * buf) {
  Message::mcopy_from_buf(buf);
  uint64_t ptr = Message::mget_size();

#if CLIENT_RESPONSE_BATCH == true
  index.init(g_batch_size);
  uint64_t tval;
  for(uint64_t i=0; i<g_batch_size; i++) {
	COPY_VAL(tval,buf,ptr);
	index.add(tval);
	//index[i] = tval;
  }

  client_ts.init(g_batch_size);
  for(uint64_t i=0; i<g_batch_size; i++) {
	COPY_VAL(tval,buf,ptr);
	client_ts.add(tval);
	//client_ts[i] = tval;
  }
#else
  COPY_VAL(client_startts,buf,ptr);
#endif

#if CONSENSUS == PBFT || CONSENSUS == DBFT
  COPY_VAL(view,buf,ptr);
#endif

#if ZYZZYVA == true
  Message * msg = create_message(&buf[ptr]);
  ptr += msg->get_size();
  clreq = *((YCSBClientQueryMessage*)msg);

  COPY_VAL(stateHashSize,buf,ptr);
  ptr = buf_to_string(buf, ptr, stateHash, stateHashSize);
#endif

 assert(ptr == get_size());
}

void ClientResponseMessage::copy_to_buf(char * buf) {
  Message::mcopy_to_buf(buf);
  uint64_t ptr = Message::mget_size();

#if CLIENT_RESPONSE_BATCH == true
  uint64_t tval;
  for(uint64_t i=0; i<g_batch_size; i++) {
	tval = index[i];
	COPY_BUF(buf,tval,ptr);
  }

  for(uint64_t i=0; i<g_batch_size; i++) {
	tval = client_ts[i];
	COPY_BUF(buf,tval,ptr);
  }
#else
  COPY_BUF(buf,client_startts,ptr);
#endif

#if CONSENSUS == PBFT || CONSENSUS == DBFT
  COPY_BUF(buf,view,ptr);
#endif

#if ZYZZYVA == true
  clreq.copy_to_buf(&buf[ptr]);
  ptr += clreq.get_size();

  COPY_BUF(buf,stateHashSize,ptr);

  char v;
  for(uint j = 0; j < stateHashSize; j++) {
  	v = stateHash[j];
  	COPY_BUF(buf, v, ptr);
  }
#endif
 assert(ptr == get_size());
}

void ClientResponseMessage::sign()
{
#if USE_CRYPTO
	string message = std::to_string(g_view) + '_' + std::to_string(g_node_id);

  #if ZYZZYVA == true
	message += '_' + stateHash;
  #endif
	
	auto signature(RsaSignString(g_priv_key, message));
	this->signature = signature;
#else
	this->signature = "0"; 
#endif

	this->pubKey = g_pub_keys[g_node_id];
	
	this->sigSize = this->signature.size();
	this->keySize = this->pubKey.size();
}


//validate message 
bool ClientResponseMessage::validate()
{
//TODO: Switch this on
/*
#if USE_CRYPTO
	//is signature valid
	string message = std::to_string(this->view) + '_' + std::to_string(this->dest) + '_' + std::to_string(this->client_startts) + '_' + std::to_string(this->return_node_id);

	if (! RsaVerifyString(this->pubKey, message, this->signature)) {
		assert(0);
		return false;
	}
	
	// was this response message sent by the node it claims to be
	if(this->pubKey != g_pub_keys[this->return_node_id]) 
	{
		assert(0);
		return false;
	}
#endif
*/
	
#if CONSENSUS == PBFT || CONSENSUS == DBFT
	//count number of accepted response messages for this transaction
//	cout << "IN: " << this->txn_id << " :: sss: " << this->client_startts << "\n";
//	fflush(stdout);

	int k = 0;
	uint64_t relIndex = this->txn_id % indexSize;
	ClientResponseMessage clrsp;
	for(uint64_t i=0; i<g_node_cnt; i++) {
		clrsp = clrspStore[relIndex][i];
	//	cout << "TXN: " << clrsp.txn_id << " :: stts: " << clrsp.client_startts << " :: Ot: " << this->client_startts << "\n";
	//	fflush(stdout);

		if(clrsp.txn_id == this->txn_id) {
			k++;

		   #if CLIENT_RESPONSE_BATCH == true
			for(uint64_t j=0; j<g_batch_size; j++) {
			   assert(this->index[j] == clrsp.index[j]);		 	
			   assert(this->client_ts[j] == clrsp.client_ts[j]);  
			}
		   #else
			assert(this->client_startts == clrsp.client_startts);
		   #endif
		} }

	//add valid message to message log
	clrspStore[relIndex][this->return_node_id] = *this;

	k++;// count message just added

#if ZYZZYVA == true
	if (k < 3 * g_min_invalid_nodes + 1)
#else
	if (k < 2 * g_min_invalid_nodes + 1)
#endif
	{  return false;  }

  	// If true, reset the entries to prevent client sending further response.
  	for(uint64_t i=0; i<g_node_cnt; i++) {
  	     clrsp = clrspStore[relIndex][i];
  	     clrsp.txn_id = UINT64_MAX;
  	     clrspStore[relIndex][i] = clrsp;
  	}
#endif

   return true;
}


/************************************/

#if LOCAL_FAULT == true && ZYZZYVA == true

uint64_t ClientCommitCertificate::get_size() {
  uint64_t size = Message::mget_size();
  size += sizeof(uint64_t);
  size += sizeof(uint64_t);

  return size;
}

void ClientCommitCertificate::copy_from_txn(TxnManager * txn) {
  // Do nothing
}

void ClientCommitCertificate::copy_to_txn(TxnManager * txn) {
  assert(0);
}

void ClientCommitCertificate::copy_from_buf(char * buf) {
  Message::mcopy_from_buf(buf);
  uint64_t ptr = Message::mget_size();
  COPY_VAL(client_startts,buf,ptr);
  COPY_VAL(view,buf,ptr);
  assert(ptr == get_size());
}

void ClientCommitCertificate::copy_to_buf(char * buf) {
  Message::mcopy_to_buf(buf);
  uint64_t ptr = Message::mget_size();
  COPY_BUF(buf,client_startts,ptr);
  COPY_BUF(buf,view,ptr);
  assert(ptr == get_size());
}

void ClientCommitCertificate::sign()
{
#if USE_CRYPTO
	string message = std::to_string(g_view) + '_' + std::to_string(this->client_startts) + '_' + std::to_string(g_node_id); 
	
	auto signature(RsaSignString(g_priv_key, message));
	this->signature = signature;
#else
	this->signature = "0"; 
#endif

	this->pubKey = g_pub_keys[g_node_id];
	
	this->sigSize = this->signature.size();
	this->keySize = this->pubKey.size();
}


//validate message 
bool ClientCommitCertificate::validate()
{
	//cout << "Here: " << this->view << "\n";
	//fflush(stdout);
#if USE_CRYPTO
	//is signature valid
	string message = std::to_string(this->view) + '_' + std::to_string(this->client_startts) + '_' + std::to_string(this->return_node_id);

	if (! RsaVerifyString(this->pubKey, message, this->signature)) {
		assert(0);
		return false;
	}
	
	if(this->pubKey != g_pub_keys[this->return_node_id]) {
		assert(0);
		return false;
	}
#endif
	
   return true;
}


/******************************/


uint64_t ClientCertificateAck::get_size() {
  uint64_t size = Message::mget_size();
  size += sizeof(uint64_t);
  size += sizeof(uint64_t);

  return size;
}

void ClientCertificateAck::copy_from_txn(TxnManager * txn) {
  // Do nothing
}

void ClientCertificateAck::copy_to_txn(TxnManager * txn) {
  assert(0);
}

void ClientCertificateAck::copy_from_buf(char * buf) {
  Message::mcopy_from_buf(buf);
  uint64_t ptr = Message::mget_size();
  COPY_VAL(view,buf,ptr);
  COPY_VAL(client_startts,buf,ptr);

  assert(ptr == get_size());
}

void ClientCertificateAck::copy_to_buf(char * buf) {
  Message::mcopy_to_buf(buf);
  uint64_t ptr = Message::mget_size();
  COPY_BUF(buf,view,ptr);
  COPY_BUF(buf,client_startts,ptr);

  assert(ptr == get_size());
}

void ClientCertificateAck::sign()
{
#if USE_CRYPTO
	string message = std::to_string(g_view);
	
	auto signature(RsaSignString(g_priv_key, message));
	this->signature = signature;
#else
	this->signature = "0"; 
#endif

	this->pubKey = g_pub_keys[g_node_id];
	
	this->sigSize = this->signature.size();
	this->keySize = this->pubKey.size();
}


//validate message 
bool ClientCertificateAck::validate()
{
#if USE_CRYPTO
	//is signature valid
	string message = std::to_string(this->view);

	if (! RsaVerifyString(this->pubKey, message, this->signature)) {
		assert(0);
		return false;
	}
	
	if(this->pubKey != g_pub_keys[this->return_node_id]) {
		assert(0);
		return false;
	}
#endif

	int k = 0;
	uint64_t relIndex = this->txn_id % indexSize;
	ClientCertificateAck clack;
	for(uint64_t i=0; i<g_node_cnt; i++) {
		clack = clackStore[relIndex][i];
	//	cout << "TXN: " << clack.txn_id << "\n";
	//	fflush(stdout);

		if(clack.txn_id == this->txn_id) {
			k++;
		} }

	//add valid message to message log
	clackStore[relIndex][this->return_node_id] = *this;

	k++;// count message just added

	if (k < 2 * g_min_invalid_nodes + 1)
	{  return false;  }

  	//If true reset the entries to prevent client sending further response.
  	for(uint64_t i=0; i<g_node_cnt; i++) {
  	     clack = clackStore[relIndex][i];
  	     clack.txn_id = UINT64_MAX;
  	     clackStore[relIndex][i] = clack;
  	}
	
   return true;
}


#endif


/************************/


uint64_t DoneMessage::get_size() {
  uint64_t size = Message::mget_size();
  return size;
}

void DoneMessage::copy_from_txn(TxnManager * txn) {
  Message::mcopy_from_txn(txn);
}

void DoneMessage::copy_to_txn(TxnManager * txn) {
  Message::mcopy_to_txn(txn);
}

void DoneMessage::copy_from_buf(char * buf) {
  Message::mcopy_from_buf(buf);
  uint64_t ptr = Message::mget_size();
 assert(ptr == get_size());
}

void DoneMessage::copy_to_buf(char * buf) {
  Message::mcopy_to_buf(buf);
  uint64_t ptr = Message::mget_size();
 assert(ptr == get_size());
}

/************************/


uint64_t ForwardMessage::get_size() {
  uint64_t size = Message::mget_size();
  size += sizeof(RC);
#if WORKLOAD == TPCC
	size += sizeof(uint64_t);
#endif
  return size;
}

void ForwardMessage::copy_from_txn(TxnManager * txn) {
  Message::mcopy_from_txn(txn);
  rc = txn->get_rc();
#if WORKLOAD == TPCC
  o_id = ((TPCCQuery*)txn->query)->o_id;
#endif
}

void ForwardMessage::copy_to_txn(TxnManager * txn) {
  // Don't copy return ID
  //Message::mcopy_to_txn(txn);
#if WORKLOAD == TPCC
  ((TPCCQuery*)txn->query)->o_id = o_id;
#endif
}

void ForwardMessage::copy_from_buf(char * buf) {
  Message::mcopy_from_buf(buf);
  uint64_t ptr = Message::mget_size();
  COPY_VAL(rc,buf,ptr);
#if WORKLOAD == TPCC
  COPY_VAL(o_id,buf,ptr);
#endif
 assert(ptr == get_size());
}

void ForwardMessage::copy_to_buf(char * buf) {
  Message::mcopy_to_buf(buf);
  uint64_t ptr = Message::mget_size();
  COPY_BUF(buf,rc,ptr);
#if WORKLOAD == TPCC
  COPY_BUF(buf,o_id,ptr);
#endif
 assert(ptr == get_size());
}

/************************/

uint64_t PrepareMessage::get_size() {
  uint64_t size = Message::mget_size();
  //size += sizeof(uint64_t);
  return size;
}

void PrepareMessage::copy_from_txn(TxnManager * txn) {
  Message::mcopy_from_txn(txn);
}

void PrepareMessage::copy_to_txn(TxnManager * txn) {
  Message::mcopy_to_txn(txn);
}

void PrepareMessage::copy_from_buf(char * buf) {
  Message::mcopy_from_buf(buf);
  uint64_t ptr = Message::mget_size();
 assert(ptr == get_size());
}

void PrepareMessage::copy_to_buf(char * buf) {
  Message::mcopy_to_buf(buf);
  uint64_t ptr = Message::mget_size();
 assert(ptr == get_size());
}

/************************/

uint64_t AckMessage::get_size() {
  uint64_t size = Message::mget_size();
  size += sizeof(RC);
#if CC_ALG == MAAT
  size += sizeof(uint64_t) * 2;
#endif
#if WORKLOAD == PPS && CC_ALG == CALVIN
  size += sizeof(size_t);
  size += sizeof(uint64_t) * part_keys.size();
#endif
  return size;
}

void AckMessage::copy_from_txn(TxnManager * txn) {
  Message::mcopy_from_txn(txn);
  //rc = query->rc;
  rc = txn->get_rc();
#if CC_ALG == MAAT
  lower = time_table.get_lower(txn->get_thd_id(),txn->get_txn_id());
  upper = time_table.get_upper(txn->get_thd_id(),txn->get_txn_id());
#endif
#if WORKLOAD == PPS && CC_ALG == CALVIN
  PPSQuery* pps_query = (PPSQuery*)(txn->query);
  part_keys.copy(pps_query->part_keys);
#endif
}

void AckMessage::copy_to_txn(TxnManager * txn) {
  Message::mcopy_to_txn(txn);
  //query->rc = rc;
#if WORKLOAD == PPS && CC_ALG == CALVIN

  PPSQuery* pps_query = (PPSQuery*)(txn->query);
  pps_query->part_keys.append(part_keys);
#endif
}

void AckMessage::copy_from_buf(char * buf) {
  Message::mcopy_from_buf(buf);
  uint64_t ptr = Message::mget_size();
  COPY_VAL(rc,buf,ptr);
#if CC_ALG == MAAT
  COPY_VAL(lower,buf,ptr);
  COPY_VAL(upper,buf,ptr);
#endif
#if WORKLOAD == PPS && CC_ALG == CALVIN

  size_t size;
  COPY_VAL(size,buf,ptr);
  part_keys.init(size);
  for(uint64_t i = 0 ; i < size;i++) {
    uint64_t item;
    COPY_VAL(item,buf,ptr);
    part_keys.add(item);
  }
#endif
 assert(ptr == get_size());
}

void AckMessage::copy_to_buf(char * buf) {
  Message::mcopy_to_buf(buf);
  uint64_t ptr = Message::mget_size();
  COPY_BUF(buf,rc,ptr);
#if CC_ALG == MAAT
  COPY_BUF(buf,lower,ptr);
  COPY_BUF(buf,upper,ptr);
#endif
#if WORKLOAD == PPS && CC_ALG == CALVIN

  size_t size = part_keys.size();
  COPY_BUF(buf,size,ptr);
  for(uint64_t i = 0; i < part_keys.size(); i++) {
    uint64_t item = part_keys[i];
    COPY_BUF(buf,item,ptr);
  }
#endif
 assert(ptr == get_size());
}

/************************/

uint64_t QueryResponseMessage::get_size() {
  uint64_t size = Message::mget_size(); 
  size += sizeof(RC);
  //size += sizeof(uint64_t);
  return size;
}

void QueryResponseMessage::copy_from_txn(TxnManager * txn) {
  Message::mcopy_from_txn(txn);
  rc = txn->get_rc();

}

void QueryResponseMessage::copy_to_txn(TxnManager * txn) {
  Message::mcopy_to_txn(txn);
  //query->rc = rc;

}

void QueryResponseMessage::copy_from_buf(char * buf) {
  Message::mcopy_from_buf(buf);
  uint64_t ptr = Message::mget_size();
  COPY_VAL(rc,buf,ptr);

 assert(ptr == get_size());
}

void QueryResponseMessage::copy_to_buf(char * buf) {
  Message::mcopy_to_buf(buf);
  uint64_t ptr = Message::mget_size();
  COPY_BUF(buf,rc,ptr);
 assert(ptr == get_size());
}

/************************/



uint64_t FinishMessage::get_size() {
  uint64_t size = Message::mget_size();
  size += sizeof(uint64_t); 
  size += sizeof(RC); 
  size += sizeof(bool); 
#if CC_ALG == MAAT
  size += sizeof(uint64_t); 
#endif
  return size;
}

void FinishMessage::copy_from_txn(TxnManager * txn) {
  Message::mcopy_from_txn(txn);
  rc = txn->get_rc();
#if CC_ALG == MAAT
  commit_timestamp = txn->get_commit_timestamp();
#endif
}

void FinishMessage::copy_to_txn(TxnManager * txn) {
  Message::mcopy_to_txn(txn);
#if CC_ALG == MAAT
  txn->commit_timestamp = commit_timestamp;
#endif
}

void FinishMessage::copy_from_buf(char * buf) {
  Message::mcopy_from_buf(buf);
  uint64_t ptr = Message::mget_size();
  COPY_VAL(pid,buf,ptr);
  COPY_VAL(rc,buf,ptr);
  COPY_VAL(readonly,buf,ptr);
#if CC_ALG == MAAT
  COPY_VAL(commit_timestamp,buf,ptr);
#endif
 assert(ptr == get_size());
}

void FinishMessage::copy_to_buf(char * buf) {
  Message::mcopy_to_buf(buf);
  uint64_t ptr = Message::mget_size();
  COPY_BUF(buf,pid,ptr);
  COPY_BUF(buf,rc,ptr);
  COPY_BUF(buf,readonly,ptr);
#if CC_ALG == MAAT
  COPY_BUF(buf,commit_timestamp,ptr);
#endif
 assert(ptr == get_size());
}


/************************/

uint64_t InitDoneMessage::get_size() {
  uint64_t size = Message::mget_size();
  return size;
}

void InitDoneMessage::copy_from_txn(TxnManager * txn) {
}

void InitDoneMessage::copy_to_txn(TxnManager * txn) {
  Message::mcopy_to_txn(txn);
}

void InitDoneMessage::copy_from_buf(char * buf) {
  Message::mcopy_from_buf(buf);
}

void InitDoneMessage::copy_to_buf(char * buf) {
  Message::mcopy_to_buf(buf);
}

/************************/

uint64_t ReadyServer::get_size() {
  uint64_t size = Message::mget_size();
  return size;
}

void ReadyServer::copy_from_txn(TxnManager * txn) {
}

void ReadyServer::copy_to_txn(TxnManager * txn) {
  Message::mcopy_to_txn(txn);
}

void ReadyServer::copy_from_buf(char * buf) {
  Message::mcopy_from_buf(buf);
}

void ReadyServer::copy_to_buf(char * buf) {
  Message::mcopy_to_buf(buf);
}

/************************/

uint64_t KeyExchange::get_size() {
  uint64_t size = Message::mget_size();

  size += sizeof(uint64_t);
  size += pkey.size();
  size += sizeof(uint64_t);
  return size;
}

void KeyExchange::copy_from_txn(TxnManager * txn) {
}

void KeyExchange::copy_to_txn(TxnManager * txn) {
  Message::mcopy_to_txn(txn);
}

void KeyExchange::copy_from_buf(char * buf) {
  Message::mcopy_from_buf(buf);
  uint64_t ptr = Message::mget_size();
  COPY_VAL(pkeySz,buf,ptr);
  char v;
  for(uint64_t j = 0; j < pkeySz; j++){
	COPY_VAL(v, buf, ptr);
	pkey += v;
  }
  COPY_VAL(return_node,buf,ptr);
}

void KeyExchange::copy_to_buf(char * buf) {
  Message::mcopy_to_buf(buf);
  uint64_t ptr = Message::mget_size();
  COPY_BUF(buf,pkeySz,ptr);
  char v;
  for(uint64_t j = 0; j < pkeySz; j++){
  	v = pkey[j];
  	COPY_BUF(buf, v, ptr);
  }
  COPY_BUF(buf,return_node,ptr);
}

/************************/

void YCSBQueryMessage::init() {
}

void YCSBQueryMessage::release() {
  QueryMessage::release();
  // Freeing requests is the responsibility of txn
/*
  for(uint64_t i = 0; i < requests.size(); i++) {
    DEBUG_M("YCSBQueryMessage::release ycsb_request free\n");
    mem_allocator.free(requests[i],sizeof(ycsb_request));
  }
*/
  requests.release();
}

uint64_t YCSBQueryMessage::get_size() {
  uint64_t size = QueryMessage::get_size();
  size += sizeof(size_t);
  size += sizeof(ycsb_request) * requests.size();
  return size;
}

void YCSBQueryMessage::copy_from_txn(TxnManager * txn) {
  QueryMessage::copy_from_txn(txn);
  requests.init(g_req_per_query);
  //requests.copy(((YCSBQuery*)(txn->query))->requests);
}

void YCSBQueryMessage::copy_to_txn(TxnManager * txn) {
  QueryMessage::copy_to_txn(txn);
  //((YCSBQuery*)(txn->query))->requests.copy(requests);
  ((YCSBQuery*)(txn->query))->requests.append(requests);
}


void YCSBQueryMessage::copy_from_buf(char * buf) {
  QueryMessage::copy_from_buf(buf);
  uint64_t ptr = QueryMessage::get_size();
  size_t size;
  COPY_VAL(size,buf,ptr);
  assert(size<=g_req_per_query);
  requests.init(size);
  for(uint64_t i = 0 ; i < size;i++) {
    DEBUG_M("YCSBQueryMessage::copy ycsb_request alloc\n");
    ycsb_request * req = (ycsb_request*)mem_allocator.alloc(sizeof(ycsb_request));
    COPY_VAL(*req,buf,ptr);
    ASSERT(req->key < g_synth_table_size);
    requests.add(req);
  }
 assert(ptr == get_size());
}

void YCSBQueryMessage::copy_to_buf(char * buf) {
  QueryMessage::copy_to_buf(buf);
  uint64_t ptr = QueryMessage::get_size();
  size_t size = requests.size();
  COPY_BUF(buf,size,ptr);
  for(uint64_t i = 0; i < requests.size(); i++) {
    ycsb_request * req = requests[i];
    COPY_BUF(buf,*req,ptr);
  }
 assert(ptr == get_size());
}


/****************************************/

#if CLIENT_BATCH

uint64_t ClientQueryBatch::get_size() {
  uint64_t size = Message::mget_size();
  
  size += sizeof(return_node);
  size += sizeof(batch_size);

  for(uint i=0; i < g_batch_size; i++) { 
        size += cqrySet[i].get_size();
  }

  return size;
}

void ClientQueryBatch::init() {
  this->return_node = g_node_id;
  this->batch_size = g_batch_size;

  this->cqrySet.resize(g_batch_size);  
  //Message * cmsg = Message::create_message(CL_QRY);
  //for(uint i=0; i<g_batch_size; i++) {
  //	((YCSBClientQueryMessage *)cmsg)->txn_id = UINT64_MAX;
  //	cqrySet[i] = *((YCSBClientQueryMessage *)cmsg);
  //}
}

void ClientQueryBatch::release() {
  vector<YCSBClientQueryMessage>().swap(cqrySet);
}


void ClientQueryBatch::copy_from_txn(TxnManager * txn) {
  assert(0);
}

void ClientQueryBatch::copy_to_txn(TxnManager * txn) {
  Message::mcopy_to_txn(txn);
}

void ClientQueryBatch::copy_from_buf(char * buf) {
	Message::mcopy_from_buf(buf);
  
	uint64_t ptr = Message::mget_size();
	COPY_VAL(return_node,buf,ptr);
	COPY_VAL(batch_size,buf,ptr);

	cqrySet.resize(g_batch_size);
	for(uint i=0; i<g_batch_size; i++) {
		Message * msg = create_message(&buf[ptr]);
	    	ptr += msg->get_size();
		cqrySet[i] = *((YCSBClientQueryMessage*)msg);
	}

	assert(ptr == get_size());
}

void ClientQueryBatch::copy_to_buf(char * buf) {
	Message::mcopy_to_buf(buf);
  
	uint64_t ptr = Message::mget_size();
	COPY_BUF(buf,return_node,ptr);
	COPY_BUF(buf,batch_size,ptr);

	for(uint i=0; i<g_batch_size; i++) {
		cqrySet[i].copy_to_buf(&buf[ptr]);
		ptr += cqrySet[i].get_size();
	}

	assert(ptr == get_size());
}

void ClientQueryBatch::sign()
{
#if USE_CRYPTO
	string message = std::to_string(g_node_id);
	YCSBClientQueryMessage yqry;
	for(uint i=0; i<g_batch_size; i++) {
  		yqry = cqrySet[i];
		message += yqry.getString();
  	}
	
	auto signature(RsaSignString(g_priv_key, message));
	this->signature = signature;
#else
	this->signature = "0";
#endif
	this->pubKey = g_pub_keys[g_node_id];
	
	this->sigSize = this->signature.size();
	this->keySize = this->pubKey.size();
}


bool ClientQueryBatch::validate()
{
	//printf("SYCSBClientQueryMessage::validate: %ld\n",txn_id);
	//fflush(stdout);

#if USE_CRYPTO
	string message = std::to_string(this->return_node);
	YCSBClientQueryMessage yqry;
	for(uint i=0; i<g_batch_size; i++) {
  		yqry = cqrySet[i];
		message += yqry.getString();
  	}
	
	// make sure signature is valid
	if(! RsaVerifyString(g_pub_keys[this->return_node], message, this->signature) ) {
		assert(0);
		return false;
	}
	
	//make sure message was signed by the node this message claims it is from
	if (this->pubKey != g_pub_keys[this->return_node])
	{
		assert (0);
		return false;
	}
#endif
	return true;
}

string ClientQueryBatch::getString()
{
	string message = std::to_string(this->return_node);

	for(int i = 0; i < BATCH_SIZE; i++)
	{
		message += cqrySet[i].getString();
	} 
	
	return message;
}


#endif

#if CONSENSUS == PBFT || CONSENSUS == DBFT

#if BATCH_ENABLE == BSET

#if RBFT_ON
uint64_t PropagateBatch::get_size() {
  uint64_t size = Message::mget_size();

  size += sizeof(uint64_t) * g_batch_size;

  for(uint i=0; i < g_batch_size; i++) {
        size += hash[i].length();
  }

  for(uint i=0; i < g_batch_size; i++) {
        size += requestMsg[i].get_size();
  }

  size += sizeof(batch_size);

  return size;
}

void PropagateBatch::copy_from_txn(TxnManager * txn) {

	this->txn_id = txn->get_txn_id() - 7;


	uint64_t relIndex;	uint i;
	uint64_t begintxn = batchSet[txn->cbatch % indexSize];

	for(i=0; i<g_batch_size; i++) {
		relIndex = begintxn % indexSize;
		YCSBClientQueryMessage clqry = cqryStore[relIndex];
		if(clqry.txn_id == begintxn) {
			this->requestMsg[i] = clqry;
		} else {
		    cout << "txn id: " << this->txn_id << " clqry: " << clqry.txn_id << " VS " << begintxn << endl;
		    fflush(stdout);
			assert(0);
		}
		//create message digest
		this->hash[i] = picosha2::hash256_hex_string(this->requestMsg[i].getString());
		this->hashSize[i] = hash[i].length();

		begintxn++;
	}

	this->batch_size = g_batch_size;

	this->sign();
}

void PropagateBatch::copy_to_txn(TxnManager * txn) {
  Message::mcopy_to_txn(txn);
}

void PropagateBatch::copy_from_buf(char * buf) {
	Message::mcopy_from_buf(buf);

	uint64_t ptr = Message::mget_size();

	uint64_t elem;
	for(uint i=0; i<g_batch_size; i++) {

		COPY_VAL(elem,buf,ptr);
		hashSize[i] = elem;

		string thash;
		ptr = buf_to_string(buf, ptr, thash, hashSize[i]);
		hash[i] = thash;

		Message * msg = create_message(&buf[ptr]);
	    	ptr += msg->get_size();
		requestMsg[i] = *((YCSBClientQueryMessage*)msg);
	}

	COPY_VAL(batch_size,buf,ptr);

	assert(ptr == get_size());
}

void PropagateBatch::copy_to_buf(char * buf) {
	Message::mcopy_to_buf(buf);

	uint64_t ptr = Message::mget_size();

	uint64_t elem;
	for(uint i=0; i<g_batch_size; i++) {

		elem = hashSize[i];
		COPY_BUF(buf,elem,ptr);

		char v;
		string hstr = hash[i];
		for(uint j = 0; j < hstr.size(); j++) {
			v = hstr[j];
			COPY_BUF(buf, v, ptr);
		}

		//copy client request stored in message to buf
		requestMsg[i].copy_to_buf(&buf[ptr]);
		ptr += requestMsg[i].get_size();
	}

	COPY_BUF(buf,batch_size,ptr);

	assert(ptr == get_size());
}

void PropagateBatch::sign()
{
#if USE_CRYPTO
	string message = this->hash[0];

	//cout << "Message signed: " << message << "\n";
	//fflush(stdout);


	auto signature(RsaSignString(g_priv_key, message));
	this->signature = signature;

	//cout << "Signature: " << signature << "\n";
	//fflush(stdout);

#else
	this->signature = "0";
#endif
	this->pubKey = g_pub_keys[g_node_id];

	this->sigSize = this->signature.size();
	this->keySize = this->pubKey.size();
}

//makes sure message is valid, returns true for false
bool PropagateBatch::validate() {

#if USE_CRYPTO
	string message = this->hash[0];
	//cout << "Input message: " << message << "\n";

    //cout << "from: " << this->return_node_id;
	//cout << "Signed Signature: " << signature << "\n";
	//fflush(stdout);


	//is signature valid
	if (! RsaVerifyString(g_pub_keys[this->return_node_id], message, this->signature) ) {
		assert(0);
		return false;
	}

#endif

//	//is client request valid
//	if (!this->requestMsg.validate() )
//	{
//		assert(0);
//		return false;
//	}

	//is hash of request message valid
	for(uint i=0; i<g_batch_size; i++) {
	  if (this->hash[i] != picosha2::hash256_hex_string(this->requestMsg[i].getString())) {
	  	assert(0);
	  	return false;
	  } }

	//cout << "Done Hash\n";
	//fflush(stdout);

#if USE_CRYPTO
	// was this pre-prepare message sent by the node it claims to be
	if(this->pubKey != g_pub_keys[this->return_node_id]) {
		assert(0);
		return false;
	}

	//cout << "Done key match \n";
	//fflush(stdout);

#endif

	return true;
}
#endif

#if VIEW_CHANGES == true
BatchRequests breqStore[2 * CLIENT_NODE_CNT * MAX_TXN_IN_FLIGHT];
//requests that were redirected to primary (may need to execute if primary changes)
ClientQueryBatch g_redirected_requests[3 * CLIENT_NODE_CNT * MAX_TXN_IN_FLIGHT];
#endif

uint64_t BatchRequests::get_size() {
  uint64_t size = Message::mget_size();
  
  size += sizeof(view);

  size += sizeof(uint64_t) * index.size();
  size += sizeof(uint64_t) * hashSize.size();

  for(uint i=0; i < g_batch_size; i++) {
        size += hash[i].length();
  }

  for(uint i=0; i < g_batch_size; i++) { 
        size += requestMsg[i].get_size();
  }

#if ZYZZYVA == true
  size += sizeof(uint64_t) * stateHashSize.size();
  for(uint i=0; i < g_batch_size; i++) {
        size += stateHash[i].length();
  }
#endif

  size += sizeof(batch_size);

  return size;
}

void BatchRequests::copy_from_txn(TxnManager * txn) {
#if RBFT_ON
    	assert(g_view == g_instance_id);
#else
	assert(g_view == g_node_id);// only primary creates this message
#endif

	
    // Setting txn_id 2 less than the actual value.
#if RBFT_ON 
    // if prop_txn_man is used then txn id is 4 less than cindex
    if(txn->prop_rsp_cnt != 0)
    	this->txn_id = txn->get_txn_id() + 5;
    else
#endif	
	this->txn_id = txn->get_txn_id() - 2;
	

#if RBFT_ON
    this->view = g_instance_id;
#else
    this->view = g_view;
#endif

#if RBFT_ON
    this->instance_id = g_instance_id;
#endif
	
	uint64_t relIndex;	uint i;
	uint64_t begintxn = batchSet[txn->cbatch % indexSize];
	//cout << "Making BR: begintxn: " << begintxn << " VS " << this->txn_id << endl;
	string mstate;

	// Initialization
	this->index.init(g_batch_size);
	this->hashSize.init(g_batch_size);
	this->hash.resize(g_batch_size);
	this->requestMsg.resize(g_batch_size);

    #if ZYZZYVA == true
	this->stateHashSize.resize(g_batch_size);
	this->stateHash.resize(g_batch_size);
    #endif

	for(i=0; i<g_batch_size; i++) {
		relIndex = begintxn % indexSize;
		YCSBClientQueryMessage clqry = cqryStore[relIndex];
		if(clqry.txn_id == begintxn) {
			this->requestMsg[i] = clqry;
			this->index.add(begintxn);
			//this->index[i] = begintxn;
		} else {
			assert(0);
		}
		//create message digest
		this->hash[i] = picosha2::hash256_hex_string(this->requestMsg[i].getString());
		//this->hashSize[i] = hash[i].length();
		this->hashSize.add(hash[i].length());

	//	cout << "Sent: " << clqry.requests[clqry.requests.size()-1]->key << "\n";
	//  	fflush(stdout);

   #if RBFT_ON 	// Only master instance can change data structure
       		if(this->instance_id != g_master_instance) {
       		   begintxn++;
       		   continue;
       		}
   #endif
		// Adding PRE message.
		Message * msg = Message::create_message(PBFT_PRE_MSG);
		PBFTPreMessage * premsg = (PBFTPreMessage *)msg;
		premsg->txn_id 		= this->index[i];
		premsg->index 		= this->index[i];
		premsg->hashSize 	= this->hashSize[i];
		premsg->hash 		= this->hash[i];
		premsg->requestMsg 	= this->requestMsg[i];
   #if RBFT_ON
        	premsg->instance_id = this->instance_id;
   #endif
       		preStore[relIndex] 	= *premsg;
       		//cout << "updated prestore[" << relIndex << "]: " << premsg->hash << endl;
       		//fflush(stdout);

	#if CONSENSUS == DBFT
		// Maintaining minimal state for DBFT.
		if((premsg->txn_id + 1) % g_batch_size == 0) {
			mstate = mstate + to_string(premsg->txn_id);
            		//cout << "mstate: " << mstate << endl;
          #if RBFT_ON
            		mystate[relIndex][this->instance_id] = mstate;
          #else
			mystate[relIndex] = mstate;
          #endif
   			string hashedTriple = std::to_string(hashGen(mstate));
          #if RBFT_ON
            	      if(this->instance_id == g_master_instance)
          #endif
            		mytriple[relIndex] = hashedTriple;
		} else {
			mstate = mstate + to_string(premsg->txn_id) + ":";
		}
		//stateUpdate(premsg->txn_id);
	#endif

	#if ZYZZYVA == true
		// Initializing.
		this->stateHash[i] = "0";
		this->stateHashSize[i] = this->stateHash[i].length();
      	    #if LOCAL_FAULT
      	      if(i == g_batch_size - 1) {
      	      	cltmap[cqryStore[relIndex].client_startts] = begintxn;
      	      }
      	    #endif

	#endif

		begintxn++;
	}

   #if !RBFT_ON // msg will be made while cl batches continue to be received
	if(this->index[i-1] != txn->get_txn_id()) {
		cout << "Idx: " << this->index[i-1] << " :: " << txn->get_txn_id() << " :: batch: " << txn->cbatch << "\n";
		fflush(stdout);
		//assert(this->index[i-1] == txn->get_txn_id());
	}
   #endif

	this->batch_size = g_batch_size;
	this->sign();
}

void BatchRequests::release() {
  index.release();
  hashSize.release();
  vector<string>().swap(hash);
  vector<YCSBClientQueryMessage>().swap(requestMsg);

#if ZYZZYVA == true
  vector<uint64_t>().swap(stateHashSize);
  vector<string>().swap(stateHash);
#endif
}

void BatchRequests::copy_to_txn(TxnManager * txn) {
  Message::mcopy_to_txn(txn);
}

void BatchRequests::copy_from_buf(char * buf) {
	Message::mcopy_from_buf(buf);
  
	uint64_t ptr = Message::mget_size();
	COPY_VAL(view,buf,ptr);

	uint64_t elem;
	// Initialization
	index.init(g_batch_size);
	hashSize.init(g_batch_size);
	hash.resize(g_batch_size);
	requestMsg.resize(g_batch_size);

    #if ZYZZYVA == true
	this->stateHashSize.resize(g_batch_size);
	this->stateHash.resize(g_batch_size);
    #endif

	for(uint i=0; i<g_batch_size; i++) {
		COPY_VAL(elem,buf,ptr);
		index.add(elem);

		COPY_VAL(elem,buf,ptr);
		hashSize.add(elem);	
		//string thash;
		//ptr = buf_to_string(buf, ptr, thash, hashSize[i]);
		//hash.push_back(thash);

		string thash;
		ptr = buf_to_string(buf, ptr, thash, hashSize[i]);
		hash[i] = thash;

		Message * msg = create_message(&buf[ptr]);
	    	ptr += msg->get_size();
		requestMsg[i] = *((YCSBClientQueryMessage*)msg);

	   #if ZYZZYVA == true
		COPY_VAL(elem,buf,ptr);
		stateHashSize[i] = elem;

		string tstate;
		ptr = buf_to_string(buf, ptr, tstate, stateHashSize[i]);
		stateHash[i] = tstate;
	   #endif
	}

	COPY_VAL(batch_size,buf,ptr);
  
	assert(ptr == get_size());
}

void BatchRequests::copy_to_buf(char * buf) {
	Message::mcopy_to_buf(buf);
  
	uint64_t ptr = Message::mget_size();
	COPY_BUF(buf,view,ptr);

	uint64_t elem;
	for(uint i=0; i<g_batch_size; i++) {
		elem = index[i];
		COPY_BUF(buf,elem,ptr);

		elem = hashSize[i];
		COPY_BUF(buf,elem,ptr);

		char v;
		string hstr = hash[i];
		for(uint j = 0; j < hstr.size(); j++) {
			v = hstr[j];
			COPY_BUF(buf, v, ptr);
		}

		//copy client request stored in message to buf
		requestMsg[i].copy_to_buf(&buf[ptr]);
		ptr += requestMsg[i].get_size();

           #if ZYZZYVA == true
		elem = stateHashSize[i];
		COPY_BUF(buf,elem,ptr);

		string tstr = stateHash[i];
		for(uint j = 0; j < tstr.size(); j++) {
			v = tstr[j];
			COPY_BUF(buf, v, ptr);
		}	
	   #endif
	}

	COPY_BUF(buf,batch_size,ptr);

	assert(ptr == get_size());
}


void BatchRequests::sign()
{
#if USE_CRYPTO
	string message = this->toString();

	//cout << "Message signed: " << message << "\n";
	//fflush(stdout);

	auto signature(RsaSignString(g_priv_key, message));
	this->signature = signature;

	//cout << "Signature: " << signature << "\n";
	//fflush(stdout);

#else
	this->signature = "0";
#endif
	this->pubKey = g_pub_keys[g_node_id];
	
	this->sigSize = this->signature.size();
	this->keySize = this->pubKey.size();
}


string BatchRequests::toString()
{
	string message = std::to_string(this->view);
	for(uint i=0; i< g_batch_size; i++) {
		message += std::to_string(index[i]) + hash[i];
	}

	return message; 
}

bool BatchRequests::addAndValidate()
{
	if(!this->validate()) {
		return false;	
	}

   #if CONSENSUS == PBFT
	//is the view the same as the view observed by this message
     #if !RBFT_ON
	if (this->view != g_view) {
		assert(0);
		return false;
	}
     #endif
   #endif

   #if ZYZZYVA == true
	for(uint64_t i=0; i<g_batch_size; i++) {
  		istate = std::to_string(hashGen((istate + this->hash[i])));
		if(istate != this->stateHash[i]) {
			assert(0);
		} }
   #endif

	uint64_t relIndex;
	PBFTPreMessage premsg;
	string mstate;

   #if VIEW_CHANGES == true
	char * buf = (char*)malloc(this->get_size() + 1);
	this->copy_to_buf(buf);
	Message * deepCopyMsg = Message::create_message(buf);
	
	uint64_t idx = this->index[g_batch_size-1];
	breqStore[idx % indexSize] = *((BatchRequests*)deepCopyMsg);
   #endif
	
	for(uint i=0; i<g_batch_size; i++) {
	   relIndex = this->index[i] % indexSize;
	   premsg = preStore[relIndex];
	   if(premsg.index == this->index[i] 
	   	&& premsg.hash == this->hash[i]) {
        #if !RBFT_ON
	   	assert(0);
        #endif
	   }
        #if RBFT_ON
       	   // only master instance can change data structures
       	   //cout << "this instance: " << this->instance_id << " this txn: " << (int)premsg.txn_id << endl;
       	   //fflush(stdout);
       	   if(this->instance_id != g_master_instance) {
       	           continue;
       	   }
        #endif
	   // Adding PRE message.
	   premsg.txn_id = this->index[i];
	   premsg.index = this->index[i];
	   premsg.hashSize = this->hashSize[i];
	   premsg.hash = this->hash[i];
	   premsg.requestMsg = this->requestMsg[i];
        #if RBFT_ON
           if(this->instance_id == g_master_instance) {
               premsg.instance_id = this->instance_id;
           }
        #endif

	#if ZYZZYVA == true
	   premsg.stateHash = this->stateHash[i];
	#endif

	   preStore[relIndex] = premsg;

        #if CONSENSUS == DBFT
	   // Maintaining minimal state for DBFT.
	   if((premsg.txn_id + 1) % g_batch_size == 0) {
	   	mstate = mstate + to_string(premsg.txn_id);
            #if RBFT_ON
           	mystate[relIndex][this->instance_id] = mstate;
            #else
		mystate[relIndex] = mstate;
            #endif
	   } else {
	   	mstate = mstate + to_string(premsg.txn_id) + ":";
	   }
	   //stateUpdate(premsg.txn_id);
        #endif
	   
	   // Adding client request.
        #if !RBFT_ON
	   cqryStore[relIndex] = this->requestMsg[i];
        #endif

	#if ZYZZYVA == true && LOCAL_FAULT
		if(i == g_batch_size - 1) {
			cltmap[cqryStore[relIndex].client_startts] = this->index[i];
		}
	#endif

	}

	return true;
}

//makes sure message is valid, returns true for false
bool BatchRequests::validate() {
	
#if USE_CRYPTO
	string message = this->toString();
	//cout << "Input message: " << message << "\n";

	//is signature valid
	if (! RsaVerifyString(g_pub_keys[this->view], message, this->signature) ) {
		assert(0);
		return false;
	}

#endif
	
//	//is client request valid
//	if (!this->requestMsg.validate() )
//	{
//		assert(0);
//		return false;
//	}
		
	//is hash of request message valid
	for(uint i=0; i<g_batch_size; i++) {
	  if (this->hash[i] != picosha2::hash256_hex_string(this->requestMsg[i].getString())) {
	  	assert(0);
	  	return false;
	  } }

	//cout << "Done Hash\n";
	//fflush(stdout);

#if USE_CRYPTO
	// was this pre-prepare message sent by the node it claims to be
	if(this->pubKey != g_pub_keys[this->view]) {
		assert(0);
		return false;
	}

	//cout << "Done key match \n";
	//fflush(stdout);

#endif
	
	return true;
}

#if CONSENSUS == DBFT
void BatchRequests::stateUpdate(uint64_t txn_id) 
{
	// Maintaining minimal state for DBFT.
	if((txn_id + 1) % g_batch_size == 0) {
		stateRep = stateRep + ":" + to_string(txn_id) + 
			"::" + to_string(g_view);
	} else if(txn_id == 0) {
		stateRep = ":" + to_string(txn_id);
		lastStateRep = stateRep;
	} else {
		stateRep = stateRep + ":" + to_string(txn_id);
	}
}
#endif


#endif

/**************************/


uint64_t PBFTPreMessage::get_size() {
  uint64_t size = Message::mget_size();
  
  size += sizeof(view);

  size += sizeof(index);
  size += sizeof(hashSize); 
  size += hash.length();
  size += requestMsg.get_size();

  return size;
}

void PBFTPreMessage::copy_from_txn(TxnManager * txn) {
	cout << "HERE!";
	assert(g_view == g_node_id);// only primary creates this message
	this->view = g_view;
	
	uint64_t relIndex = this->txn_id % indexSize;
	YCSBClientQueryMessage clqry = cqryStore[relIndex];
	if(clqry.txn_id == this->txn_id) {
		this->requestMsg = clqry;
		this->index = this->txn_id;
	} else {
		assert(0);
	}


	//create message digest
	this->hash = "0"; //picosha2::hash256_hex_string(this->requestMsg.getString());
	this->hashSize = hash.length();

	//this->sign();

	//@Suyash
	//printf("PBFTPreMessage -- copy_from_txn: Index:%ld : View:%ld Node:%d\n",this->index, this->view, g_node_id);
	//fflush(stdout);
}

void PBFTPreMessage::copy_to_txn(TxnManager * txn) {
  Message::mcopy_to_txn(txn);
  txn->client_id = this->requestMsg.return_node;
  txn->client_startts = this->requestMsg.client_startts;
  ((YCSBQuery*)(txn->query))->requests.append(this->requestMsg.requests);
}

void PBFTPreMessage::copy_from_buf(char * buf) {
	Message::mcopy_from_buf(buf);
  
	uint64_t ptr = Message::mget_size();

	COPY_VAL(view,buf,ptr);

	COPY_VAL(index,buf,ptr); 

	COPY_VAL(hashSize,buf,ptr); 
	ptr = buf_to_string(buf, ptr, hash, hashSize);
	
	//create client request message (stored in pre-prepare message) from buffer
	Message * msg = create_message(&buf[ptr]);
    	ptr += msg->get_size();
	requestMsg = *((YCSBClientQueryMessage*)msg);
  
	assert(ptr == get_size());
}

void PBFTPreMessage::copy_to_buf(char * buf) {
	Message::mcopy_to_buf(buf);
  
	uint64_t ptr = Message::mget_size();
	COPY_BUF(buf,view,ptr);

	COPY_BUF(buf,index,ptr);
	COPY_BUF(buf,hashSize,ptr);

	char v;
	for(uint64_t i = 0; i < hash.size(); i++){
		v = hash[i];
		COPY_BUF(buf, v, ptr);
	}
  
	//copy client request stored in message to buf
	requestMsg.copy_to_buf(&buf[ptr]);
	ptr += requestMsg.get_size();
  
	assert(ptr == get_size());
}

/*
void PBFTPreMessage::sign()
{
//TODO: Switch this on.

#if USE_CRYPTO
	string message = this->toString();
	
	auto signature(RsaSignString(g_priv_key, message));
	this->signature = signature;
#else
	this->signature = "0";
#endif

	this->signature = "0";

	this->pubKey = g_pub_keys[g_node_id];
	
	this->sigSize = this->signature.size();
	this->keySize = this->pubKey.size();
}
*/

string PBFTPreMessage::toString()
{
	return std::to_string(this->view) + '_' + std::to_string(this->index) + '_' + this->hash;
}

bool PBFTPreMessage::addAndValidate()
{
   #if CONSENSUS == PBFT
	//is the view the same as the view observed by this message
	if (this->view != g_view) {
		assert(0);
		return false;
	}
   #endif

	uint64_t relIndex = this->index % indexSize;
	PBFTPreMessage premsg = preStore[relIndex];
	if(premsg.txn_id == this->txn_id) {
		if(premsg.index == this->index && premsg.view == this->view) {
				//&& premsg.hash == this->hash) {
			assert(0);
		} }

	// Adding PRE message.
	preStore[relIndex] = *this;

	// Adding client request.
	cqryStore[relIndex] = this->requestMsg;

	return true;
}

//makes sure message is valid, returns true for false
bool PBFTPreMessage::validate() {
	
//#if USE_CRYPTO
//	string message = this->toString();
//	//is signature valid
//	if (! RsaVerifyString(this->pubKey, message, this->signature) ) {
//		assert(0);
//		return false;
//	}
//#endif
	
//	//is client request valid
//	if (!this->requestMsg.validate() )
//	{
//		assert(0);
//		return false;
//	}
		
//	//is hash of request message valid
//	if (this->hash != picosha2::hash256_hex_string( this->requestMsg.getString())) {
//		assert(0);
//		return false;
//	}

#if USE_CRYPTO
	// was this pre-prepare message sent by the node it claims to be
	if(this->pubKey != g_pub_keys[this->view]) {
		assert(0);
		return false;
	}
#endif
	
	return true;
}


/************************************/

uint64_t ExecuteMessage::get_size() {
  uint64_t size = Message::mget_size();
  
  size += sizeof(view);
  size += sizeof(index);
  size += hash.length();
  size += sizeof(hashSize);
  size += sizeof(return_node);

  #if BATCH_ENABLE == BSET
    size += sizeof(end_index);
    size += sizeof(batch_size);
  #endif
  
  return size;
}

void ExecuteMessage::copy_from_txn(TxnManager * txn) {
	// Constructing txn manager for one transaction less than end index.
#if CONSENSUS == DBFT
    #if RBFT_ON
   	// to tell if from BR or not
   	if(txn->get_txn_id() + 1 % g_batch_size != 0)
   		this->txn_id = txn->get_txn_id() + 5;
   	else
    #endif
#endif
		this->txn_id = txn->get_txn_id() - 1;
	
	this->view = g_view;
	
	uint64_t relIndex = this->txn_id % indexSize;
	PBFTPreMessage premsg = preStore[relIndex];
	if(premsg.txn_id == this->txn_id) {
		this->index = premsg.index;
		this->hash = premsg.hash;
		this->hashSize = premsg.hashSize;
	} else {
		assert(0);
	}
	
	this->return_node = g_node_id;

#if BATCH_ENABLE == BSET
	this->end_index = this->index;
	this->batch_size = g_batch_size;
  #if CONSENSUS == DBFT
     #if RBFT_ON
   	if(txn->get_txn_id() + 1 % g_batch_size != 0)
   		this->index = (txn->get_txn_id() + 7) - this->batch_size;
   	else
     #endif
  #endif
	  	this->index = (txn->get_txn_id() + 1) - this->batch_size;
#endif
}

void ExecuteMessage::copy_to_txn(TxnManager * txn) {
	Message::mcopy_to_txn(txn);
}

void ExecuteMessage::copy_from_buf(char * buf) {
	Message::mcopy_from_buf(buf);
  
	uint64_t ptr = Message::mget_size();

	COPY_VAL(view,buf,ptr);
	COPY_VAL(index,buf,ptr); 
	COPY_VAL(hashSize,buf,ptr); 
  
	ptr = buf_to_string(buf, ptr, hash, hashSize);
	
	COPY_VAL(return_node,buf,ptr); 

 #if BATCH_ENABLE == BSET
 	COPY_VAL(end_index, buf, ptr);
   #if CONSENSUS == PBFT
 	COPY_VAL(batch_size, buf, ptr);
   #endif
 #endif
  
	assert(ptr == get_size());
}

void ExecuteMessage::copy_to_buf(char * buf) {
	Message::mcopy_to_buf(buf);
    
	uint64_t ptr = Message::mget_size();
  
	COPY_BUF(buf,view,ptr);
	COPY_BUF(buf,index,ptr);
	COPY_BUF(buf,hashSize,ptr);
	
  	char v;
	for(uint64_t i = 0; i < hash.size(); i++){
		v = hash[i];
		COPY_BUF(buf, v, ptr);
	}
  
	COPY_BUF(buf,return_node,ptr);

#if BATCH_ENABLE == BSET
	  COPY_BUF(buf,end_index,ptr);
  #if CONSENSUS == PBFT
	  COPY_BUF(buf,batch_size,ptr);
  #endif
#endif
	
	assert(ptr == get_size());
}


/************************************/

uint64_t CheckpointMessage::get_size() {
  uint64_t size = Message::mget_size();
  size += sizeof(index);
  size += sizeof(return_node);

#if CONSENSUS == PBFT
  size += sizeof(beg_index);
#endif

  return size;
}

void CheckpointMessage::copy_from_txn(TxnManager * txn) {
  this->txn_id = txn->get_txn_id() - 5;
  this->index = g_next_index - 5;//index of last executed request
  this->return_node = g_node_id;

#if CONSENSUS == PBFT
  this->beg_index = g_next_index - g_txn_per_chkpt;
#endif

  this->sign();
}

//unused
void CheckpointMessage::copy_to_txn(TxnManager * txn) {
  assert(0);
  Message::mcopy_to_txn(txn);
}

void CheckpointMessage::copy_from_buf(char * buf) {
	Message::mcopy_from_buf(buf);
	
	uint64_t ptr = Message::mget_size();
	
  	COPY_VAL(index,buf,ptr);
	COPY_VAL(return_node,buf,ptr); 

   #if CONSENSUS == PBFT
	COPY_VAL(beg_index,buf,ptr);
   #endif
	
	assert(ptr == get_size());
}

void CheckpointMessage::copy_to_buf(char * buf) {
	Message::mcopy_to_buf(buf);
	
	uint64_t ptr = Message::mget_size();
	
  	COPY_BUF(buf,index,ptr);
	COPY_BUF(buf,return_node,ptr);

   #if CONSENSUS == PBFT
	COPY_BUF(buf,beg_index,ptr);
   #endif
	
	assert(ptr == get_size());
}

void CheckpointMessage::sign()
{
#if USE_CRYPTO
	string message = this->toString();
	
	auto signature(RsaSignString(g_priv_key, message));
	this->signature = signature;
#else
	this->signature = "0";
#endif
	
	this->pubKey = g_pub_keys[g_node_id];
	
	this->sigSize = this->signature.size();
	this->keySize = this->pubKey.size();
}

//validate message, add to message log, and check if node has enough messages
bool CheckpointMessage::addAndValidate(){
	if(!this->validate()) {
		return false;	
	}	
	

	//make sure we arent accepting extra messages 
	if(this->index == g_last_stable_chkpt) {
		return false;
	}

	//count number of accepted response messages for this checkpoint
	int k = 0;
	bool add = true;
	for(int i = g_pbft_chkpt_msgs.size() - 1; i >= 0; i--) {
		if (g_pbft_chkpt_msgs[i].index == this->index) {
		     #if CONSENSUS == PBFT	
			if (g_pbft_chkpt_msgs[i].beg_index == this->beg_index) {
				if(g_pbft_chkpt_msgs[i].return_node == this->return_node){
					add = false;//dont want to add duplicate message
				}	
				k++;
			}
		     #endif 
		} }
	if(add) {
		g_pbft_chkpt_msgs.push_back(*this);
		k++;//message just push_backed counts towards # of recieved messages
	}
	if (k < (2 * g_min_invalid_nodes)) {
		return false; 
	}
	
	return true;
}

string CheckpointMessage::toString(){
	return std::to_string(this->index) + '_' + std::to_string(this->return_node);//still needs digest of state
}

//is message valid
bool CheckpointMessage::validate()
{
#if USE_CRYPTO
	string message = this->toString();

	//verify signature of message
	if (! RsaVerifyString(g_pub_keys[this->return_node], message, this->signature) ) {
		assert(0);
		return false;
	}
	
	//make sure message was signed by the node this message claims it is from
	if (this->pubKey != g_pub_keys[this->return_node]) {
		assert (0);
		return false;
	}
#endif
	
	return true;
}


/************************************/



//function for copying a string into the char buffer
uint64_t Message::string_to_buf(char * buf, uint64_t ptr, string str) {
	char v;
	for(uint64_t i = 0; i < str.size(); i++) {
		v = str[i];
		COPY_BUF(buf, v, ptr);
	}
	return ptr;
}

//function for copying data from the buffer into a string
uint64_t Message::buf_to_string(char * buf, uint64_t ptr, string& str, uint64_t strSize) {
	char v;
	for(uint64_t i = 0; i < strSize; i++){
		COPY_VAL(v, buf, ptr);
		str += v;
	}
	return ptr;
}


//Arrays that stores messages of each type.
//YCSBClientQueryMessage cqryStore[2 * CLIENT_NODE_CNT * MAX_TXN_IN_FLIGHT];
//PBFTPreMessage preStore[2 * CLIENT_NODE_CNT * MAX_TXN_IN_FLIGHT];
//ClientResponseMessage clrspStore[2 * CLIENT_NODE_CNT * MAX_TXN_IN_FLIGHT][NODE_CNT];

vector<YCSBClientQueryMessage> cqryStore(2 * CLIENT_NODE_CNT * MAX_TXN_IN_FLIGHT);
vector<PBFTPreMessage> preStore(2 * CLIENT_NODE_CNT * MAX_TXN_IN_FLIGHT);
vector<vector<ClientResponseMessage>> clrspStore(2 * CLIENT_NODE_CNT * MAX_TXN_IN_FLIGHT, 
					vector<ClientResponseMessage>(NODE_CNT));
vector<CheckpointMessage> g_pbft_chkpt_msgs;


#endif 



/**************************/

#if CONSENSUS == PBFT

uint64_t PBFTPrepMessage::get_size() {
  uint64_t size = Message::mget_size();
  
  size += sizeof(view);
  size += sizeof(index);
  size += hash.length();
  size += sizeof(hashSize);
  size += sizeof(return_node);

  #if BATCH_ENABLE == BSET
    size += sizeof(end_index);
    //size += sizeof(batch_idx);
    size += sizeof(batch_size);
  #endif

  return size;
}

void PBFTPrepMessage::copy_from_txn(TxnManager * txn) {

	Message::mcopy_from_txn(txn);
#if !RBFT_ON
	assert(g_view != g_node_id);// primary des not create this message
#endif
	this->view = g_view;
	
	uint64_t relIndex = this->txn_id % indexSize;
	PBFTPreMessage premsg = preStore[relIndex];
	if(premsg.txn_id == this->txn_id) {
		this->index = premsg.index;
		this->hash = premsg.hash;
		this->hashSize = premsg.hashSize;
#if RBFT_ON
        } else if(txn->instance_id != (int)g_master_instance) {
       		this->index = this->txn_id;
        	this->hash = "";
        	this->hashSize = 0;
#endif
	} else {
		assert(0);
	}
	
	this->return_node = g_node_id;

#if CONSENSUS == PBFT
  #if BATCH_ENABLE == BSET
	this->batch_size = g_batch_size; //premsg.batch_size;
	this->end_index = this->index;
	this->index = (this->index + 1) - this->batch_size;
  #endif
#endif

	this->sign();
}

void PBFTPrepMessage::copy_to_txn(TxnManager * txn) {
  Message::mcopy_to_txn(txn);
}

void PBFTPrepMessage::copy_from_buf(char * buf) {
	
	Message::mcopy_from_buf(buf);
  
	uint64_t ptr = Message::mget_size();

	COPY_VAL(view,buf,ptr);
	COPY_VAL(index,buf,ptr); 
	COPY_VAL(hashSize,buf,ptr); 
  
	ptr = buf_to_string(buf, ptr, hash, hashSize);
	
	COPY_VAL(return_node,buf,ptr); 

#if CONSENSUS == PBFT
  #if BATCH_ENABLE == BSET
 	COPY_VAL(end_index, buf, ptr);
 	// COPY_VAL(batch_idx, buf, ptr);
 	COPY_VAL(batch_size, buf, ptr);
  #endif
#endif
  
	assert(ptr == get_size());
}

void PBFTPrepMessage::copy_to_buf(char * buf) {
	Message::mcopy_to_buf(buf);
  
	uint64_t ptr = Message::mget_size();
  
	COPY_BUF(buf,view,ptr);
	COPY_BUF(buf,index,ptr);
	COPY_BUF(buf,hashSize,ptr);
	
  	char v;
	for(uint64_t i = 0; i < hash.size(); i++){
		v = hash[i];
		COPY_BUF(buf, v, ptr);
	}
  
	COPY_BUF(buf,return_node,ptr);

#if CONSENSUS == PBFT
   #if BATCH_ENABLE == BSET
	COPY_BUF(buf,end_index,ptr);
	// COPY_BUF(buf,batch_idx,ptr);
	COPY_BUF(buf,batch_size,ptr);
   #endif
#endif
	
	assert(ptr == get_size());
}


void PBFTPrepMessage::sign()
{
#if USE_CRYPTO
	string message = this->toString();

	uint64_t stime = get_sys_clock();
	auto signature(RsaSignString(g_priv_key, message));

	uint64_t edtime = get_sys_clock() - stime;

	rsacount++;
	rsatime += edtime;

	if(!flagrsa) {
		minrsa = edtime;
		maxrsa = edtime;
		flagrsa = true;
	} else {
		if(edtime < minrsa) {
			minrsa = edtime;
		}

		if(edtime > maxrsa) {
			maxrsa = edtime;
		}	
	}


	this->signature = signature;
#else
	this->signature = "0";
#endif
	
	this->pubKey = g_pub_keys[g_node_id];
	
	this->sigSize = this->signature.size();
	this->keySize = this->pubKey.size();

}


string PBFTPrepMessage::toString(){
	string signString = std::to_string(this->view) + '_' + 
		std::to_string(this->index) + '_' + this->hash  
		+ '_' + std::to_string(this->return_node);

    #if BATCH_ENABLE == BSET
	  signString += to_string(this->end_index);
    #endif

	return signString;
}

//validate message and add to message log
bool PBFTPrepMessage::addAndValidate()
{
	if(!this->validate()) {
		return false;	
	}

	//make sure message and node have same view
#if !RBFT_ON
	if (this->view != g_view)
	{
  #if VIEW_CHANGES == true
		char * buf = (char*)malloc(this->get_size() + 1);
		this->copy_to_buf(buf);
		Message * deepCopyMsg = Message::create_message(buf);
		g_msg_wait.push_back(deepCopyMsg);
  #else
		assert(0);
  #endif
		return false;
	}
#endif
	
#if BATCH_ENABLE == BUNSET
    	prepPStore[this->index % indexSize][this->return_node] = *this;
#else
  #if RBFT_ON
    	if(this->instance_id == g_master_instance) {
  #endif
    		prepPStore[this->end_index % indexSize][this->return_node] = *this;
  #if RBFT_ON
    	}
  #endif
#endif
	
	return true;
}

//makes sure message is valid, returns true or false;
bool PBFTPrepMessage::validate()
{
#if USE_CRYPTO
	//verifies message signature
	string message = this->toString();
	if (! RsaVerifyString(g_pub_keys[this->return_node], message, this->signature) ) {
		assert(0);
		return false;
	}
	
	//make sure message was signed by the node this message claims it is from
	if (this->pubKey != g_pub_keys[this->return_node]) {
		assert (0);
		return false;
	}
#endif

	return true;
}


/****************************************/

uint64_t PBFTCommitMessage::get_size() {
  uint64_t size = Message::mget_size();
  
  size += sizeof(view);
  size += sizeof(index);
  size += hash.length();
  size += sizeof(hashSize);
  size += sizeof(return_node);

#if BATCH_ENABLE == BSET
    size += sizeof(end_index);
    //size += sizeof(batch_idx);
    size += sizeof(batch_size);
#endif
  
  return size;
}

void PBFTCommitMessage::copy_from_txn(TxnManager * txn) {
	Message::mcopy_from_txn(txn);
	
	this->view = g_view;
	
	uint64_t relIndex = this->txn_id % indexSize;
	PBFTPreMessage premsg = preStore[relIndex];
	if(premsg.txn_id == this->txn_id) {
       		this->index = premsg.index;
       		this->hash = premsg.hash;
       		this->hashSize = premsg.hashSize;
#if RBFT_ON
        } else if(txn->instance_id != (int)g_master_instance) {
        	this->index = this->txn_id;
        	this->hash = "";
        	this->hashSize = 0;
#endif
	} else {
		assert(0);
	}
	
	this->return_node = g_node_id;

#if CONSENSUS == PBFT
   #if BATCH_ENABLE == BSET
	//this->batch_idx = premsg.batch_idx;
	this->batch_size = g_batch_size;
	this->end_index = this->index;
	this->index = (this->index + 1) - this->batch_size;
   #endif
#endif

	this->sign();
}

void PBFTCommitMessage::copy_to_txn(TxnManager * txn) {
	Message::mcopy_to_txn(txn);
}

void PBFTCommitMessage::copy_from_buf(char * buf) {
	Message::mcopy_from_buf(buf);
  
	uint64_t ptr = Message::mget_size();

	COPY_VAL(view,buf,ptr);
	COPY_VAL(index,buf,ptr); 
	COPY_VAL(hashSize,buf,ptr); 
  
	ptr = buf_to_string(buf, ptr, hash, hashSize);
	
	COPY_VAL(return_node,buf,ptr); 

 #if BATCH_ENABLE == BSET
 	COPY_VAL(end_index, buf, ptr);
 	//COPY_VAL(batch_idx, buf, ptr);
 	COPY_VAL(batch_size, buf, ptr);
 #endif
  
	assert(ptr == get_size());
}

void PBFTCommitMessage::copy_to_buf(char * buf) {
	Message::mcopy_to_buf(buf);
    
	uint64_t ptr = Message::mget_size();
  
	COPY_BUF(buf,view,ptr);
	COPY_BUF(buf,index,ptr);
	COPY_BUF(buf,hashSize,ptr);
	
  	char v;
	for(uint64_t i = 0; i < hash.size(); i++){
		v = hash[i];
		COPY_BUF(buf, v, ptr);
	}
  
	COPY_BUF(buf,return_node,ptr);

 #if BATCH_ENABLE == BSET
	COPY_BUF(buf,end_index,ptr);
	//COPY_BUF(buf,batch_idx,ptr);
	COPY_BUF(buf,batch_size,ptr);
 #endif
	
	assert(ptr == get_size());
}

//signs current message

void PBFTCommitMessage::sign() 
{
#if USE_CRYPTO
	string message = std::to_string(this->view) + '_' + 
		std::to_string(this->index) + '_' + 
		this->hash  + '_' + std::to_string(g_node_id);
	
	//cout << "Signing Commit msg: " << message << endl;
	uint64_t stime = get_sys_clock();

	auto signature(RsaSignString(g_priv_key, message));

	uint64_t edtime = get_sys_clock() - stime;
	
	rsacount++;
	rsatime += edtime;

	if(edtime < minrsa) {
		minrsa = edtime;
	}

	if(edtime > maxrsa) {
		maxrsa = edtime;
	}	

	this->signature = signature;
#else
	this->signature = "0";
#endif
	
	this->pubKey = g_pub_keys[g_node_id];
	
	this->sigSize = this->signature.size();
	this->keySize = this->pubKey.size();
}


bool PBFTCommitMessage::addAndValidate() 
{
	if(!this->validate()) {
		return false;	
	}

	//make sure message and node have same view
#if RBFT_ON
    	if (this->instance_id > (uint)g_min_invalid_nodes) {
#else
	if (this->view != g_view) {
#endif

   #if VIEW_CHANGES == true
		char * buf = (char*)malloc(this->get_size() + 1);
		this->copy_to_buf(buf);
		Message * deepCopyMsg = Message::create_message(buf);
		g_msg_wait.push_back(deepCopyMsg);
   #else
		assert(0);
   #endif
		return false;
	}
	
#if BATCH_ENABLE == BUNSET
    	commStore[this->index % indexSize][this->return_node] = *this;
#else
   #if RBFT_ON
        if(this->instance_id == g_master_instance) {
   #endif
    		commStore[this->end_index % indexSize][this->return_node] = *this;
   #if RBFT_ON
    	}
   #endif
#endif

	return true;
}


//makes sure message is valid, returns true or false;
bool PBFTCommitMessage::validate() {
		
	string message = std::to_string(this->view) + '_' + 
		std::to_string(this->index) + '_' + 
		this->hash  + '_' + std::to_string(this->return_node);

#if USE_CRYPTO
	//verify signature of message
	if (! RsaVerifyString(g_pub_keys[this->return_node], message, this->signature) ) {
		assert(0);
		return false;
	}

	//make sure message was signed by the node this message claims it is from
	if (this->pubKey != g_pub_keys[this->return_node]) {
		assert (0);
		return false;
	}
#endif

	return true;
}

/************************************/

//Arrays that stores messages of each type.
//PBFTPrepMessage prepPStore[2 * CLIENT_NODE_CNT * MAX_TXN_IN_FLIGHT][NODE_CNT];
//PBFTCommitMessage commStore[2 * CLIENT_NODE_CNT * MAX_TXN_IN_FLIGHT][NODE_CNT];

vector<vector<PBFTPrepMessage>> prepPStore(2 * CLIENT_NODE_CNT * MAX_TXN_IN_FLIGHT, 
					vector<PBFTPrepMessage>(NODE_CNT));
vector<vector<PBFTCommitMessage>> commStore(2 * CLIENT_NODE_CNT * MAX_TXN_IN_FLIGHT, 
					vector<PBFTCommitMessage>(NODE_CNT));

#endif // PBFT

/************************************/

#if CONSENSUS == PBFT || CONSENSUS == DBFT

#if VIEW_CHANGES == true
void PBFTViewChangeMsg::init() {
	
	this->view = ((g_view + 1) % g_node_cnt);// current primary node id + 1
	this->index = g_last_stable_chkpt;
  
    #if CONSENSUS == PBFT
	//copy 2f checkpoint messages into this message
	for(int i = 0; i < (2 * g_min_invalid_nodes); i++) {
		assert((uint)i < g_pbft_chkpt_msgs.size()); 
		this->checkPointMessages.push_back(g_pbft_chkpt_msgs[i]);
		cout << "Checkpoint index: " << g_pbft_chkpt_msgs[i].index << "\n";
		fflush(stdout);
	}
    #else
	cout << "Checkpoint index: " << g_last_stable_chkpt << "\n";
	int chkcnt = 0;

	DBFTPrepMessage dmsg;
	for(uint i=0; i < g_dbft_chkpt_msgs.size(); i++) {
		dmsg = g_dbft_chkpt_msgs[i];
		if(dmsg.index == this->index) {
			cout << "Found \n";
			cout << "State hash: " << dmsg.tripleSign << "\n";
			fflush(stdout);

			this->checkSizes.push_back(dmsg.tripleSignSize);
			this->checkPointMessages.push_back(dmsg.tripleSign);
			chkcnt++;
		} 

		if(chkcnt == 2 * g_min_invalid_nodes) {
			cout << "Check hash: " << dmsg.tripleSign << "\n";
			break;
		} }
    #endif

	// First adding requests from last checkpoint to end (indexSize).
	uint64_t stIndex = (this->index + 1) % indexSize;
	for(uint i = stIndex; i < indexSize; i++) {
	   uint64_t idx = breqStore[i].index[g_batch_size - 1];
	   if(idx > g_last_stable_chkpt && idx < UINT64_MAX) {
		cout << "Idx greater: " << idx << "\n";
		fflush(stdout);

	      uint64_t relIndex = idx % indexSize;
	      int cntPrep = 0;
	      for(uint j=0; j<g_node_cnt; j++) {
	   #if CONSENSUS == PBFT
	      	if(prepPStore[relIndex][j].end_index == idx) {
		   assert(breqStore[i].view == prepPStore[relIndex][j].view);
		   assert(breqStore[i].hash[g_batch_size - 1] == prepPStore[relIndex][j].hash);
		   cntPrep++;
		} }

	      if(cntPrep >= 2 * g_min_invalid_nodes) {

	   #else // For DBFT
	      	if(prepStore[relIndex][j].index == idx) {
			cout << "Found Prepare: \n";
			fflush(stdout);

		   assert(breqStore[i].view == prepStore[relIndex][j].view);
		   assert(breqStore[i].hash[g_batch_size - 1] == prepStore[relIndex][j].hash);
		   cntPrep++;
		} }
	      if(cntPrep >= 2 * g_min_invalid_nodes + 1) {

	   #endif

		cout << "View Pre: " << breqStore[i].index[g_batch_size - 1] << "\n";
		fflush(stdout);

	   	for(uint j=0; j<g_node_cnt; j++) {
	   #if CONSENSUS == PBFT
	   	   if(prepPStore[relIndex][j].end_index == idx) {
	   	      this->prepareMessages.push_back(prepPStore[relIndex][j]);

		      cout << "View Prep: "<< prepPStore[relIndex][j].end_index << "\n";
		      fflush(stdout);		
	   	   } }
	   #else
	   	   if(prepStore[relIndex][j].index == idx) {
	   	      this->prepareMessages.push_back(prepStore[relIndex][j]);

		      cout << "View Prep: "<< prepStore[relIndex][j].index << "\n";
		      fflush(stdout);		

	   	   } }
	   #endif

	     	this->prePrepareMessages.push_back(breqStore[i]);
	      }      
	   } }


	// Now adding requests from 0 to stIndex. 
	// This split in loop is done to ensure requests are added in sorted manner.
	for(uint i = 0; i < stIndex; i++) {
	   uint64_t idx = breqStore[i].index[g_batch_size - 1];
	   if(idx > g_last_stable_chkpt && idx < UINT64_MAX) {
		cout << "Idx greater: " << idx << "\n";
		fflush(stdout);

	      uint64_t relIndex = idx % indexSize;
	      int cntPrep = 0;
	      for(uint j=0; j<g_node_cnt; j++) {
	#if CONSENSUS == PBFT
	      	if(prepPStore[relIndex][j].end_index == idx) {
		   assert(breqStore[i].view == prepPStore[relIndex][j].view);
		   assert(breqStore[i].hash[g_batch_size - 1] == prepPStore[relIndex][j].hash);
		   cntPrep++;
		} }

	      if(cntPrep >= 2 * g_min_invalid_nodes) {

	#else // For DBFT
	      	if(prepStore[relIndex][j].index == idx) {
			cout << "Found Prepare: \n";
			fflush(stdout);

		   assert(breqStore[i].view == prepStore[relIndex][j].view);
		   assert(breqStore[i].hash[g_batch_size - 1] == prepStore[relIndex][j].hash);
		   cntPrep++;
		} }

	      if(cntPrep >= 2 * g_min_invalid_nodes + 1) {

	#endif

		cout << "View Pre: " << breqStore[i].index[g_batch_size - 1] << "\n";
		fflush(stdout);

	   	for(uint j=0; j<g_node_cnt; j++) {
	#if CONSENSUS == PBFT
	   	   if(prepPStore[relIndex][j].end_index == idx) {
	   	      this->prepareMessages.push_back(prepPStore[relIndex][j]);

		      cout << "View Prep: "<< prepPStore[relIndex][j].end_index << "\n";
		      fflush(stdout);		

	   	   } }
	#else
	   	   if(prepStore[relIndex][j].index == idx) {
	   	      this->prepareMessages.push_back(prepStore[relIndex][j]);

		      cout << "View Prep: "<< prepStore[relIndex][j].index << "\n";
		      fflush(stdout);		

	   	   } }
	#endif

	     	this->prePrepareMessages.push_back(breqStore[i]);
	      }      
	   } }

	this->numPrepMsgs = this->prepareMessages.size();
	this->numPreMsgs = this->prePrepareMessages.size();
	this->return_node = g_node_id;//node that is creating the message right now.
	this->sign();
}

uint64_t PBFTViewChangeMsg::get_size() {
	uint64_t size = Message::mget_size();
  
	size += sizeof(view);
	size += sizeof(index);
	size += sizeof(numPreMsgs);
	size += sizeof(numPrepMsgs);

   #if CONSENSUS == PBFT
	for(uint i = 0; i < checkPointMessages.size(); i++)
		size += checkPointMessages[i].get_size();
   #else
	size += sizeof(uint64_t) * checkSizes.size();
	for(uint i = 0; i < checkPointMessages.size(); i++) {
		size += checkSizes[i];
	}
   #endif
		
	for(uint i = 0; i < prePrepareMessages.size(); i++)
		size += prePrepareMessages[i].get_size();
		
	for(uint i = 0; i < prepareMessages.size(); i++)
		size += prepareMessages[i].get_size();
  
	size += sizeof(return_node);
  
	return size;
}

//unused
void PBFTViewChangeMsg::copy_from_txn(TxnManager * txn) {
  Message::mcopy_from_txn(txn);
  assert(0);
}

//unused
void PBFTViewChangeMsg::copy_to_txn(TxnManager * txn) {
  Message::mcopy_to_txn(txn);
  assert(0);
}

void PBFTViewChangeMsg::copy_from_buf(char * buf) {
	
	Message::mcopy_from_buf(buf);
  
	uint64_t ptr = Message::mget_size();

	COPY_VAL(view,buf,ptr);
	COPY_VAL(index,buf,ptr); 
	COPY_VAL(numPreMsgs,buf,ptr);
	COPY_VAL(numPrepMsgs,buf,ptr); 

   #if CONSENSUS == PBFT
	for(int i = 0; i < (2 * g_min_invalid_nodes); i++) {
		Message * msg = create_message(&buf[ptr]);
		ptr += msg->get_size();
		checkPointMessages.push_back(*((CheckpointMessage*)msg));
	}
   #else
	uint64_t csz;
	for(int i = 0; i < (2 * g_min_invalid_nodes); i++) {
		COPY_VAL(csz,buf,ptr);
		checkSizes.push_back(csz);
	//	cout << "CFB S: " << checkSizes[i] << "\n";
	//	fflush(stdout);
	}


	string chash;
	for(int i = 0; i < (2 * g_min_invalid_nodes); i++) {
		chash = "";
		csz = checkSizes[i];
		ptr = buf_to_string(buf, ptr, chash, csz);
		checkPointMessages.push_back(chash);
	//	cout << "CFB C: " << checkPointMessages[i] << "\n";
	//	fflush(stdout);
	}
   #endif
	
	for(uint i = 0; i < numPreMsgs; i++) {
		Message * msg = create_message(&buf[ptr]);
		ptr += msg->get_size();
		prePrepareMessages.push_back(*((BatchRequests*)msg));
	}
	
	for(uint i = 0; i < numPrepMsgs; i++) {
		Message * msg = create_message(&buf[ptr]);
		ptr += msg->get_size();
	#if CONSENSUS == PBFT
		prepareMessages.push_back(*((PBFTPrepMessage*)msg));
	#else
		prepareMessages.push_back(*((DBFTPrepMessage*)msg));
	#endif
	}
	
	COPY_VAL(return_node,buf,ptr); 
  
	assert(ptr == get_size());
}

void PBFTViewChangeMsg::copy_to_buf(char * buf) {
	Message::mcopy_to_buf(buf);
  
	uint64_t ptr = Message::mget_size();

	COPY_BUF(buf,view,ptr);
	COPY_BUF(buf,index,ptr);
	COPY_BUF(buf,numPreMsgs,ptr);
	COPY_BUF(buf,numPrepMsgs,ptr);

   #if CONSENSUS == PBFT
	assert((uint)((2 * g_min_invalid_nodes)) == checkPointMessages.size());
	
	for(uint i = 0; i < checkPointMessages.size(); i++) {
		checkPointMessages[i].copy_to_buf(&buf[ptr]);
		ptr += checkPointMessages[i].get_size();
	}
   #else
	uint64_t csz;
	for(int i=0; i < (2 * g_min_invalid_nodes); i++) {
		csz = checkSizes[i];
		COPY_BUF(buf,csz,ptr);
	//	cout << "CTB S: " << csz << "\n";
	//	fflush(stdout); 
	}

	char v;
	string chash;	
	for(int j=0; j < (2 * g_min_invalid_nodes); j++) {
		chash = checkPointMessages[j];
		for(uint64_t i = 0; i < checkSizes[j]; i++) {
			v = chash[i];
			COPY_BUF(buf, v, ptr);
		}
	//	cout << "CTB C: " << chash << "\n"; 
	//	fflush(stdout);
	}
   #endif
	
	assert(numPreMsgs == prePrepareMessages.size());
	for(uint i = 0; i < numPreMsgs; i++) {
		prePrepareMessages[i].copy_to_buf(&buf[ptr]);
		ptr += prePrepareMessages[i].get_size();
	}
	
	assert(numPrepMsgs == prepareMessages.size());
	for(uint i = 0; i < numPrepMsgs; i++) {
		prepareMessages[i].copy_to_buf(&buf[ptr]);
		ptr += prepareMessages[i].get_size();
	}
	
	COPY_BUF(buf,return_node,ptr);
	assert(ptr == get_size());
}

void PBFTViewChangeMsg::sign() 
{
#if USE_CRYPTO
	string message = this->toString();
	
	auto signature(RsaSignString(g_priv_key, message));
	this->signature = signature;
#else
	this->signature = "0";
#endif
	
	this->pubKey = g_pub_keys[g_node_id];
	
	this->sigSize = this->signature.size();
	this->keySize = this->pubKey.size();

}
vector<PBFTViewChangeMsg> g_pbft_view_change_msgs;

vector<Message*> g_msg_wait;
// validate message and check to see if it should be added to message log 
// and if we have recieved enough messages
bool PBFTViewChangeMsg::addAndValidate()
{
	if(!this->validate()) {
		return false;
	}
	
	//count number of matching view change messages recieved previously
	uint i;
	for(i = 0; i < g_pbft_view_change_msgs.size(); i++) {
		assert(this->return_node != g_pbft_view_change_msgs[i].return_node);
		assert(this->index == g_pbft_view_change_msgs[i].index);
		assert(this->view == g_pbft_view_change_msgs[i].view);
	}
	
	g_pbft_view_change_msgs.push_back(*this);
	i++;//i now equals number of msgs recieved
	
	if((int)i < (2 * g_min_invalid_nodes)) {
		return false;
	}
	
	return true;
}

//validate message and all messages this message contains
bool PBFTViewChangeMsg::validate() {
	string message = this->toString();
	
#if USE_CRYPTO 
	//verify signature of message
	if (! RsaVerifyString(this->pubKey, message, this->signature) )
	{
		assert(0); 
		return false; 
	}

	//make sure message was signed by the node this message claims it is from
	if (this->pubKey != g_pub_keys[this->return_node])
	{
		assert (0);
		return false;
	}
#endif
	
   #if CONSENSUS == PBFT
	//are all checkpoint messages valid
	for(uint i = 0; i < checkPointMessages.size(); i++) {
		if(!checkPointMessages[i].validate()) {
			assert(0);
			return false;
		} }
   #else
	// All the states same.
	string chash1 = checkPointMessages[0];
//	cout << "Hash1: " << chash1 << "\n";
	for(uint i = 1; i < checkPointMessages.size(); i++) {
//		cout << "Hash2: " << checkPointMessages[i] << "\n";
//		fflush(stdout);
		if(chash1 != checkPointMessages[i]) {
			assert(0);
		} }
   #endif
	
	//are all prepare messages valid
	for(uint i = 0; i < prepareMessages.size(); i++) {
		if(!prepareMessages[i].validate()) {
			assert(0);
			return false;
		} }
	
	// All preprepare messages valid and have enough matching prepare messages
	for(uint i = 0; i < prePrepareMessages.size(); i++) {
		if(!prePrepareMessages[i].validate()) {
			assert(0);
			return false;
		}

		cout << "VPre: " << prePrepareMessages[i].index[g_batch_size - 1] << "\n";
		fflush(stdout); 

		//for every preprepare message, find 2f matching prepare messages
		int k = 0; 

		//set [node] to 1 when prepare msg from node is recorded
		bool recievedFrom[NODE_CNT] = { 0 };
		for(uint j = 0; j < prepareMessages.size(); j++) {
	    #if CONSENSUS == PBFT
		  if(prePrepareMessages[i].index[0] == prepareMessages[j].index) {
		    assert(prePrepareMessages[i].index[g_batch_size - 1] == prepareMessages[j].end_index);
	    #else
		  if(prePrepareMessages[i].index[g_batch_size - 1] == prepareMessages[j].index) {
	    #endif
		    assert(prePrepareMessages[i].view == prepareMessages[j].view);
		    assert(prePrepareMessages[i].hash[g_batch_size - 1] == prepareMessages[j].hash);

		    // Already received a prepare for this preprepare from this node
		    assert(!recievedFrom[prepareMessages[j].return_node]); 
		    recievedFrom[prepareMessages[j].return_node] = true;
		    k++;
		  } }
		assert(k >= 2 * g_min_invalid_nodes);
	}
	
	return true;
}

string PBFTViewChangeMsg::toString() {
	string message;
	message += to_string(view) + '_' + to_string(index) + '_';
	
    #if CONSENSUS == PBFT
	for(uint i = 0; i < checkPointMessages.size(); i++) {
		message += checkPointMessages[i].toString() += '_';
	}
    #endif
	
	for(uint i = 0; i < prePrepareMessages.size(); i++) {
		message += prePrepareMessages[i].toString() + '_';
	}
	
    #if CONSENSUS == PBFT   
	for(uint i = 0; i < prepareMessages.size(); i++) {
		message += prepareMessages[i].toString() + '_';
	}
    #endif 
	
	message += to_string(return_node);
	
	return message;
}


/************************************/

void PBFTNewViewMsg::init(uint64_t thd_id) {
	this->view = g_node_id;
	
	//add view change messages
	for(uint i = 0; i < g_pbft_view_change_msgs.size(); i++) {
		cout << "View MSG: " << g_pbft_view_change_msgs[i].index << "\n";
		fflush(stdout);

		this->viewChangeMessages.push_back(g_pbft_view_change_msgs[i]);
		assert(g_last_stable_chkpt == g_pbft_view_change_msgs[i].index);
	}

	//highest sequence number in prepare messages
	uint max_s = 0;

	//lowest sequence number in view change messages
	uint min_s = viewChangeMessages[0].index; 

	nextSetId = (viewChangeMessages[0].index + 1) / g_batch_size;

	// out of every prePrepare message in every view change message, 
	// find the highest sequence number and latest stable checkpoint.
	PBFTViewChangeMsg vmsg;
	BatchRequests bmsg;
	for(uint i = 0; i < viewChangeMessages.size(); i++) {
		vmsg = viewChangeMessages[i];
		if(vmsg.index > min_s) {
			min_s = vmsg.index;
		}
		
		for(uint j = 0; j < vmsg.prePrepareMessages.size(); j++) {
			bmsg = vmsg.prePrepareMessages[j];
			if(bmsg.index[g_batch_size - 1] > max_s) {
				max_s = bmsg.index[g_batch_size - 1];
			}

			uint64_t some_index = bmsg.index[0] / g_batch_size;
			cout << "some_index = " << bmsg.index[0] << " / " << g_batch_size << endl;
			batchSet[some_index % indexSize] = bmsg.index[0];
			cout << "batchSet[" << some_index << " % " << indexSize << "] = " << bmsg.index[0] << endl;
		} }

	// needs to set new stable checkpoint if false
	assert(min_s <= g_last_stable_chkpt); 
	
	if(min_s <= max_s) {
	  cout << "Min: " << min_s << " :: Max: " << max_s <<"\n";
	  fflush(stdout);
	  
	  //list of sequence numbers that have a preprepare message (stores their view)
	  uint viewNumbers[( max_s - min_s) / g_batch_size] = { 0 };  
	  
	  //tells you whether a message has been found for this index
	  bool messageFound[( max_s - min_s) / g_batch_size] = { 0 }; 
	  
	  assert(prePrepareMessages.size() == 0);
	  
	  // create something to replace when finding a message with higher "view number"
	  for(uint i = 0; i < ( max_s - min_s) / g_batch_size; i++) {
	  	BatchRequests msg = BatchRequests();
	  	prePrepareMessages.push_back(msg);
	  }
	  
	  // find or create preprepare message for every sequence number 
	  // between min_s and max_s inclusive
	  for(uint j = 0; j < viewChangeMessages.size(); j++) {
	    vmsg = viewChangeMessages[j];
	    for(uint k = 0; k < vmsg.prePrepareMessages.size(); k++) {
	      BatchRequests currentMessage = vmsg.prePrepareMessages[k];
	      uint pos = (currentMessage.index[0] / g_batch_size) - (min_s / g_batch_size) - 1;
	      
	      // if a pre-prepare for this index has not been found yet or 
	      // this pre-prepare has a higher "view number" than a previously found.
	      if(!messageFound[pos] || currentMessage.view > viewNumbers[pos]) {
	        TxnManager * local_txn_man = 
	       	txn_table.get_transaction_manager(thd_id,currentMessage.get_txn_id() + 2,0);
	        local_txn_man->cbatch = nextSetId;

	        Message * msg = Message::create_message(local_txn_man, BATCH_REQ);
	        prePrepareMessages[pos] = (*(BatchRequests*)msg);
	        messageFound[pos] = true;
	        viewNumbers[pos] = currentMessage.view; 
	        
	        cout << "NextSetId: " << nextSetId << "\n";
	        fflush(stdout);
	        
	        nextSetId++;
	        } } }
	  
	  for(uint i = 0; i < prePrepareMessages.size(); i++) {
	  	if(!messageFound[i]) {
	  		//not yet bugtested, shouldnt be happening now anyways.
	  		assert(0);
	  	} } }
	  	
	this->numViewChangeMsgs = this->viewChangeMessages.size();
	this->numPreMsgs = this->prePrepareMessages.size();
	
	this->sign();
}

uint64_t PBFTNewViewMsg::get_size() {
	uint64_t size = Message::mget_size();
  
	size += sizeof(view);
	size += sizeof(numViewChangeMsgs);
	size += sizeof(numPreMsgs);

	for(uint i = 0; i < viewChangeMessages.size(); i++)
		size += viewChangeMessages[i].get_size();
		
	for(uint i = 0; i < prePrepareMessages.size(); i++)
		size += prePrepareMessages[i].get_size();
  
	return size;
}

//unused
void PBFTNewViewMsg::copy_from_txn(TxnManager * txn) {
  Message::mcopy_from_txn(txn);
  assert(0);
}

//unused
void PBFTNewViewMsg::copy_to_txn(TxnManager * txn) {
  Message::mcopy_to_txn(txn);
  assert(0);
}

void PBFTNewViewMsg::copy_from_buf(char * buf) {
	
	Message::mcopy_from_buf(buf);
  
	uint64_t ptr = Message::mget_size();

	COPY_VAL(view,buf,ptr);
	COPY_VAL(numViewChangeMsgs,buf,ptr); 
	COPY_VAL(numPreMsgs,buf,ptr);
	
	for(uint i = 0; i < numViewChangeMsgs; i++)
	{
		Message * msg = create_message(&buf[ptr]);
		ptr += msg->get_size();
		viewChangeMessages.push_back(*((PBFTViewChangeMsg*)msg));
	}
	
	for(uint i = 0; i < numPreMsgs; i++)
	{
		Message * msg = create_message(&buf[ptr]);
		ptr += msg->get_size();
		prePrepareMessages.push_back(*((BatchRequests*)msg));
	}
  
	assert(ptr == get_size());
}

void PBFTNewViewMsg::copy_to_buf(char * buf) {
	Message::mcopy_to_buf(buf);
  
	uint64_t ptr = Message::mget_size();
  
	COPY_BUF(buf,view,ptr);
	COPY_BUF(buf,numViewChangeMsgs,ptr);
	COPY_BUF(buf,numPreMsgs,ptr);
	
	assert(viewChangeMessages.size() == numViewChangeMsgs);
	for(uint i = 0; i < viewChangeMessages.size(); i++)
	{
		viewChangeMessages[i].copy_to_buf(&buf[ptr]);
		ptr += viewChangeMessages[i].get_size();
	}
	
	assert(numPreMsgs == prePrepareMessages.size());
	for(uint i = 0; i < numPreMsgs; i++)
	{
		prePrepareMessages[i].copy_to_buf(&buf[ptr]);
		ptr += prePrepareMessages[i].get_size();
	}

	assert(ptr == get_size());
}

string PBFTNewViewMsg::toString()
{
	string message;
	message += this->view;
	message += '_';
	
	for(uint i = 0; i < viewChangeMessages.size(); i++) {
		message += viewChangeMessages[i].toString();
		message += '_';
	}
	
	for(uint i = 0; i < prePrepareMessages.size(); i++) {
		message += prePrepareMessages[i].toString();
		message += '_';
	}
	
	return message;
}

void PBFTNewViewMsg::sign()
{
#if USE_CRYPTO
	string message = this->toString();

	auto signature(RsaSignString(g_priv_key, message));
	this->signature = signature;
#else
	this->signature = "0";
#endif
	
	this->pubKey = g_pub_keys[g_node_id];
	
	this->sigSize = this->signature.size();
	this->keySize = this->pubKey.size();

}

bool PBFTNewViewMsg::validate()
{
	string message = this->toString();
	
#if USE_CRYPTO
	//verify signature of message
	if (! RsaVerifyString(this->pubKey, message, this->signature) ) {
		assert(0);
		return false;
	}


	//make sure message was signed by the node this message claims it is from
	if (this->pubKey != g_pub_keys[this->view]) {
		assert (0);
		return false;
	}
#endif
	//make sure all view change messages are valid
	for(uint i = 0; i < viewChangeMessages.size(); i++) {
		if(!viewChangeMessages[i].validate()) {
			return false;
		}
		
		if(viewChangeMessages[i].view != this->view) {
			assert(0);
			return false;
		} }
	
	for(uint i = 0; i < prePrepareMessages.size(); i++) {
		if(!prePrepareMessages[i].validate()) {
			return false;
		} }

	//highest sequence number in prepare messages
	uint max_s = 0;

	//lowest sequence number in view change messages
	uint min_s = viewChangeMessages[0].index; 

	// from every prepare message in each view change message, 
	// find the highest sequence number and last stable checkpoint.
	for(uint i = 0; i < viewChangeMessages.size(); i++) {
		if(viewChangeMessages[i].index < min_s) {
			min_s = viewChangeMessages[i].index;
		}
		
		for(uint j = 0; j < viewChangeMessages[i].prepareMessages.size(); j++) {
		   #if CONSENSUS == PBFT
			if(viewChangeMessages[i].prepareMessages[j].end_index > max_s){
				max_s= viewChangeMessages[i].prepareMessages[j].end_index;
			}
		   #else
			if(viewChangeMessages[i].prepareMessages[j].index > max_s) {
				max_s= viewChangeMessages[i].prepareMessages[j].index;
			}
		   #endif
		} }

	#if CONSENSUS == DBFT
		if( max_s >= min_s) {
			g_next_index = max_s + 1;
		} else {
			g_next_index = min_s + 1;
		}
	#endif

	if(min_s <= max_s) {
	  uint viewNumbers[( max_s - min_s) / g_batch_size] = { 0 };  
	  bool messageFound[( max_s - min_s) / g_batch_size] = { 0 }; 
	  
	  vector<BatchRequests> compareMessages;
	  for(uint i = 0; i < ( max_s - min_s) / g_batch_size; i++) {
	  	BatchRequests msg = BatchRequests();
	  	compareMessages.push_back(msg);
	  }
	  assert(compareMessages.size() == prePrepareMessages.size());
	  
	  // mirror method used to create pre prepare messages in new view message, 
	  // compare to see if they were created correctly
	  for(uint j = 0; j < viewChangeMessages.size(); j++) {
	  	for(uint k = 0; k < viewChangeMessages[j].prePrepareMessages.size(); k++) {
	  		BatchRequests currentMessage = viewChangeMessages[j].prePrepareMessages[k];
	  		uint pos = (currentMessage.index[0] / BATCH_SIZE) - (min_s / BATCH_SIZE) - 1;
	  		
	  		// if a pre-prepare for this index has not been found yet or 
	  		// if this pre-prepare has a higher "view number" than a previously found message
	  		if(!messageFound[pos] || currentMessage.view >= viewNumbers[pos])
	  		{
	  			compareMessages[pos].hash[BATCH_SIZE-1] = currentMessage.hash[BATCH_SIZE-1];//only need hash to compare
	  			messageFound[pos] = true;
	  			viewNumbers[pos] = currentMessage.view;
	  		}
	  	}
	  }
	  for(uint i = 0; i < prePrepareMessages.size(); i++) {
	  	if(prePrepareMessages[i].hash[BATCH_SIZE-1] != compareMessages[i].hash[BATCH_SIZE-1]) {
	  		assert(0);
	  		return false;
	  	}
	  } }
	return true;
}
	
#endif // view changes

#endif // PBFT or DBFT


/************************************/

#if CONSENSUS == DBFT

uint64_t DBFTPrepMessage::get_size() {
  uint64_t size = Message::mget_size();
  
  size += sizeof(view);
  size += sizeof(index);
  size += hash.length();
  size += sizeof(hashSize);
  size += sizeof(return_node);

  //Triple
 // size += istate.length();
 // size += sizeof(istateSize);
 // size += sizeof(tid);
 // size += fstate.length();
 // size += sizeof(fstateSize);
  size += tripleSign.length();
  size += sizeof(tripleSignSize);

  return size;
}


void DBFTPrepMessage::copy_from_txn(TxnManager * txn) {
#if RBFT_ON
	// If not a primary
	if((int)g_instance_id != txn->instance_id) {
	    this->txn_id = txn->get_txn_id() - 6;
#else
    	if(g_node_id != local_view[txn->get_thd_id()]) {
    	    Message::mcopy_from_txn(txn);
#endif
    	} else {
#if RBFT_ON
            this->txn_id = txn->get_txn_id() - 3;
#else
	    this->txn_id = txn->get_txn_id() + 3;
#endif
    	}

#if RBFT_ON
    	this->instance_id = txn->instance_id;
#endif

	this->view = local_view[txn->get_thd_id()];
	
	uint64_t relIndex = this->txn_id % indexSize;
	PBFTPreMessage premsg = preStore[relIndex];
	if(premsg.txn_id == this->txn_id) {
            this->index = premsg.index;
            this->hash = premsg.hash;
            this->hashSize = premsg.hashSize;
#if RBFT_ON
        } else if(this->instance_id != g_master_instance) {
            this->index = this->txn_id;
            this->hash = "";
            this->hashSize = 0;
#endif
	} else {
	    cout << "relIndex: " << relIndex << "premsg txn id: " << premsg.txn_id << endl;
	    fflush(stdout);
	    assert(0);
	}
	//cout << "creating dbft prep, hash: " << this->hash << endl;
	//fflush(stdout);
	
	this->return_node = g_node_id;

//	this->istate 		= txn->istate;
//	this->istateSize 	= txn->istate.length();
//	this->tid 		= txn->tid;
//	this->fstate 		= txn->fstate;
//	this->fstateSize 	= txn->fstate.length();
	this->tripleSign 	= txn->tripleSign;
	this->tripleSignSize 	= txn->tripleSign.length();
	
	this->sign();
}

void DBFTPrepMessage::copy_to_txn(TxnManager * txn) {
  Message::mcopy_to_txn(txn);
}

void DBFTPrepMessage::copy_from_buf(char * buf) {
    Message::mcopy_from_buf(buf);

    uint64_t ptr = Message::mget_size();

    COPY_VAL(view,buf,ptr);
    COPY_VAL(index,buf,ptr);
    COPY_VAL(hashSize,buf,ptr);

    ptr = buf_to_string(buf, ptr, hash, hashSize);

    COPY_VAL(return_node,buf,ptr);

    // DBFT Triple
//	COPY_VAL(istateSize,buf,ptr);
//	ptr = buf_to_string(buf, ptr, istate, istateSize);
//
//	COPY_VAL(tid,buf,ptr);
//
//	COPY_VAL(fstateSize,buf,ptr);
//	ptr = buf_to_string(buf, ptr, fstate, fstateSize);

    COPY_VAL(tripleSignSize,buf,ptr);
    ptr = buf_to_string(buf, ptr, tripleSign, tripleSignSize);

    assert(ptr == get_size());
}

void DBFTPrepMessage::copy_to_buf(char * buf) {
    Message::mcopy_to_buf(buf);

    uint64_t ptr = Message::mget_size();

    COPY_BUF(buf,view,ptr);
    COPY_BUF(buf,index,ptr);
    COPY_BUF(buf,hashSize,ptr);

      char v;
    for(uint64_t i = 0; i < hash.size(); i++){
        v = hash[i];
        COPY_BUF(buf, v, ptr);
    }

    COPY_BUF(buf,return_node,ptr);

    // DBFT Triple
//	COPY_BUF(buf,istateSize,ptr);
//	for(uint64_t i = 0; i < istate.size(); i++){
//		v = istate[i];
//		COPY_BUF(buf, v, ptr);
//	}
//
//	COPY_BUF(buf,tid,ptr);
//
//	COPY_BUF(buf,fstateSize,ptr);
//	for(uint64_t i = 0; i < fstate.size(); i++){
//		v = fstate[i];
//		COPY_BUF(buf, v, ptr);
//	}

    COPY_BUF(buf,tripleSignSize,ptr);
    for(uint64_t i = 0; i < tripleSign.size(); i++) {
        v = tripleSign[i];
        COPY_BUF(buf, v, ptr);
    }

    assert(ptr == get_size());
}

void DBFTPrepMessage::sign()
{
#if USE_CRYPTO
	// We are not signing the tripleSign field.
	string message = std::to_string(this->view) + '_' + this->hash + '_' + this->tripleSign;

	//	cout << "Sign message: " << message << "\n \n";
	//	fflush(stdout);

	uint64_t stime = get_sys_clock();

	auto signature(RsaSignString(g_priv_key, message));

	uint64_t edtime = get_sys_clock() - stime;

	rsacount++;
	rsatime += edtime;

	if(!flagrsa) {
		minrsa = edtime;
		maxrsa = edtime;
		flagrsa = true;
	} else {
		if(edtime < minrsa) {
			minrsa = edtime;
		}

		if(edtime > maxrsa) {
			maxrsa = edtime;
		}	
	}

//	cout << "Signature: " << signature << "\n";
//	fflush(stdout);

    	this->signature = signature;
#else
    	this->signature = "0";
#endif

    	this->pubKey = g_pub_keys[g_node_id];

    	this->sigSize = this->signature.size();
    	this->keySize = this->pubKey.size();
}

// Makes sure message is valid, returs true or false;
// We don't validate the triple here.
bool DBFTPrepMessage::validate()
{
#if USE_CRYPTO
	//is signature valid
	string message = std::to_string(this->view) + '_' + this->hash + '_' + this->tripleSign;

//	cout << "Verify message: " << message << "\n \n";
//	fflush(stdout);
//
//	cout << "This Signature: " << this->signature << "\n";
//	fflush(stdout);

    	//verifies message signature
    	if (! RsaVerifyString(this->pubKey, message, this->signature) ) {
    	    assert(0);
    	    return false;
    	}
#endif

//	//make sure message and node have same view
//	if (this->view != g_view) {
//		assert(0);
//		return false;
//	}

#if USE_CRYPTO
    	//make sure message was signed by the node this message claims it is from
    	if (this->pubKey != g_pub_keys[this->return_node]) {
    	    assert (0);
    	    return false;
    	}
#endif

    return true;
}

/***************************************/

uint64_t PrimaryPrepMessage::get_size() {
  uint64_t size = Message::mget_size();
  return size;
}

void PrimaryPrepMessage::copy_from_txn(TxnManager * txn) {
    	// Setting txn_id 3 less than the actual.
#if RBFT_ON
    	if(txn->prop_rsp_cnt == (int)(g_min_invalid_nodes + 1))
    		this->txn_id = txn->get_txn_id() + 2;
    	else
#endif
    		this->txn_id = txn->get_txn_id() - 3;
}

void PrimaryPrepMessage::copy_to_txn(TxnManager * txn) {
  Message::mcopy_to_txn(txn);
}

void PrimaryPrepMessage::copy_from_buf(char * buf) {
    Message::mcopy_from_buf(buf);
    //assert(ptr == get_size());
}

void PrimaryPrepMessage::copy_to_buf(char * buf) {
    Message::mcopy_to_buf(buf);
     //assert(ptr == get_size());
}

/***************************************/


// Arrays.
DBFTPrepMessage prepStore[2 * CLIENT_NODE_CNT * MAX_TXN_IN_FLIGHT][NODE_CNT];
vector<DBFTPrepMessage> g_dbft_chkpt_msgs;
#endif


#if ZYZZYVA == true && LOCAL_FAULT
vector<vector<ClientCertificateAck>> clackStore(2 * CLIENT_NODE_CNT * MAX_TXN_IN_FLIGHT, vector<ClientCertificateAck>(NODE_CNT));
//ClientQueryBatch timeoutQry[2 * CLIENT_NODE_CNT * MAX_TXN_IN_FLIGHT];
std::map<uint64_t, ClientQueryBatch> timeoutQry;
std::map<uint64_t, uint64_t> cltmap;
#endif


