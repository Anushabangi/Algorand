void ClientThread::send_key() {
	// Send everyone the public key and the value
	for(uint64_t i = 0; i < g_node_cnt + g_client_node_cnt; i++) {
		if(i == g_node_id) {
			continue;
		}

		Message *msg = Message::create_message(KEYEX);
		KeyExchange *keyex = (KeyExchange *)msg;
		keyex->pkey = g_public_key;
		keyex->pkeySz = keyex->pkey.size();
		keyex->return_node = g_node_id;
		msg_queue.enqueue(get_thd_id(), keyex, i);
	}
}



void ClientThread::setup() {

  // Increment commonVar.
  batchMTX.lock();
  commonVar++;
  batchMTX.unlock();

  if( _thd_id == 0) {
    while(commonVar < g_client_thread_cnt + g_client_rem_thread_cnt + g_client_send_thread_cnt);

    send_init_done_to_all_nodes();

    send_key();
  }

}

RC ClientThread::run() {

  tsetup();
  printf("Running ClientThread %ld\n",_thd_id);

  while(true) {
     keyMTX.lock();
     if(keyAvail) {
	keyMTX.unlock();
	break;
     }
     keyMTX.unlock();	
  }


  BaseQuery * m_query;
  uint64_t iters = 0;
  uint32_t num_txns_sent = 0;
  int txns_sent[g_node_cnt];
  for (uint32_t i = 0; i < g_node_cnt; ++i)
      txns_sent[i] = 0;

  run_starttime = get_sys_clock();

#if CLIENT_BATCH
  uint addMore = 0;

  // Initializing first batch
  Message *mssg = Message::create_message(CL_BATCH);
  ClientQueryBatch *bmsg = (ClientQueryBatch *)mssg;
  bmsg->init();
#endif

  while(!simulation->is_done()) {
    heartbeat();
    progress_stats();
    int32_t inf_cnt;

    uint32_t next_node = g_view;
    #if !RBFT_ON
    uint32_t next_node_id = g_view;
    #endif

    // Just in case...
    if (iters == UINT64_MAX)
	iters = 0;

    if ((inf_cnt = client_man.inc_inflight(next_node)) < 0) {
    	continue;
    }

    m_query = client_query_queue.get_next_query(_thd_id);
    if(last_send_time > 0) {
      INC_STATS(get_thd_id(),cl_send_intv,get_sys_clock() - last_send_time);
    }
    last_send_time = get_sys_clock();
    assert(m_query);

    Message * msg = Message::create_message((BaseQuery*)m_query,CL_QRY);
    ((ClientQueryMessage*)msg)->client_startts = get_sys_clock();
    // cout << "STS: " << ((ClientQueryMessage*)msg)->client_startts << "\n";
    // fflush(stdout);

    YCSBClientQueryMessage *clqry = (YCSBClientQueryMessage*)msg;
 #if CONSENSUS == PBFT || CONSENSUS == DBFT
    clqry->return_node = g_node_id;
 #endif

    // cout << "INSIDE Req: " << clqry->requests[clqry->requests.size()-1]->key << "\n";
    // fflush(stdout);

    bmsg->cqrySet[addMore] = *clqry;
    addMore++;

    // Resetting and sending the message
    if(addMore == g_batch_size) {
	bmsg->sign();
	
	//broadcast the batch here
	msg_queue.enqueue(get_thd_id(), bmsg, next_node_id);
	num_txns_sent += g_batch_size;
        txns_sent[next_node] += g_batch_size;
        INC_STATS(get_thd_id(),txn_sent_cnt, g_batch_size);

	mssg = Message::create_message(CL_BATCH);
	bmsg = (ClientQueryBatch *)mssg;
	bmsg->init();
	addMore = 0;

    }

  }

#if CONSENSUS == PBFT || CONSENSUS == DBFT
  for (uint64_t l = 0; l < g_node_cnt; ++l)
	//printf("Txns sent to node %lu: %d\n", l, txns_sent[l]);
#else
  for (uint64_t l = 0; l < g_servers_per_client; ++l)
	printf("Txns sent to node %lu: %d\n", l+g_server_start_node, txns_sent[l]);
#endif

  //SET_STATS(get_thd_id(), total_runtime, get_sys_clock() - simulation->run_starttime); 

  printf("FINISH %ld:%ld\n",_node_id,_thd_id);
  fflush(stdout);
  return FINISH;
}

