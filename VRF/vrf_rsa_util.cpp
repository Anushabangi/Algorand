#include "vrf_rsa_util.h"

#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include <openssl/evp.h>
#include <openssl/rsa.h>


/* !
 * convert private key in RSA to public key in C string
 */
char* key_to_string(RSA* pri_key) {
	if (!pri_key) {
		return NULL;
	}
	// convert RSA to BIO
	BIO *pub = BIO_new(BIO_s_mem());
	PEM_write_bio_RSAPublicKey(pub, pri_key);

	size_t pub_len = BIO_pending(pub);

	char *pub_key_char = (char *)malloc(pub_len + 1);
	// read BIO into Char[]
	BIO_read(pub, pub_key_char, pub_len);
	// append stop sign
	pub_key_char[pub_len] = '\0';
	BIO_free(pub);
	return pub_key_char;
}

/*
 * Load RSA key from a pem key string
 */
RSA* string_to_key(const char* key_str, bool is_public) {
	RSA* rsa_key = RSA_new();
	BIO* key_bio;
	key_bio = BIO_new_mem_buf(key_str, -1);
	if (key_bio==NULL) {
			printf( "Failed to get key BIO");
			return NULL;
	}
	// printf("converting...\n");
	if(is_public) {
		PEM_read_bio_RSAPublicKey(key_bio, &rsa_key,NULL, NULL);
	}
	else {
		PEM_read_bio_RSAPrivateKey(key_bio, &rsa_key,NULL, NULL);
	}
	BIO_free(key_bio);
	return rsa_key;
}

/*!
 * generate RSA private key
 */
RSA* generate_pri_key(int key_length) {
	int pub_exp = 3;
	if (key_length < 1024) {
		// too short to be secure
		return NULL;
	}

	return RSA_generate_key(key_length, pub_exp, NULL, NULL);
}

/*!
 * generate corresponding public key according to private key
 */
RSA *privkey_to_pubkey(RSA *pri_key) {
	RSA *pub_key = RSA_new();
	if (!pub_key || !pri_key) {
		// failed to create RSA instance
		// or private key is invalid
		return NULL;
	}

	pub_key->n = BN_dup(pri_key->n);
	pub_key->e = BN_dup(pri_key->e);

	return pub_key;
}

/*!
 * Get size of Full Domain Hash result.
 */
size_t get_key_len(RSA *key) {
	if (!key) {
		return 0;
	}

	return RSA_size(key);
}

/*!
 * Compute Full Domain Hash.
 */
size_t vrf_rsa_sign(const uint8_t *data, size_t data_len,
			uint8_t *sign, size_t sign_len,
			RSA *key, const EVP_MD *hash) {
	if (!data || !key || !sign || !hash || sign_len < (size_t) RSA_size(key)) {
		return 0;
	}

	// compute MGF1 mask
	uint8_t mask[BN_num_bytes(key->n)];
	mask[0] = 0;
	if (PKCS1_MGF1(mask + 1, sizeof(mask) - 1, data, data_len, hash) != 0) {
		return 0;
	}

	// preform raw RSA signature
	int r = RSA_private_encrypt(sizeof(mask), mask, sign, key, RSA_NO_PADDING);
	if (r < 0) {
		return 0;
	}

	return r;
}


// helper function to compute the hash output
size_t compute_hash_out(const EVP_MD *hash, uint8_t* msg, size_t msg_len,
			uint8_t* md_value, bool debug) {

	EVP_MD_CTX *mdctx;
	unsigned int md_len;

	OpenSSL_add_all_digests();

	mdctx = EVP_MD_CTX_create();
	if (!EVP_DigestInit_ex(mdctx, hash, NULL)
			|| !EVP_DigestUpdate(mdctx, msg, msg_len)
			|| !EVP_DigestFinal_ex(mdctx, md_value, &md_len)) {
		return 0;
	}

	EVP_MD_CTX_destroy(mdctx);
	if (debug) {
		printf("Digest is: \n");
		for (size_t i = 0; i < md_len; i++) {
			printf("%02x%c", md_value[i], i % 16 == 15 ? '\n' : ' ');
		}

		if (md_len % 16 != 0) {
			printf("\n");
		}
	}
	/* Call this once before exit. */
	EVP_cleanup();
	return md_len;
}


/*!
 * Compute hash output using proof as an input
 */
size_t proof_to_output(const uint8_t* proof, size_t proof_len,
			uint8_t* hash_output, const EVP_MD* hash) {

	uint8_t tmp[proof_len];
	memcpy(tmp, proof, proof_len);
	//
	tmp[proof_len-1] |= 0x02;
	bool debug = false;
	return compute_hash_out(hash, tmp, proof_len, hash_output, debug);
}

/*!
 * Verify Full Domain Hash.
 */
bool vrf_rsa_verify(const uint8_t *data, size_t data_len,
			const uint8_t *sign, size_t sign_len,
			RSA *key, const EVP_MD *hash) {
	if (!data || !key || !sign || !hash || sign_len != (size_t) RSA_size(key)) {
		return false;
	}

	// compute MGF1 mask
	uint8_t mask[BN_num_bytes(key->n)];
	mask[0] = 0;
	if (PKCS1_MGF1(mask + 1, sizeof(mask) - 1, data, data_len, hash) != 0) {
		return false;
	}

	// reverse RSA signature
	uint8_t decrypted[sign_len];
	int r = RSA_public_decrypt(sign_len, sign, decrypted, key, RSA_NO_PADDING);
	if (r < 0 || (size_t) r != sign_len) {
		return false;
	}

	// compare the result
	return sizeof(mask) == sizeof(decrypted) &&
	       memcmp(mask, decrypted, sizeof(mask)) == 0;
}
