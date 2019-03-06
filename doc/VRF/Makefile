CFLAGS=-std=c++11 -Wall -g -O2 -Icrypto

openssl_FLAGS=$(shell pkg-config --cflags --libs libcrypto)

.PHONY: all clean

all: test_vrf_rsa

clean:
	rm -f test_vrf_rsa

test_vrf_rsa: test.cpp vrf_rsa_util.cpp
	g++ $(CFLAGS)  -o $@ $^ $(openssl_FLAGS)
