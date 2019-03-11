# Verifiable Random Function

VRF is a pseudo-random function that provides publicly verifiable proofs of its outputs' correctness. It's used as an essential part in Algorand.

## Background

Project website at Boston University https://www.cs.bu.edu/~goldbe/projects/vrf.

## Our goal
Build a daemon where we expose Algorand/VRF related function interfaces. THe ExpoDB processes will communicate with the process to call certain functions to do sortition, and verification etc.

## Steps for VRF
1. Make use of the existing implementation of VRF (C program, using openssl library, based on RSA algorithm). Configure, run and test.
2. Add functions specificly for our project model, for example, function preparing the communication between processes.
3. Testing sortition and verfication procedure.
