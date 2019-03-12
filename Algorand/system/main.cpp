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

#include "global.h"
#include "message.h"
#include "ycsb.h"
#include "thread.h"
#include "worker_thread.h"
#include "abort_thread.h"
#include "io_thread.h"
//#include "log_thread.h"
#include "math.h"
#include "query.h"
#include "transport.h"
#include "msg_queue.h"
#include "ycsb_query.h"
//#include "logger.h"
#include "sim_manager.h"
#include "abort_queue.h"
#include "work_queue.h"
#include "client_query.h"
#include "crypto.h"
#include "picosha2.h"

void network_test();
void network_test_recv();
void * run_thread(void *);


WorkerThread * worker_thds;
InputThread * input_thds;
OutputThread * output_thds;
AbortThread * abort_thds;
//LogThread * log_thds;


#if ALGORAND == true

#include <openssl/opensslconf.h>

static void cleanup(void)
{
  EVP_cleanup();
  CRYPTO_cleanup_all_ex_data();
}

#endif


// defined in parser.cpp
void parser(int argc, char * argv[]);

int main(int argc, char* argv[])
{
  // 0. initialize global data structure
  parser(argc, argv);
#if SEED != 0
  uint64_t seed = SEED + g_node_id;
#else
  uint64_t seed = get_sys_clock();
#endif
  srand(seed);
  printf("Random seed: %ld\n",seed);

  int64_t starttime;
  int64_t endtime;
  starttime = get_server_clock();
  printf("Initializing stats... ");
  fflush(stdout);
  stats.init(g_total_thread_cnt);
  printf("Done\n");

  //printf("Initializing global manager... ");
  //fflush(stdout);
  //glob_manager.init();
  //printf("Done\n");

  printf("Initializing transport manager... ");
  fflush(stdout);
  tport_man.init();
  printf("Done\n");
  fflush(stdout);

  printf("Initializing simulation... ");
  fflush(stdout);
  simulation = new SimManager;
  simulation->init();
  printf("Done\n");
  fflush(stdout);

  Workload * m_wl = new YCSBWorkload;
  m_wl->init();
  printf("Workload initialized!\n");
  fflush(stdout);

#if NETWORK_TEST
	tport_man.init(g_node_id,m_wl);
	sleep(3);
	if(g_node_id == 0)
		network_test();
	else if(g_node_id == 1)
		network_test_recv();

	return 0;
#endif


  printf("Initializing work queue... ");
  fflush(stdout);
  work_queue.init();
  printf("Done\n");
  printf("Initializing abort queue... ");
  fflush(stdout);
  abort_queue.init();
  printf("Done\n");
  printf("Initializing message queue... ");
  fflush(stdout);
  msg_queue.init();
  printf("Done\n");
  printf("Initializing transaction manager pool... ");
  fflush(stdout);
  txn_man_pool.init(m_wl,0);
  printf("Done\n");
  printf("Initializing transaction pool... ");
  fflush(stdout);
  txn_pool.init(m_wl,0);
  printf("Done\n");
  //printf("Initializing row pool... ");
  //fflush(stdout);
  //row_pool.init(m_wl,0);
  //printf("Done\n");
  //printf("Initializing access pool... ");
  //fflush(stdout);
  //access_pool.init(m_wl,0);
  //printf("Done\n");
  printf("Initializing txn node table pool... ");
  fflush(stdout);
  txn_table_pool.init(m_wl,0);
  printf("Done\n");
  printf("Initializing query pool... ");
  fflush(stdout);
  qry_pool.init(m_wl,0);
  printf("Done\n");
  printf("Initializing msg pool... ");
  fflush(stdout);
  msg_pool.init(m_wl,0);
  printf("Done\n");
  printf("Initializing transaction table... ");
  fflush(stdout);
  txn_table.init();
  printf("Done\n");
#if CC_ALG == CALVIN
  printf("Initializing sequencer... ");
  fflush(stdout);
  seq_man.init(m_wl);
  printf("Done\n");
#endif
#if CC_ALG == MAAT
  printf("Initializing Time Table... ");
  fflush(stdout);
  time_table.init();
  printf("Done\n");
  printf("Initializing MaaT manager... ");
  fflush(stdout);
	maat_man.init();
  printf("Done\n");
#endif
#if LOGGING
  printf("Initializing logger... ");
  fflush(stdout);
  logger.init("logfile.log");
  printf("Done\n");
#endif

#if SERVER_GENERATE_QUERIES
  printf("Initializing client query queue... ");
  fflush(stdout);
  client_query_queue.init(m_wl);
  printf("Done\n");
  fflush(stdout);
#endif

    #if CONSENSUS == PBFT || CONSENSUS == DBFT
	printf("Reading in Keys... ");
	
	//ifstream iPrivFile;
	//iPrivFile.open("privkeyS" + to_string(g_node_id) + ".txt");
	//if (iPrivFile.good()) {
	//	string privateKey;
	//	getline(iPrivFile, privateKey);
	//	g_priv_key = privateKey;
	//} else {
		printf("Creating keyPair\n");
		auto key = RsaGenerateHexKeyPair(3072);
		g_priv_key = key.privateKey;
		g_pub_keys[g_node_id] = key.publicKey;
		g_public_key = key.publicKey;


#if ALGORAND == true
//init
    printf("Init OpenSSL\n");
    fflush(stdout);
    atexit(cleanup);
    SSL_library_init();
    OpenSSL_add_all_digests();
    SSL_load_error_strings();
    ERR_load_BIO_strings();

    printf("Creating Algorand keyPair\n");
    m_dp.pk = g_algorand.init(g_node_id);
    fflush(stdout);
    m_dp = g_algorand.sortition(20,0,100,400,picosha2::hash256_hex_string(string("0x428a2f98")));
    g_dp = m_dp;
    cout<<m_dp.seed.size()<<endl;
    cout<<m_dp.hash.size()<<endl;
    cout<<m_dp.proof.size()<<endl;
    cout<<m_dp.value.size()<<endl;
    cout<<m_dp.pk.size()<<endl;
    cout<<sizeof(m_dp)<<endl;
    printf("Algorand Warmup Done\n");
    fflush(stdout);
#endif

		//ofstream oPrivFile;
		//oPrivFile.open("privkeyS" + to_string(g_node_id) + ".txt");
		//oPrivFile << key.privateKey;
		//oPrivFile.close();
	
		//ofstream oPubFile;
		//oPubFile.open("pubkeyS" + to_string(g_node_id) + ".txt");
		//oPubFile << key.publicKey;
		//oPubFile.close();
		//
		////every node needs to have the public key file for this node, 
		//// run again once thats done
		//assert(0);
	//}

	//record every node's public key (including itself)
	/*for(uint i = 0; i < g_node_cnt + g_client_node_cnt; i++) {
		ifstream iPubFile;
		if (i < g_node_cnt){
			iPubFile.open("pubkeyS" + to_string(i) + ".txt");
		}else{
			iPubFile.open("pubkeyC" + to_string(i) + ".txt");
		}

		if(iPubFile.good()) {
			//get and save public key of node with node_id == i
			string publicKey;
			getline(iPubFile, publicKey);
			g_pub_keys.push_back(publicKey);
			assert(g_pub_keys[i] == publicKey);
		} else {
			assert(0);
		} } */
	
	printf("Done\n");


	Message * cmsg = Message::create_message(CL_QRY);
	for(uint64_t i=0; i<2 * g_client_node_cnt * g_inflight_max; i++) {
		((YCSBClientQueryMessage *)cmsg)->txn_id = UINT64_MAX;
		//((YCSBClientQueryMessage *)cmsg)->client_startts = 0;
		cqryStore[i] = *((YCSBClientQueryMessage *)cmsg);
	}

	Message * msg = Message::create_message(PBFT_PRE_MSG);
	for(uint64_t i=0; i<2 * g_client_node_cnt * g_inflight_max; i++) {
		((PBFTPreMessage *)msg)->txn_id = UINT64_MAX;
		preStore[i] = *((PBFTPreMessage *)msg);
	}

	Message * clrsp = Message::create_message(CL_RSP);
	for(uint64_t i=0; i<2 * g_client_node_cnt * g_inflight_max; i++) {
		for(uint64_t j=0; j<g_node_cnt; j++) {
			((ClientResponseMessage *)clrsp)->txn_id = UINT64_MAX;
			clrspStore[i][j] = *((ClientResponseMessage *)clrsp);
		}
	}
    #endif

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

	#if VIEW_CHANGES == true
	#if CONSENSUS == PBFT || CONSENSUS == DBFT
	#if BATCH_ENABLE == BSET
	Message * bmsg = Message::create_message(BATCH_REQ);
	((BatchRequests*)bmsg)->index[BATCH_SIZE - 1] = UINT64_MAX;
	for(uint i = 0; i < indexSize; i++) {
		breqStore[i] = *((BatchRequests *)bmsg);
	}

	Message * cbmsg = Message::create_message(CL_BATCH);
	((ClientQueryBatch*)cbmsg)->txn_id = UINT64_MAX;
	for(uint i = 0; i < 3 * CLIENT_NODE_CNT * MAX_TXN_IN_FLIGHT; i++) {
		g_redirected_requests[i] = *((ClientQueryBatch *)cbmsg);
	}
	#endif
	#endif
	#endif //view changes
	
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

    // 2. spawn multiple threads
    uint64_t thd_cnt = g_thread_cnt;
    uint64_t wthd_cnt = thd_cnt;
    uint64_t rthd_cnt = g_rem_thread_cnt;
    uint64_t sthd_cnt = g_send_thread_cnt;
    uint64_t all_thd_cnt = thd_cnt + rthd_cnt + sthd_cnt + g_abort_thread_cnt;
#if LOGGING
    all_thd_cnt += 1; // logger thread
#endif
#if CC_ALG == CALVIN
    all_thd_cnt += 2; // sequencer + scheduler thread
#endif

    assert(all_thd_cnt == g_this_total_thread_cnt);
	
    pthread_t * p_thds =
    (pthread_t *) malloc(sizeof(pthread_t) * (all_thd_cnt));
    pthread_attr_t attr;
    pthread_attr_init(&attr);

    worker_thds = new WorkerThread[wthd_cnt];
    input_thds = new InputThread[rthd_cnt];
    output_thds = new OutputThread[sthd_cnt];
    abort_thds = new AbortThread[1];
    //log_thds = new LogThread[1];

    /*
    printf("Initializing threads... ");
    fflush(stdout);
      for (uint32_t i = 0; i < all_thd_cnt; i++)
          m_thds[i].init(i, g_node_id, m_wl);
    printf("Done\n");
    fflush(stdout);
    */

    endtime = get_server_clock();
    printf("Initialization Time = %ld\n", endtime - starttime);
    fflush(stdout);
    warmup_done = true;
    pthread_barrier_init( &warmup_bar, NULL, all_thd_cnt);

#if SET_AFFINITY
  uint64_t cpu_cnt = 0;
  cpu_set_t cpus;
//  CPU_ZERO(&cpus);
//  CPU_SET(cpu_cnt, &cpus);
//  pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpus);
//  cpu_cnt++;
#endif
  // spawn and run txns again.
  starttime = get_server_clock();
  simulation->run_starttime = starttime;

  uint64_t id = 0;
  for (uint64_t i = 0; i < wthd_cnt - 1; i++) {
#if SET_AFFINITY
      CPU_ZERO(&cpus);
      CPU_SET(cpu_cnt, &cpus);
      pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpus);
      cpu_cnt++;
#endif
      assert(id >= 0 && id < wthd_cnt);
      worker_thds[i].init(id,g_node_id,m_wl);
      pthread_create(&p_thds[id++], &attr, run_thread, (void *)&worker_thds[i]);
      pthread_setname_np(p_thds[id-1], "s_worker");
  }

  // Not setting execution thread.
  if(g_node_id == g_view) {
#if SET_AFFINITY
      CPU_ZERO(&cpus);
      CPU_SET(cpu_cnt, &cpus);
      pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpus);
      cpu_cnt++;
#endif
  }
  uint64_t ii = id;
  assert(id >= 0 && id < wthd_cnt);
  worker_thds[ii].init(id,g_node_id,m_wl);
  pthread_create(&p_thds[id++], &attr, run_thread, (void *)&worker_thds[ii]);
  pthread_setname_np(p_thds[id-1], "s_worker");

  for (uint64_t j = 0; j < rthd_cnt ; j++) {
#if SET_AFFINITY
    CPU_ZERO(&cpus);
    CPU_SET(cpu_cnt, &cpus);
    pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpus);
    cpu_cnt++;
#endif
    assert(id >= wthd_cnt && id < wthd_cnt + rthd_cnt);
    input_thds[j].init(id,g_node_id,m_wl);
    pthread_create(&p_thds[id++], &attr, run_thread, (void *)&input_thds[j]);
    pthread_setname_np(p_thds[id-1], "s_receiver");
  }


	for (uint64_t j = 0; j < sthd_cnt; j++) {
#if SET_AFFINITY
        CPU_ZERO(&cpus);
      CPU_SET(cpu_cnt, &cpus);
      pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpus);
      cpu_cnt++;
#endif
	    assert(id >= wthd_cnt + rthd_cnt && id < wthd_cnt + rthd_cnt + sthd_cnt);
	    output_thds[j].init(id,g_node_id,m_wl);
	    pthread_create(&p_thds[id++], &attr, run_thread, (void *)&output_thds[j]);
        pthread_setname_np(p_thds[id-1], "s_sender");
	  }
#if LOGGING
    #if SET_AFFINITY
      CPU_ZERO(&cpus);
      CPU_SET(cpu_cnt, &cpus);
      pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpus);
      cpu_cnt++;
    #endif
    log_thds[0].init(id,g_node_id,m_wl);
    pthread_create(&p_thds[id++], &attr, run_thread, (void *)&log_thds[0]);
    pthread_setname_np(p_thds[id-1], "s_logger");
#endif

#if CC_ALG != CALVIN
//    #if SET_AFFINITY
//      CPU_ZERO(&cpus);
//      CPU_SET(cpu_cnt, &cpus);
//      pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpus);
//      cpu_cnt++;
//    #endif
  abort_thds[0].init(id,g_node_id,m_wl);
  pthread_create(&p_thds[id++], &attr, run_thread, (void *)&abort_thds[0]);
    pthread_setname_np(p_thds[id-1], "aborter");
#endif

#if CC_ALG == CALVIN
#if SET_AFFINITY
    CPU_ZERO(&cpus);
    CPU_SET(cpu_cnt, &cpus);
    pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpus);
    cpu_cnt++;
#endif
    calvin_lock_thds[0].init(id,g_node_id,m_wl);
    pthread_create(&p_thds[id++], &attr, run_thread, (void *)&calvin_lock_thds[0]);
    pthread_setname_np(p_thds[id-1], "calvin_sched");

#if SET_AFFINITY
	CPU_ZERO(&cpus);
    CPU_SET(cpu_cnt, &cpus);
    pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpus);
    cpu_cnt++;
#endif

  calvin_seq_thds[0].init(id,g_node_id,m_wl);
  pthread_create(&p_thds[id++], &attr, run_thread, (void *)&calvin_seq_thds[0]);
    pthread_setname_np(p_thds[id-1], "calvin_seq");
#endif


	for (uint64_t i = 0; i < all_thd_cnt ; i++) 
		pthread_join(p_thds[i], NULL);

	endtime = get_server_clock();
	
  fflush(stdout);
  printf("PASS! SimTime = %f\n", (float)(endtime - starttime) / BILLION);
  if (STATS_ENABLE)
    stats.print(false);
  //malloc_stats_print(NULL, NULL, NULL);
  printf("\n");
  fflush(stdout);

#if ALGORAND == true
  RSA_free(g_algorand.m_publickey);
  RSA_free(g_algorand.m_privatekey);
  ERR_free_strings();
  cleanup();
  ERR_remove_state(0);
#endif
  // Free things
	//tport_man.shutdown();
  //m_wl->index_delete_all();

  /*
  txn_table.delete_all();
  txn_pool.free_all();
  access_pool.free_all();
  txn_table_pool.free_all();
  msg_pool.free_all();
  qry_pool.free_all();
  */
	return 0;
}

void * run_thread(void * id) {
    Thread * thd = (Thread *) id;
	thd->run();
	return NULL;
}

void network_test() {

      /*
	ts_t start;
	ts_t end;
	ts_t time;
	int bytes;
  float total = 0;
	for (int i = 0; i < 4; ++i) {
		time = 0;
		int num_bytes = (int) pow(10,i);
		printf("Network Bytes: %d\nns: ", num_bytes);
		for(int j = 0;j < 1000; j++) {
			start = get_sys_clock();
			tport_man.simple_send_msg(num_bytes);
			while((bytes = tport_man.simple_recv_msg()) == 0) {}
			end = get_sys_clock();
			assert(bytes == num_bytes);
			time = end-start;
      total += time;
			//printf("%lu\n",time);
		}
		printf("Avg(s): %f\n",total/BILLION/1000);
    fflush(stdout);
		//time = time/1000;
		//printf("Network Bytes: %d, s: %f\n",i,time/BILLION);
		//printf("Network Bytes: %d, ns: %.3f\n",i,time);
		
	}
      */

}

void network_test_recv() {
  /*
	int bytes;
	while(1) {
		if( (bytes = tport_man.simple_recv_msg()) > 0)
			tport_man.simple_send_msg(bytes);
	}
  */
}
