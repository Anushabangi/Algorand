#include "vrf_rsa_util.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

#define error(fmt, ...) fprintf(stderr, fmt "\n", ##__VA_ARGS__)

static void print_hex(const uint8_t *data, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		printf("%02x%c", data[i], i % 16 == 15 ? '\n' : ' ');
	}

	if (len % 16 != 0) {
		printf("\n");
	}
}

static void cleanup(void)
{
	EVP_cleanup();
	CRYPTO_cleanup_all_ex_data();
}

static void help_info(const char *prog_name)
{
	printf("usage: %s <hash> <input-string>\n", prog_name);
}


int main(int argc, char *argv[]) {
  if (argc != 3) {
    help_info(argv[0]);
    return 1;
  }

	const char *hash_name = argv[1];

	const uint8_t *input = (uint8_t *)argv[2];
	const size_t input_len = strlen(argv[2]);

	printf("# input\n");
	print_hex(input, input_len);

  //。---------------------------------------------
	atexit(cleanup);
	OpenSSL_add_all_digests();

	// ----------------- hasher section ----------------------
	// 1. find hash function
	const EVP_MD *hash = EVP_get_digestbyname(hash_name);
	if (!hash) {
		error("Error retrieving hash function.");
		return 1;
	}

	// 2. generate private key (in practice, this is determined at beginning)
	int key_length = 2048;
	RSA *pri_key = generate_pri_key(key_length);
	if (!pri_key) {
		error("Error loading RSA private key.");
		return 1;
	}

	// 3. sign the input, taking input -> get proof
	size_t proof_len = get_key_len(pri_key);
	uint8_t proof[proof_len];
	size_t written = vrf_rsa_sign(input, input_len, proof, proof_len, pri_key, hash);
	if (written != proof_len) {
		error("Error creating FDH signature.");
		RSA_free(pri_key);
		return 1;
	}

  printf("## %s\n", "VRF proof");
	print_hex(proof, proof_len);
	printf("## %s: %zu\n", "length of proof", proof_len);

	// 4. compute hash output based on proof

	uint8_t hash_output[EVP_MAX_MD_SIZE];
	size_t hash_output_len = proof_to_output(proof, proof_len, hash_output, hash);
	if (hash_output_len <= 0) {
		error("Error computing hash output!");
	}
	printf("## %s\n", "hash output");
	print_hex(hash_output, hash_output_len);
	printf("## %s: %zu\n", "length of hash output", hash_output_len);

	// 5. convert (output, proof) to string format for communication


	// ----------------- verifier section ----------------------
	// 1. get public key from private key
	RSA *pub_key = privkey_to_pubkey(pri_key);

	// test convert pub key to string, and output
	char* key_str = key_to_string(pub_key);
	printf("%s\n", key_str);

	RSA* temp = string_to_key(key_str, 1);
	printf("Convert key to string, and back, to check the functions...\n");
	char* key_str2 = key_to_string(temp);
	printf("%s\n", key_str2);


	RSA_free(pri_key);
	if (!pub_key) {
		error("Error extracting public RSA parameters from the key.");
		return 1;
	}

	// 2. verify using input & proof
	bool valid = vrf_rsa_verify(input, input_len, proof, proof_len, pub_key, hash);
	RSA_free(pub_key);
	printf("# verification: %s\n", valid ? "succeeded!" : "failed!");

	return 0;
}
