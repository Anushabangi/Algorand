#include "Algorand.h"
#include "picosha2.h"
#include <math.h>

const EVP_MD *f_hash = EVP_get_digestbyname("sha1");

float nCr(float n, float r) {
    if (r > n / 2)
        r = n - r;
    
    float answer = 1;
    for (float i = 1; i <= r; i++) {
        answer *= (n - r + i);
        answer /= i;
    }
    return answer;
}

float binary_distribution(float k, float n, float p) {
    return nCr(n, k) * pow(p, k) * pow(1 - p, n - k);
}

float algorand_sum(int start, int end, int w, float p) {
    float sum = 0;
    for(float i = start; i <= end; i++) {
        sum += binary_distribution(i, w, p);
    }
    return sum;
}

int Algorand::getkey(string pk, int node_id) {

    if(node_id <= 3) {
        this->publickeys[node_id] = pk;
        this->wallet[pk] = 100;
        this->sum_w += 100;
    }

    return 0;
}


string Algorand::init(int node_id) {
    this->user_count = 4;
    this->m_wallet = 100;
    this->round = 0;
    this->step = 0;
    this->seed = "aaa"; //start with a given seed
    this->m_id = node_id;
    this->m_privatekey = generate_pri_key(KEY_LENGTH);
    this->m_publickey = privkey_to_pubkey(this->m_privatekey);
    this->m_block = "0";
    this->sum_w = 100;
    this->publickeys[node_id] = key_to_string(m_publickey);

    return key_to_string(this->m_publickey);
}

dataPackage Algorand::sortition(uint64_t threshold, int role, int w, int W, string value) {
    this->m_block = value;

    uint8_t proof[KEY_LENGTH];
    uint8_t hash[20];
    vrf_rsa_sign((const uint8_t*)this->seed.c_str(),this->seed.size(),proof,KEY_LENGTH,this->m_privatekey,f_hash);
    proof_to_output(proof,KEY_LENGTH,hash,f_hash);

    float p = threshold / W;
    int j = 0;
    int hashlen = 16;    //TODO!
    unsigned int upper = unsigned(hash[0])*256+unsigned(hash[1]);
    float hash_div = (float)(upper) / pow(2.0,hashlen);
    cout<<proof<<endl;
    cout<<upper<<endl;
    cout<<hash_div<<endl;
    
    //TODO: shorter hash and hashlen, need discussion on Sunday
    while( hash_div < algorand_sum(0, j, w, p) || hash_div >= algorand_sum(0, j+1, w, p)) {
        j++;
        cout<<algorand_sum(0, j, w, p)<<endl;
    }
    
    dataPackage results;
    results.hash = (char*)hash;
    results.proof = (char*)proof;
    results.j = j;
    results.value = value;
    results.pk = key_to_string(m_publickey);
    results.node_id = this->m_id;

    return results;
}

int Algorand::verify(dataPackage *dp, uint64_t threshold, int role, int W) {

    if (!vrf_rsa_verify((const uint8_t*)this->seed.c_str(), this->seed.size(), (const uint8_t*)dp->proof.c_str(), KEY_LENGTH,
                       string_to_key(dp->pk.c_str(), true), f_hash)) {
        return 0;
    }
    float p = threshold / W;
    int j = 0;
    int hashlen = 16;
    int w = this->wallet[dp->pk];
    
    while( ( (dp->hash[0]*256+dp->hash[1]) / pow(2.0,hashlen)) < algorand_sum(0, j, w, p) || ( (dp->hash[0]*256+dp->hash[1])/ pow(2.0,hashlen)) >= algorand_sum(0, j+1, w, p)) {
        j++;
    }

    return j;
}

// 1 means success, 0 means failure
int Algorand::countVotes(float T, uint64_t threshold, dataPackage *dp) {
    string pk = dp->pk;
    string value = dp->value; // the hash of the proposed block
    int votes = dp->j;

    unordered_map<string,int>::const_iterator gotPk = this->countvotes_pk.find(pk);

    if(gotPk != countvotes_pk.end()) return -1;

    countvotes_pk[pk] = 1;
    unordered_map<string,int>::const_iterator gotValue = this->counts.find(value);

    if(gotValue != counts.end()) {
        counts[value] += votes;
    } else {
        counts[value] = votes;
    }

    if(counts[value] >= 2)
        return 1;
    else
        return 0;
}

void Algorand::reset() {
    this->counts.clear();
    this->countvotes_pk.clear();
    this->m_block = "0";
    this->seed = this->m_block; //use as the next seed
    return;
}
