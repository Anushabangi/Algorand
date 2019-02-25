#include <string>
sortition(sk, seed, threshold, role, w, W) {
	<hash, proof> = VRF(sk, seed, role);
	p = threshold / W;
	j = 0;
	while(hash / 2^hashlen < sum(0, j, B(k,w,p)) || hash / 2^hashlen >= sum(0, j+1, B(k, w, p))) {
		j++;
	}
	return <hash, proof, j>;
}

