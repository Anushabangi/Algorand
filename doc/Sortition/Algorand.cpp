#include "Algorand.h"
#include "picosha2.h"
#include "vrf_rsa_util.h"
#include <math.h>

int algorand_sum(int start, int end, int k, int w, int p) {
    int sum = 0;
    for(int i = start; i <= end; i++) {
        sum += B(i, w, p);
    }
    return sum;
}

int nCr(int n, int r) {
    if (r > n / 2)
        r = n - r;
    
    int answer = 1;
    for (int i = 1; i <= r; i++) {
        answer *= (n - r + i);
        answer /= i;
    }
    
    return answer;
}

int binary_distribution(int k, int n, int p) {
    return nCr(n, k) * pow(p, k) * pow(1 - p, n - k);
}

RSA* Algorand::init() {
    //TODO: generate keys handle error?
    this->user_count = 4;
    this->m_wallet = 100;
    this->round = 0;
    this->step = 0;
    this->seed = "0";
    this->state = 0;
    this->m_privatekey = generate_pri_key(2048);
    this->m_publickey = privkey_to_pubkey(this->m_privatekey);
    return this->m_publickey;
}

sortitionRet Algorand::sortition(string sk, uint64_t threshold, int role, int w, int W) {
    
    vrfStruct vrfRet = VRF(sk, this->seed, role);
    uint8_t hash = vrfRet.hash;
    uint8_t proof = vrfRet.proof;
    float p = threshold / W;
    int j = 0;
    int hashlen = hash.length();    //TODO
    
    //TODO: shorter hash and hashlen, need discussion on Sunday
    while( (hash / pow(2.0,hashlen)) < algorand_sum(0, j, binary_distribution(k,w,p)) || (hash / pow(2.0,hashlen)) >= algorand_sum(0, j+1, binary_distribution(k, w, p))) {
        j++;
    }
    
    sortitionRet results;
    results.hash = hash;
    results.proof = proof;
    results.j = j;
    return results;
}

//TODO: pass sign(proof) as parameters or get form VRF function
int Algorand::verify(const uint8_t *data, size_t data_len, const uint8_t *sign, size_t sign_len, const EVP_MD *hash, uint64_t threshold, int role, int w, int W) {
    vrfStruct vrfRet = VRF(this.m_privatekey, this->seed, role);
    uint8_t hashString = vrfRet.hash;
    uint8_t proof = vrfRet.proof;
    int hashlen = hashString.length();
    if (!vrf_rsa_verify(data, data_len, sign, sign_len,
                       this->m_publickey, hash)) {
        return 0;
    }
    float p = threshold / W;
    int j = 0;
    
    //TODO: shorter hash and hashlen, need discussion on Sunday
    while( (hashString / pow(2.0,hashlen)) < algorand_sum(0, j, binary_distribution(k,w,p)) || (hash / pow(2.0,hashlen)) >= algorand_sum(0, j+1, binary_distribution(k, w, p))) {
        j++;
    }
    return j;
}

// 1 means success, 0 means failure
int Algorand::countVotes(float T, uint64_t threshold, dataPackage dp) {
    RSA* pk = dp.pk;
    string value = dp.value;
    int votes = dp.votes;
    unordered_map<RSA*,int>::const_iterator gotPk = this->countvotes_pk.find(pk);
    if(gotPk != countvotes_pk.end()) continue;
//    pair<RSA*,int> newPk (pk, 1);
//    countvotes_pk.insert(newPk);
    countvotes_pk[pk] = 1;
    unordered_map<string,int>::const_iterator gotValue = this->counts.find(value);
    if(gotValue != counts.end()) {
        counts[value] += votes;
    } else {
        pair<string,int> newValue (value, votes);
        counts[value] = votes;
        // counts.insert(newValue);
    }
    if(counts[value] > T * threshold)
        return 1;
    else
        return 0;
}

//BA() {
//    
//}
