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

#ifndef _MESSAGE_H_
#define _MESSAGE_H_

#include "global.h"
#include "helper.h"
//#include "logger.h"
#include "array.h"

#define ALGORAND_SEED_SIZE 20
#define ALGORAND_HASH_SIZE 20
#define ALGORAND_PROOF_SIZE 256
#define ALGORAND_VALUE_SIZE 64
#define ALGORAND_PK_SIZE 426

class ycsb_request;
class LogRecord;
struct Item_no;

class Message {
public:
  virtual ~Message(){}
  static Message * create_message(char * buf); 
  static Message * create_message(BaseQuery * query, RemReqType rtype); 
  static Message * create_message(TxnManager * txn, RemReqType rtype); 
  static Message * create_message(uint64_t txn_id, RemReqType rtype); 
  static Message * create_message(uint64_t txn_id,uint64_t batch_id, RemReqType rtype); 
  static Message * create_message(LogRecord * record, RemReqType rtype); 
  static Message * create_message(RemReqType rtype); 
  static std::vector<Message*> * create_messages(char * buf); 
  static void release_message(Message * msg); 
  RemReqType rtype;
  uint64_t txn_id;
  uint64_t batch_id;
  uint64_t return_node_id;

  uint64_t wq_time;
  uint64_t mq_time;
  uint64_t ntwk_time;

#if CONSENSUS == PBFT || CONSENSUS == DBFT
  //signature is 768 chars, pubkey is 840
  uint64_t sigSize = 1;
  uint64_t keySize = 1;
  string signature = "0";
  string pubKey = "0";

#if ALGORAND == true
//the resources for Algorand
  dataPackage dp;
#endif

  static uint64_t string_to_buf(char * buf,uint64_t ptr, string str);
  static uint64_t buf_to_string(char * buf, uint64_t ptr, string& str, uint64_t strSize);

#if RBFT_ON
  uint64_t instance_id;
#endif
#endif


  // Collect other stats
  double lat_work_queue_time;
  double lat_msg_queue_time;
  double lat_cc_block_time;
  double lat_cc_time;
  double lat_process_time;
  double lat_network_time;
  double lat_other_time;

  uint64_t mget_size();
  uint64_t get_txn_id() {return txn_id;}
  uint64_t get_batch_id() {return batch_id;}
  uint64_t get_return_id() {return return_node_id;}
  void mcopy_from_buf(char * buf);
  void mcopy_to_buf(char * buf);
  void mcopy_from_txn(TxnManager * txn);
  void mcopy_to_txn(TxnManager * txn);
  RemReqType get_rtype() {return rtype;}

  virtual uint64_t get_size() = 0;
  virtual void copy_from_buf(char * buf) = 0;
  virtual void copy_to_buf(char * buf) = 0;
  virtual void copy_to_txn(TxnManager * txn) = 0;
  virtual void copy_from_txn(TxnManager * txn) = 0;
  virtual void init() = 0;
  virtual void release() = 0;
};

// Message types
class InitDoneMessage : public Message {
public:
  void copy_from_buf(char * buf);
  void copy_to_buf(char * buf);
  void copy_from_txn(TxnManager * txn);
  void copy_to_txn(TxnManager * txn);
  uint64_t get_size();
  void init() {}
  void release() {}
};


class KeyExchange : public Message {
public:
  void copy_from_buf(char * buf);
  void copy_to_buf(char * buf);
  void copy_from_txn(TxnManager * txn);
  void copy_to_txn(TxnManager * txn);
  uint64_t get_size();
  void init() {}
  void release() {}

  string pkey;
  uint64_t pkeySz;
  uint64_t return_node;
};


class ReadyServer : public Message {
public:
  void copy_from_buf(char * buf);
  void copy_to_buf(char * buf);
  void copy_from_txn(TxnManager * txn);
  void copy_to_txn(TxnManager * txn);
  uint64_t get_size();
  void init() {}
  void release() {}
};

class FinishMessage : public Message {
public:
  void copy_from_buf(char * buf);
  void copy_to_buf(char * buf);
  void copy_from_txn(TxnManager * txn);
  void copy_to_txn(TxnManager * txn);
  uint64_t get_size();
  void init() {}
  void release() {}
  bool is_abort() { return rc == Abort;}

  uint64_t pid;
  RC rc;
  //uint64_t txn_id;
  //uint64_t batch_id;
  bool readonly;
};

class QueryResponseMessage : public Message {
public:
  void copy_from_buf(char * buf);
  void copy_to_buf(char * buf);
  void copy_from_txn(TxnManager * txn);
  void copy_to_txn(TxnManager * txn);
  uint64_t get_size();
  void init() {}
  void release() {}

  RC rc;
  uint64_t pid;

};

class AckMessage : public Message {
public:
  void copy_from_buf(char * buf);
  void copy_to_buf(char * buf);
  void copy_from_txn(TxnManager * txn);
  void copy_to_txn(TxnManager * txn);
  uint64_t get_size();
  void init() {}
  void release() {}

  RC rc;
#if CC_ALG == MAAT
  uint64_t lower;
  uint64_t upper;
#endif

  // For Calvin PPS: part keys from secondary lookup for sequencer response
  Array<uint64_t> part_keys;
};

class PrepareMessage : public Message {
public:
  void copy_from_buf(char * buf);
  void copy_to_buf(char * buf);
  void copy_from_txn(TxnManager * txn);
  void copy_to_txn(TxnManager * txn);
  uint64_t get_size();
  void init() {}
  void release() {}

  uint64_t pid;
  RC rc;
  uint64_t txn_id;
};

class ForwardMessage : public Message {
public:
  void copy_from_buf(char * buf);
  void copy_to_buf(char * buf);
  void copy_from_txn(TxnManager * txn);
  void copy_to_txn(TxnManager * txn);
  uint64_t get_size();
  void init() {}
  void release() {}

  RC rc;
#if WORKLOAD == TPCC
	uint64_t o_id;
#endif
};


class DoneMessage : public Message {
public:
  void copy_from_buf(char * buf);
  void copy_to_buf(char * buf);
  void copy_from_txn(TxnManager * txn);
  void copy_to_txn(TxnManager * txn);
  uint64_t get_size();
  void init() {}
  void release() {}
  uint64_t batch_id;
};

class ClientQueryMessage : public Message {
public:
  void copy_from_buf(char * buf);
  void copy_to_buf(char * buf);
  void copy_from_query(BaseQuery * query); 
  void copy_from_txn(TxnManager * txn);
  void copy_to_txn(TxnManager * txn);
  uint64_t get_size();
  void init();
  void release();

  uint64_t pid;
  uint64_t ts;
#if CC_ALG == CALVIN
  uint64_t batch_id;
  uint64_t txn_id;
#endif
  uint64_t client_startts;
  uint64_t first_startts;
  Array<uint64_t> partitions;
};

class YCSBClientQueryMessage : public ClientQueryMessage {
public:
  void copy_from_buf(char * buf);
  void copy_to_buf(char * buf);
  void copy_from_query(BaseQuery * query);
  void copy_from_txn(TxnManager * txn);
  void copy_to_txn(TxnManager * txn);
  uint64_t get_size();
  void init(); 
  void release(); 

#if CONSENSUS == PBFT || CONSENSUS == DBFT
  uint64_t return_node;  // node that send this message.

  string getString();
  string getRequestString();
#endif

  Array<ycsb_request*> requests;

};


class ClientResponseMessage : public Message {
public:
  void copy_from_buf(char * buf);
  void copy_to_buf(char * buf);
  void copy_from_txn(TxnManager * txn);
  void copy_to_txn(TxnManager * txn);
  uint64_t get_size();
  void init() {}
  void release();

  RC rc;

#if CLIENT_RESPONSE_BATCH == true
  Array<uint64_t> index;
  Array<uint64_t> client_ts;  
#else
  uint64_t client_startts;
#endif

#if CONSENSUS == PBFT || CONSENSUS == DBFT 
  uint64_t view;// primary node id

  void sign();
  bool validate();
#endif

#if ZYZZYVA == true
  YCSBClientQueryMessage clreq;	// Request from primary
  string stateHash;
  uint64_t stateHashSize;
#endif
};



#if ZYZZYVA == true
class ClientCommitCertificate : public Message {
public:
  void copy_from_buf(char * buf);
  void copy_to_buf(char * buf);
  void copy_from_txn(TxnManager * txn);
  void copy_to_txn(TxnManager * txn);
  uint64_t get_size();
  void init() {}
  void release() {}

  RC rc;
  uint64_t client_startts;
  uint64_t view;

  void sign();
  bool validate();
};

class ClientCertificateAck : public Message {
public:
  void copy_from_buf(char * buf);
  void copy_to_buf(char * buf);
  void copy_from_txn(TxnManager * txn);
  void copy_to_txn(TxnManager * txn);
  uint64_t get_size();
  void init() {}
  void release() {}

  uint64_t view;
  uint64_t client_startts;
  void sign();
  bool validate();
};
#endif



#if CLIENT_BATCH
class ClientQueryBatch : public Message {
public:
  void copy_from_buf(char * buf);
  void copy_to_buf(char * buf);
  void copy_from_txn(TxnManager * txn);
  void copy_to_txn(TxnManager * txn);
  uint64_t get_size();
  void init(); 
  void release(); 

  void sign(); 
  bool validate();
  string getString();

  uint64_t return_node;  
  uint batch_size;
  vector<YCSBClientQueryMessage> cqrySet;
  //YCSBClientQueryMessage cqrySet[BATCH_SIZE];
};
#endif


class QueryMessage : public Message {
public:
  void copy_from_buf(char * buf);
  void copy_to_buf(char * buf);
  void copy_from_txn(TxnManager * txn);
  void copy_to_txn(TxnManager * txn);
  uint64_t get_size();
  void init() {}
  void release() {}

  uint64_t pid;
#if CC_ALG == WAIT_DIE || CC_ALG == TIMESTAMP || CC_ALG == MVCC
  uint64_t ts;
#endif
#if CC_ALG == MVCC 
  uint64_t thd_id;
#elif CC_ALG == OCC 
  uint64_t start_ts;
#endif
#if MODE==QRY_ONLY_MODE
  uint64_t max_access;
#endif
};

class YCSBQueryMessage : public QueryMessage {
public:
  void copy_from_buf(char * buf);
  void copy_to_buf(char * buf);
  void copy_from_txn(TxnManager * txn);
  void copy_to_txn(TxnManager * txn);
  uint64_t get_size();
  void init();
  void release(); 

 Array<ycsb_request*> requests;

};


/***********************************/

#if CONSENSUS == PBFT || CONSENSUS == DBFT

#if BATCH_ENABLE == BSET
#if RBFT_ON
class PropagateBatch : public Message {
public:
  void copy_from_buf(char * buf);
  void copy_to_buf(char * buf);
  void copy_from_txn(TxnManager * txn);
  void copy_to_txn(TxnManager * txn);
  uint64_t get_size();
  void init() {}
  void release() {}

  void sign();
  bool validate();

  string hash[BATCH_SIZE];
  uint64_t hashSize[BATCH_SIZE];
  YCSBClientQueryMessage requestMsg[BATCH_SIZE];
  uint32_t batch_size;
};
#endif

class BatchRequests : public Message {
public:
  void copy_from_buf(char * buf);
  void copy_to_buf(char * buf);
  void copy_from_txn(TxnManager * txn);
  void copy_to_txn(TxnManager * txn);
  uint64_t get_size();
  void init() {}
  void release(); 

  void sign();
  bool validate();
  bool addAndValidate();
  string toString();
  
  uint64_t view; 	// primary node id

  Array<uint64_t> index; 	
  Array<uint64_t> hashSize;	
  vector<string> hash;
  vector<YCSBClientQueryMessage> requestMsg;
  //YCSBClientQueryMessage requestMsg[BATCH_SIZE];
  uint32_t batch_size;

#if CONSENSUS == DBFT
  void stateUpdate(uint64_t txn_id);
#endif

#if ZYZZYVA == true
  // For storing hash of the state.
  vector<uint64_t> stateHashSize;
  vector<string> stateHash;
#endif

};

#if VIEW_CHANGES == true
extern BatchRequests breqStore[2 * CLIENT_NODE_CNT * MAX_TXN_IN_FLIGHT];
//requests that were redirected to primary (may need to execute if primary changes)
extern ClientQueryBatch g_redirected_requests[3 * CLIENT_NODE_CNT * MAX_TXN_IN_FLIGHT];
#endif

#endif	// Batch Set

class PBFTPreMessage : public Message {
public:
  void copy_from_buf(char * buf);
  void copy_to_buf(char * buf);
  void copy_from_txn(TxnManager * txn);
  void copy_to_txn(TxnManager * txn);
  uint64_t get_size();
  void init() {}
  void release() {} 

  //void sign();
  bool validate();
  string toString();
  bool addAndValidate();
  
  uint64_t view; 	// primary node id

  uint64_t index; 	// position in sequence of requests
  string hash;		//request message digest
  uint64_t hashSize;	//size of hash (for removing from buf)
  YCSBClientQueryMessage requestMsg;//client's request message

#if ZYZZYVA == true
  // For storing hash of the state.
  string stateHash;
#endif

};

class ExecuteMessage : public Message {
public:
  void copy_from_buf(char * buf);
  void copy_to_buf(char * buf);
  void copy_from_txn(TxnManager * txn);
  void copy_to_txn(TxnManager * txn);
  uint64_t get_size();
  void init() {}
  void release() {}
  
  uint64_t view; // primary node id
  uint64_t index; // position in sequence of requests
  string hash; //request message digest
  uint64_t hashSize;//size of hash (for removing from buf)
  uint64_t return_node;//id of node that sent this message

  #if BATCH_ENABLE == BSET
    uint64_t end_index;
    uint64_t batch_size;
  #endif

};


class CheckpointMessage : public Message {
public:
  void copy_from_buf(char * buf);
  void copy_to_buf(char * buf);
  void copy_from_txn(TxnManager * txn);
  void copy_to_txn(TxnManager * txn);
  uint64_t get_size();
  void init() {}
  void release() {}
  void sign();
  bool validate();
  string toString();
  bool addAndValidate();
  
  uint64_t index; // sequence number of last request
  uint64_t return_node;//id of node that sent this message

  #if CONSENSUS == PBFT
    uint64_t beg_index;
  #endif
};

//Arrays that stores messages of each type.
//extern YCSBClientQueryMessage cqryStore[2 * CLIENT_NODE_CNT * MAX_TXN_IN_FLIGHT];
//extern PBFTPreMessage preStore[2 * CLIENT_NODE_CNT * MAX_TXN_IN_FLIGHT];
//extern ClientResponseMessage clrspStore[2 * CLIENT_NODE_CNT * MAX_TXN_IN_FLIGHT][NODE_CNT];

extern vector<YCSBClientQueryMessage> cqryStore;
extern vector<PBFTPreMessage> preStore;
extern vector<vector<ClientResponseMessage>> clrspStore;
extern vector<CheckpointMessage> g_pbft_chkpt_msgs;

#if LOCAL_FAULT == true && ZYZZYVA == true
extern vector<vector<ClientCertificateAck>> clackStore;
extern std::map<uint64_t, ClientQueryBatch> timeoutQry;
extern std::map<uint64_t, uint64_t> cltmap;
#endif


#endif	// Either PBFT or DBFT


/* PBFT SPECIFIC MESSAGE TYPES */

#if CONSENSUS == PBFT

class PBFTPrepMessage : public Message {
public:
  void copy_from_buf(char * buf);
  void copy_to_buf(char * buf);
  void copy_from_txn(TxnManager * txn);
  void copy_to_txn(TxnManager * txn);
  uint64_t get_size();
  void init() {}
  void release() {}
  void sign();
  bool validate();
  string toString();
  bool addAndValidate();
  
  uint64_t view; // primary node id
  uint64_t index; // position in sequence of requests
  string hash;//request message digest
  uint64_t hashSize;//size of hash (for removing from buf)
  uint64_t return_node;//id of node that sent this message

  #if BATCH_ENABLE == BSET
    uint64_t end_index;
    uint32_t batch_size;
  #endif

};


class PBFTCommitMessage : public Message {
public:
  void copy_from_buf(char * buf);
  void copy_to_buf(char * buf);
  void copy_from_txn(TxnManager * txn);
  void copy_to_txn(TxnManager * txn);
  uint64_t get_size();
  void init() {}
  void release() {}
  void sign();
  bool validate();
  bool addAndValidate();
  
  uint64_t view; // primary node id
  uint64_t index; // position in sequence of requests
  string hash; //request message digest
  uint64_t hashSize;//size of hash (for removing from buf)
  uint64_t return_node;//id of node that sent this message

  #if BATCH_ENABLE == BSET
    uint64_t end_index;
    uint64_t batch_size;
  #endif

};


//extern PBFTPrepMessage prepPStore[2 * CLIENT_NODE_CNT * MAX_TXN_IN_FLIGHT][NODE_CNT];
//extern PBFTCommitMessage commStore[2 * CLIENT_NODE_CNT * MAX_TXN_IN_FLIGHT][NODE_CNT];

extern vector<vector<PBFTPrepMessage>> prepPStore;
extern vector<vector<PBFTCommitMessage>> commStore;

#endif // PBFT endif


#if CONSENSUS == DBFT
class DBFTPrepMessage : public Message {
public:
  void copy_from_buf(char * buf);
  void copy_to_buf(char * buf);
  void copy_from_txn(TxnManager * txn);
  void copy_to_txn(TxnManager * txn);
  uint64_t get_size();
  void init() {}
  void release() {}
  void sign();
  bool validate();
  string toString();
  
  uint64_t view; // primary node id
  uint64_t index; // position in sequence of requests
  string hash;//request message digest
  uint64_t hashSize;//size of hash (for removing from buf)
  uint64_t return_node;//id of node that sent this message

  // DBFT Triple
//  string istate;
//  uint64_t istateSize;
//  uint64_t tid;
//  string fstate;
//  uint64_t fstateSize;
  string tripleSign;
  uint64_t tripleSignSize;
};


class PrimaryPrepMessage : public Message {
public:
  void copy_from_buf(char * buf);
  void copy_to_buf(char * buf);
  void copy_from_txn(TxnManager * txn);
  void copy_to_txn(TxnManager * txn);
  uint64_t get_size();
  void init() {}
  void release() {}
};



extern DBFTPrepMessage prepStore[2 * CLIENT_NODE_CNT * MAX_TXN_IN_FLIGHT][NODE_CNT];
extern vector<DBFTPrepMessage> g_dbft_chkpt_msgs;
#endif

#if CONSENSUS == PBFT || CONSENSUS == DBFT
#if VIEW_CHANGES == true
class PBFTViewChangeMsg : public Message {
public:
  void copy_from_buf(char * buf);
  void copy_to_buf(char * buf);
  void copy_from_txn(TxnManager * txn);
  void copy_to_txn(TxnManager * txn);
  uint64_t get_size();
  void init();
  void release() {}
  void sign();
  bool validate();
  string toString();
  bool addAndValidate();
  
  uint64_t view; // proposed view (v + 1)
  uint64_t index; //index of last stable checkpoint
  uint64_t numPreMsgs;
  uint64_t numPrepMsgs;
  
  vector<BatchRequests> prePrepareMessages;

#if CONSENSUS == PBFT
  vector<CheckpointMessage> checkPointMessages;
  vector<PBFTPrepMessage> prepareMessages;
#else
  vector <uint64_t> checkSizes;
  vector <string> checkPointMessages;
  vector<DBFTPrepMessage> prepareMessages;
#endif

  uint64_t return_node;//id of node that sent this message

};

class PBFTNewViewMsg : public Message {
public:
  void copy_from_buf(char * buf);
  void copy_to_buf(char * buf);
  void copy_from_txn(TxnManager * txn);
  void copy_to_txn(TxnManager * txn);
  uint64_t get_size();
  void init() {assert(0);}//unused
  void init(uint64_t thd_id);
  void release() {}
  void sign();
  bool validate();
  string toString();
  
  uint64_t view; // proposed view (v + 1)
  uint64_t numViewChangeMsgs;
  uint64_t numPreMsgs;

  vector<PBFTViewChangeMsg> viewChangeMessages;
  
  vector<BatchRequests> prePrepareMessages;

};


/****************************************/

extern vector<PBFTViewChangeMsg> g_pbft_view_change_msgs;
extern vector<Message*> g_msg_wait;

#endif // View Change
#endif // PBFT or DBFT

#endif
