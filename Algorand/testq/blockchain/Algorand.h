#ifndef ALGORAND_H
#define ALGORAND_H

#include <string>
#include <ctime>
#include <unordered_map>
#include <openssl/rsa.h>
#include <vector>
#include "helper.h"
#include "vrf_rsa_util.h"

using namespace std;

enum ALGORAND_QRY_TYPE { INIT=0, GETKEY, SORT, VERIFY, COUNT_V, RESET};

struct dataPackage {
    string seed; //the seed of current round
    string hash; //hash output
    string proof; //proof
    string value; //the hash of proposed block
    int j; //votes
    int node_id;
    string pk; //public key
    ALGORAND_QRY_TYPE type;
};



#define KEY_LENGTH 2048
#define EXPECTED_NUM 3
//TIMEOUT
//empty_hash
//MAXSTEPS
//threshold

class Algorand
{
public:
    Algorand(){};
    ~Algorand(){};   
    string publickeys[4];
    unordered_map<string,int> wallet; // context ctx - it should be a kind of hash map
    uint64_t round; //round number (the hash of a block)
    string seed;
    uint64_t step; //step number
    uint64_t user_count; //user number
    int sum_w; // the sum of current currency
    int m_id;
    unordered_map<string,int> counts;
    unordered_map<string,int> countvotes_pk;
    
    RSA* m_publickey;
    RSA* m_privatekey;

    uint64_t m_wallet;
    string m_block; //the hash of proposed block
    
    //uint64_t state; //0.init 1.waiting 2.voting 3.commit
    
    string init(int node_id);
    int getkey(string pk, int node_id);
    dataPackage sortition(uint64_t threshold, int role, int w, int W, string value);
    int verify(dataPackage *dp, uint64_t threshold, int role, int W);
    int countVotes(float T, uint64_t threshold, dataPackage *dp);
    //int BA();
    void reset();
    //void execute(int m_seed);
};

#endif // BLOCK_H
