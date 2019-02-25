u#ifndef ALGORAND_H
#define ALGORAND_H

#include <string>
#include <ctime>
#include <unordered_map>
#include <vector>
#include "helper.h"

using namespace std;


//KeyExchange - data structure for initial and exchange

//TIMEOUT 

//empty_hash

//MAXSTEPS

//threshold



class Algorand
{
public:
	Algorand();
	~Algorand();

	string *publickeys;
	//unordered_map<string,uint64_t> wallet; // context ctx - it should be a kind of hash map
	string round; //round number (the hash of a block)
	uint64_t step; //step number
	uint64_t user_count; //user number
	//int *role; //the role of each member
	

	string m_publickey;
	string m_privatekey;
	uint64_t m_wallet;
	//block m_block[hash]; in flight block
	//unordered_map<string, vector<message>> old_message; old messages[round,step]

	uint64_t state; //0.init 1.waiting 2.voting 3.commit

	//sortition();
	//verify();
	//vote();
	//countvotes();
	//BA();
};

#endif // BLOCK_H
