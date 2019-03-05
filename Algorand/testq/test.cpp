#include "helper.h"
#include "mem_alloc.h"
#include <pthread.h>
#include <atomic>
#include "Block.h"
#include "Algorand.h"

using namespace std;

//enum SysState {START,INIT,EXEC,PREP,FIN,DONE};

typedef struct node{
	uint64_t a;
	uint64_t b;
	uint64_t c;
	struct node * next;
}*pNode;

void *PrintHello(void *threadid) { 
	uint64_t tid; 
	tid = (uint64_t)threadid; 
	printf("Hello World! It's me, thread #%ld!\n", tid); 
	pthread_exit(NULL); 
} 


void *Tester(void *threadid) {
	uint64_t tid; 
	tid = (uint64_t)threadid;
	printf("Hello World! It's me, thread #%ld!\n", tid); 
	//TODO
	pthread_exit(NULL);
}

int main(int argc, char* argv[])
{
/**
   	pthread_t threads[4];
	pthread_attr_t attr;
	uint64_t rc, t; 
	uint64_t start,end,dff;
    uint64_t cpu_cnt = 0;
    cpu_set_t cpus;
	dff = 0;
**/

	Algorand m_algorand;
	dataPackage m_package;

	string m_pk = m_algorand.init(0);

	cout<<"init done"<<endl;
	m_algorand.getkey("1",1);
	m_algorand.getkey("2",2);
	m_algorand.getkey("3",3);

	cout<<"getkey done"<<endl;

	cout<<m_algorand.m_wallet<<endl;
	cout<<m_algorand.sum_w<<endl;
	m_package = m_algorand.sortition(3,0,m_algorand.m_wallet,m_algorand.sum_w,"a");


	cout<<m_package.j<<endl;

/**
	pthread_attr_init(&attr);
    CPU_ZERO(&cpus);
    CPU_SET(cpu_cnt, &cpus);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpus);
    cpu_cnt++;

	
   	for( t = 0; t < 4; t++){ 
      		printf("In main: creating tester thread %ld\n", t); 
		//CPU_ZERO(&cpus);
		//CPU_SET(cpu_cnt, &cpus);
		//pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpus);
		cpu_cnt++;
		rc = pthread_create(&threads[t], &attr, Tester, (void *)t); 
      		if (rc){ 
         		printf("ERROR; return code from pthread_create() is %ld\n", rc); 
         		exit(-1); 
      		} 
	} 
	start = get_server_clock();

	//timing
	while(dff <= 10000000){
		end = get_server_clock();
		dff = end - start;
	}
	
	//cancel all threads
	for( t = 0; t < 3; t++){
		pthread_cancel(threads[t]);
	}
	for( t = 0; t < 3; t++){
		pthread_join(threads[t],NULL);
	}		

	printf("The total time is %ld\n", dff);
**/
	return 0;
}
