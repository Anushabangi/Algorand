CFLAGS=-std=gnu99 -Wall -g -O2 -Icrypto

openssl_FLAGS=$(shell pkg-config --cflags --libs libcrypto)

.PHONY: all clean

all: test_vrf_rsa

clean:
	rm -f test_vrf_rsa

test_vrf_rsa: test.c vrf_rsa_util.c
	$(CC) $(CFLAGS)  -o $@ $^ $(openssl_FLAGS)
