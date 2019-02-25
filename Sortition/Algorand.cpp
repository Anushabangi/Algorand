#include "Algorand.h"
#include "picosha2.h"

#include <string>
#include <ctime>
#include <iostream>

using namespace std;
class Algorand
{
public:
    
    struct vrfStruct {
        string hash;
        string proof;
    };
    struct sortitionRet {
        string hash;
        string proof;
        int j;
    }
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
    unit64_t sortition(string sk, string seed, uint64_t threshold, int role, int w, int W) {
        struct vrfStruct vrfRet = VRF(sk, seed, role);
        string hash = vrfRet.hash;
        string proof = vrfRet.proof;
        float p = threshold / W;
        int j = 0;
        int hashlen = hash.length();
        while(hash / 2^hashlen < sum(0, j, B(k,w,p)) || hash / 2^hashlen >= sum(0, j+1, B(k, w, p))) {
            j++;
        }
        struct sortitionRet results;
        results.hash = hash;
        results.proof = proof;
        results.j = j;
        return results;
    }
    
    int sum(int start, int end, int k, int w, int p) {
        int sum = 0;
        for(int i = start; i <= end; i++) {
            sum += B(i, w, p);
        }
        return sum;
    }
    int nCr(int n, int r)
    {
        if (r > n / 2)
        r = n - r;
        
        int answer = 1;
        for (int i = 1; i <= r; i++) {
            answer *= (n - r + i);
            answer /= i;
        }
        
        return answer;
    }
    int B(int k, int n, int p) {
        return nCr(n, k) * pow(p, k) * pow(1 - p, n - k);
    }
    int verify() {
        
    }
};
