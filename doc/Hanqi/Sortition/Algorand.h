u#ifndef ALGORAND_H
#define ALGORAND_H

#include <string>
#include <ctime>
#include <unordered_map>
#include <openssl/rsa.h>
#include <vector>
#include "helper.h"

using namespace std;

struct vrfStruct {
    uint8_t hash;
    uint8_t proof;
};

struct sortitionRet {
    uint8_t hash;
    uint8_t proof;
    int j;
}

struct dataPackage {
    uint8_t hash;
    RSA* pk;
    string value;
    int votes;
};
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
    uint64_t round; //round number (the hash of a block)
    string seed;
    uint64_t step; //step number
    uint64_t user_count; //user number
    //int *role; //the role of each member
    unordered_map<string,int> counts;
    unordered_map<RSA*,int> countvotes_pk;
    
    RSA* m_publickey;
    RSA* m_privatekey;
    uint64_t m_wallet;
    string m_block;
    //unordered_map<string, vector<message>> old_message; old messages[round,step]
    
    uint64_t state; //0.init 1.waiting 2.voting 3.commit
    
    RSA* init();
    sortitionRet Sortition(string sk, uint64_t threshold, int role, int w, int W);
    int verify(const uint8_t *data, size_t data_len, const uint8_t *sign, size_t sign_len, const EVP_MD *hash, uint64_t threshold, int role, int w, int W);
    int countVotes(float T, uint64_t threshold, dataPackage dp);
    //BA();
};

#endif // BLOCK_H
