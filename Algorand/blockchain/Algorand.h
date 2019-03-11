#ifndef ALGORAND_H
#define ALGORAND_H

#include <string>
#include <unordered_map>
#include <openssl/rsa.h>
#include <vector>
#include "helper.h"
#include "vrf_rsa_util.h"

using namespace std;

enum ALGORAND_QRY_TYPE { ALGORAND_INIT=0, ALGORAND_GETKEY, ALGORAND_SORT, ALGORAND_VERIFY, ALGORAND_COUNT_V, ALGORAND_RESET};

struct dataPackage {
    string seed; //the seed of current round
    string hash; //hash output
    string proof; //proof
    string value; //the hash of proposed block
    int j; //votes
    int node_id;
    string pk; //public key
    ALGORAND_QRY_TYPE type;

    dataPackage() {
        seed = string(20,'0');
        hash = string(20,'0');
        proof = string(256,'0');
        value = string(64,'0');
        j = 0;
        node_id = 0;
        pk = string(426,'0');
        type = ALGORAND_INIT;
    }
};



#define KEY_LENGTH 2048
#define EXPECTED_NUM 3

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
    const EVP_MD *f_hash;
    
    RSA* m_publickey;
    RSA* m_privatekey;

    uint64_t m_wallet;
    string m_block; //the hash of proposed block
    
    string init(int node_id);
    int getkey(string pk, int node_id);
    dataPackage sortition(int threshold, int role, int w, int W, string value);
    int verify(dataPackage *dp, int threshold, int role, int W);
    int countVotes(double T, int threshold, dataPackage *dp);
    void reset();
    void execute(int seed);//TODO
    void show_dp(dataPackage *dp);
};

#endif // BLOCK_H
