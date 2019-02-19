//Robert He 2/18/2019
//some existent code and potential apis
//batch size 100
//work thread -> run consensus protocol
//execute thread -> run transactions

/**
Client batch all requests and then send to some nodes
The server nodes will receive the batch and process it with multi threads 
**/

RC WorkerThread::process_client_batch(Message * msg)
{
	//TODO: check role here and decide whether process or wait for batch
	printf("Batch PBFT: %ld, THD: %ld\n",msg->txn_id, get_thd_id());
	fflush(stdout);

	RC rc = RCOK;
	ClientQueryBatch *btch = (ClientQueryBatch*)msg;
	if(!btch->validate()) {
		assert(0);
	} 

	//batchMTX.lock();
	next_set = msg->txn_id; // nextSetId++;
	//batchMTX.unlock();

	char * buf = (char*)malloc(btch->get_size() + 1);
    	btch->copy_to_buf(buf);
	Message * bmsg = Message::create_message(buf);
	ClientQueryBatch *clbtch = (ClientQueryBatch*)bmsg;

	// Allocate transaction manager for all other requests in batch.
	uint i;
	for(i=0; i<g_batch_size-1; i++) {
	   uint64_t txn_id = get_next_txn_id() + i;

	   TxnManager * tman = 
		txn_table.get_transaction_manager(get_thd_id(), txn_id, 0);

           clbtch->cqrySet[i].txn_id = txn_id;

	   tman->return_id = clbtch->return_node;
	   tman->client_id = clbtch->cqrySet[i].return_node;
  	   tman->client_startts = clbtch->cqrySet[i].client_startts;
  	   ((YCSBQuery*)(tman->query))->requests.append(clbtch->cqrySet[i].requests);


	   uint64_t relIndex = txn_id % indexSize;

	   //store client query
	   cqryStore[relIndex] = clbtch->cqrySet[i];

	   if(txn_id % g_batch_size == 0) {
		// Adding the first transaction of the batch
		batchSet[next_set % indexSize] = txn_id;
	   }
	}

	// Allocating data for the txn marking the batch.	
	uint64_t cindex = get_next_txn_id() + i;

        txn_man = txn_table.get_transaction_manager(get_thd_id(), cindex, 0);
    	txn_man->register_thread(this);
	uint64_t ready_starttime = get_sys_clock();
	bool ready = txn_man->unset_ready();
	assert(ready);
    	INC_STATS(get_thd_id(),worker_activate_txn_time,get_sys_clock() - ready_starttime);
	clbtch->cqrySet[i].txn_id = cindex;

	txn_man->return_id = clbtch->return_node;
	txn_man->client_id = clbtch->cqrySet[i].return_node;
  	txn_man->client_startts = clbtch->cqrySet[i].client_startts;
  	((YCSBQuery*)(txn_man->query))->requests.append(clbtch->cqrySet[i].requests);

    	INC_STATS(get_thd_id(),worker_activate_txn_time,get_sys_clock() - ready_starttime);

	uint64_t relIndex = cindex % indexSize;
	cqryStore[relIndex] = clbtch->cqrySet[i];


   	if((cindex + 1) % g_batch_size == 0) {
		// Forwarding message for ordered transmission.
		//cout << "Going to add \n";
		//fflush(stdout);
		txn_man->cbatch = next_set;

        //cout << "About to make BR msg of txn_id: " << cindex << endl;
        Message * deepCMsg = Message::create_message(txn_man, BATCH_REQ);


	//wait for another thread to process it
	work_queue.enqueue(get_thd_id(), deepCMsg, false);
	//rc = txn_man->start_commit();

      }

      return rc;
}

/**
pre-prepare - other non-leader nodes get the batch
**/

RC WorkerThread::process_batch(Message * msg)
{
    uint64_t ctime = get_sys_clock();
    
    BatchRequests *breq = (BatchRequests*)msg;
    
    printf("BatchRequests Received: Index:%ld : View: %ld : Thd: %ld\n",breq->txn_id, breq->view, get_thd_id());
    fflush(stdout);

    // ONLY NON PRIMARY NODES.
	if(g_node_id != g_view) { //Algorand TODO: change to role check

		if ( !breq->addAndValidate() ) {
			return ERROR; 
		}

		// Allocate transaction manager for all other requests in batch.
		uint i;
		for(i=0; i<g_batch_size-1; i++) {

	   		TxnManager * tman =
			txn_table.get_transaction_manager(get_thd_id(), breq->index[i], 0);

		   	tman->return_id = breq->return_node_id;
		   	tman->client_id = breq->requestMsg[i].return_node;
	  	   	tman->client_startts = breq->requestMsg[i].client_startts;
	  	   	((YCSBQuery*)(tman->query))->requests.append(breq->requestMsg[i].requests);
        	}

		// Allocating data for the txn marking the batch.
		uint64_t cindex = breq->index[i];

		txn_man = txn_table.get_transaction_manager(get_thd_id(), cindex, 0);
		uint64_t ready_starttime = get_sys_clock();
		bool ready = txn_man->unset_ready();
		INC_STATS(get_thd_id(), worker_activate_txn_time, get_sys_clock() - ready_starttime);
		assert(ready);
		txn_man->register_thread(this);
		txn_man->return_id = breq->return_node_id;
		txn_man->client_id = breq->requestMsg[i].return_node;
  		txn_man->client_startts = breq->requestMsg[i].client_startts;
  		((YCSBQuery*)(txn_man->query))->requests.append(breq->requestMsg[i].requests);

       	 	// Send Prepare messages.
        	txn_man->send_pbft_prep_msgs();

        	// End the pre-prepare counter, as the prepare phase tasks next.

       		double timepre = get_sys_clock() - ctime;
        	INC_STATS(get_thd_id(), time_pre_prepare, timepre);

		// Only when pre-prepare comes after prepare.
		if(txn_man->prep_rsp_cnt <= 0) {
	   		if(prepared()) {
		     		// Send Commit messages.
		     		cout << "Send commit from batch request" << endl;
		     		txn_man->send_pbft_commit_msgs();

		     		// End the prepare counter.
		     		double timeprep = get_sys_clock() - txn_man->txn_stats.time_start_prepare - timepre;
		     		INC_STATS(get_thd_id(), time_prepare, timeprep);
		     		double timediff = get_sys_clock()- ctime;

		     		// Sufficient number of commit messages would have come.
		     		if(txn_man->commit_rsp_cnt <= 0) {
		      			 uint64_t relIndex = cindex % indexSize;
		       			PBFTCommitMessage pcmsg;
		       			for(i=0; i<g_node_cnt; i++) {
		         			pcmsg = commStore[relIndex][i];
		         			if(pcmsg.txn_id == cindex) {
		           				break;
		         			} 
					}

		      		if(committed_local(&pcmsg)) {
			 		Message * tmsg = Message::create_message(txn_man, EXECUTE_MSG);
			 		work_queue.enqueue(get_thd_id(),tmsg,false);
					// End the commit counter.
			 		INC_STATS(get_thd_id(), time_commit, get_sys_clock() - txn_man->txn_stats.time_start_commit - timediff);
	 	       		} 

	      		} 
	  	}
   	} 
	else { //leader

	   	txn_man =
		  txn_table.get_transaction_manager(get_thd_id(), msg->txn_id, 0);
		bool ready = txn_man->unset_ready();
		assert(ready);

		char * buf = (char*)malloc(breq->get_size() + 1);
		breq->copy_to_buf(buf);
	
		INC_STATS(_thd_id,msg_node_out,g_node_cnt-1);
	
		for (uint64_t i = 0; i < g_node_cnt; i++) {
		    if (i == g_node_id) {
			continue;
		    }
		    	
		    Message * deepCopyMsg = Message::create_message(buf);
		    msg_queue.enqueue(get_thd_id(), deepCopyMsg, i); //broadcast the batch
		}

		// Setting the next expected prepare message id.
		expectedBatchCount = g_batch_size + msg->txn_id;
   	} 

	return RCOK;
}

//need more consideration
process_algorand_chkpt_msg

//process incoming prepare message
RC WorkerThread::process_algorand_prep_msg(Message * msg)
{
	cout << "PREPARE " << msg->txn_id << " FROM: " << msg->return_node_id << endl;
  	fflush(stdout);

	//TODO: transfer this part into BA* 
	//need timer
	//why reduction

	if(txn_man->prep_rsp_cnt == 2 * g_min_invalid_nodes) {
		txn_man->txn_stats.time_start_prepare = get_sys_clock();
	}

	if ( !((PBFTPrepMessage*)msg)->addAndValidate() ) {
		return ERROR;
	}
	
	//check if enough prepare messages have been recieved
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

//end the round
RC WorkerThread::process_algorand_commit_msg(Message * msg)
{
	cout << "PROCESS COMMIT " << msg->txn_id << " FROM: " << msg->return_node_id << "\n";
  	fflush(stdout);

	if(txn_man->commit_rsp_cnt == 2 * g_min_invalid_nodes + 1) {
		txn_man->txn_stats.time_start_commit = get_sys_clock();
	}

	PBFTCommitMessage *pcmsg = (PBFTCommitMessage*)msg;
	if ( !pcmsg->addAndValidate() ) {
		return ERROR;
	}
	
	int responses_left = --(txn_man->commit_rsp_cnt);

	cout << "Com Response: " <<responses_left << " :: Txn: " << msg->txn_id << "\n";
	fflush(stdout);

	if(responses_left == 0 && !txn_man->committed_local) {
	    if (committed_local(pcmsg)) {
		  	cout << "TO EXECUTE: " << msg->txn_id << "\n";
		  	fflush(stdout);

			// Add this message to execute thread's queue.
	    		Message *msg = Message::create_message(txn_man, EXECUTE_MSG);
	    		work_queue.enqueue(get_thd_id(), msg, false);

			// End the commit counter.
			INC_STATS(get_thd_id(), time_commit, get_sys_clock() - txn_man->txn_stats.time_start_commit);
        	} 
	}

	return RCOK;
}




