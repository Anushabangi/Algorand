# Verifiable Random Function

VRF is a pseudo-random function that provides publicly verifiable proofs of its outputs' correctness. It's used as an essential part in Algorand.

## Background

Project website at Boston University https://www.cs.bu.edu/~goldbe/projects/vrf.

## Our goal

There are already some good implementations on VRF. However, since our project is going to run on ExpoDB, we need a version written in C++ with the help of Crypto++ library https://www.cryptopp.com/.

## Steps
1. Design basic APIs of VRF. Function names and their input/output.
2. Figure out desired functions in Crypto++ to be adopted.
3. Finish VRF main program. Test on random keys.
