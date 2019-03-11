#include "Algorand.h"
#include "picosha2.h"
#include <cmath>

double nCr(double n, double r) {
    if (r > n / 2)
        r = n - r;

    double answer = 1;
    for (double i = 1; i <= r; i++) {
        answer *= (n - r + i);
        answer /= i;
    }
    return answer;
}

double binary_distribution(double k, double n, double p) {
    return nCr(n, k) * pow(p, k) * pow(1 - p, n - k);
}

double algorand_sum(int start, int end, int w, double p) {
    double sum = 0;
    for(double i = start; i <= end; i++) {
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
    this->seed = "FWLKFWLKFWLKFWLKFWLK"; //start with a given seed
    this->m_id = node_id;
    this->m_privatekey = generate_pri_key(KEY_LENGTH);
    this->m_publickey = privkey_to_pubkey(this->m_privatekey);
    this->m_block = "0";
    this->sum_w = 100;
    this->publickeys[node_id] = key_to_string(m_publickey);
    this->f_hash = EVP_get_digestbyname("sha1");

    return key_to_string(this->m_publickey);
}

dataPackage Algorand::sortition(int threshold, int role, int w, int W, string value) {
    
    this->m_block = value;

    size_t proof_len = get_key_len(m_privatekey);
    uint8_t proof[proof_len];
    uint8_t hash[20];   // 20 for sha1

    if(!f_hash) {
        printf("Unknown hash function!\n");
        exit(1);
    }

    vrf_rsa_sign((const uint8_t*)this->seed.c_str(),this->seed.size(),proof,proof_len,m_privatekey,this->f_hash);
    //cout << "## wirtten: " << written << endl;

    //for (size_t i = 0; i < proof_len; i++) {
    //		    printf("%02x%c", proof[i], i % 16 == 15 ? '\n' : ' ');
    //	}
    size_t hash_len = proof_to_output(proof,proof_len,hash,this->f_hash);
    
    //cout << hash_len << endl;

    double p = (double) threshold / (double) W;
    int j = 0;
    int hashlen = 16; 
    unsigned int upper = unsigned(hash[0])*256+unsigned(hash[1]);
    double hash_div = (double)(upper) / pow(2.0,hashlen);

    //cout << "## hash output: " << endl;
    //for (size_t i = 0; i < hash_len; i++) {
	//	    printf("%02x%c", hash[i], i % 16 == 15 ? '\n' : ' ');
	//  }
    //cout<<p<<endl;
    //cout<<upper<<endl;
    //cout<<hash_div<<endl;

    while( hash_div < algorand_sum(0, j, w, p) || hash_div >= algorand_sum(0, j+1, w, p)) {
        j++;
    }

    dataPackage results;
    results.seed = this->seed;
    results.hash.clear();
    for(int i = 0; i < (int) hash_len; i++) {
        results.hash.push_back((char)hash[i]);
    }
    results.proof.clear();
    for(int i = 0; i < (int) proof_len; i++) {
        results.proof.push_back((char)proof[i]);
    }
    results.j = j;
    results.value = value;
    results.pk.clear();
    results.pk = key_to_string(m_publickey);
    results.node_id = this->m_id;

    this->wallet[results.pk] = this->m_wallet;

    return results;
}

int Algorand::verify(dataPackage *dp, int threshold, int role, int W) {
    
    size_t proof_len = dp->proof.size();
    uint8_t proof[proof_len];

    for (size_t i = 0; i < proof_len; i++) {
    		    proof[i] = (uint8_t) dp->proof[i];
    }

    char m_pk[dp->pk.length()];
    strcpy(m_pk,dp->pk.c_str());
    RSA *rsa_pk = string_to_key(m_pk, true);

    if (!vrf_rsa_verify((const uint8_t*)this->seed.c_str(), this->seed.size(), proof, proof_len,
                       rsa_pk, this->f_hash)) {
        return -1;
    }

    double p = (double) threshold / (double) W;
    int j = 0;
    int hashlen = 16;
    int w = this->wallet[dp->pk];

    unsigned int upper = unsigned((uint8_t)dp->hash[0])*256+unsigned((uint8_t)dp->hash[1]);
    double hash_div = (double)(upper) / pow(2.0,hashlen);

    while( hash_div < algorand_sum(0, j, w, p) || hash_div >= algorand_sum(0, j+1, w, p)) {
        j++;
    }

    RSA_free(rsa_pk);

    return j;
}

// 1 means success, 0 means failure
int Algorand::countVotes(double T, int threshold, dataPackage *dp) {
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

    if(counts[value] >= (T * threshold))
        return 1;
    else
        return 0;
}

void Algorand::reset() {
    this->counts.clear();
    this->countvotes_pk.clear();
    for(int i = 0; i < (int) this->seed.size(); i++) {
        this->seed[i] = this->m_block[i];
    }
    return;
}

void Algorand::execute(int seed) {
    //TODO
    return;
}

void Algorand::show_dp(dataPackage *dp) {
    cout<<dp->seed<<endl;
    fflush(stdout);
    cout << "## hash output: " << '\n';
    for (size_t i = 0; i < dp->hash.size(); i++) {
        printf("%02x%c", unsigned((uint8_t) dp->hash[i]), i % 16 == 15 ? '\n' : ' ');
        fflush(stdout);
    }
    cout << "\n";
    fflush(stdout);
    cout << "## proof: " << endl;
    for (size_t i = 0; i < dp->proof.size(); i++) {
        printf("%02x%c", unsigned((uint8_t) dp->proof[i]), i % 16 == 15 ? '\n' : ' ');
        fflush(stdout);
    }
    cout << "\n";
    fflush(stdout);
    cout<<dp->value<<'\n';
    fflush(stdout);
    cout<<dp->pk<<'\n';
    fflush(stdout);
    cout<<dp->node_id<<'\n';
    fflush(stdout);
    cout<<dp->j<<'\n';
    fflush(stdout);
    return;
}