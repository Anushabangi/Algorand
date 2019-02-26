#include "openssl_fdh.h"

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

	char *pub_key_char = malloc(pub_len + 1);
	// read BIO into Char[]
	BIO_read(pub, pub_key_char, pub_len);
	// append stop sign
	pub_key_char[pub_len] = '\0';

	return *pub_key_char;
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
size_t openssl_fdh_len(RSA *key)
{
	if (!key) {
		return 0;
	}

	return RSA_size(key);
}

/*!
 * Compute Full Domain Hash.
 */
size_t openssl_fdh_sign(const uint8_t *data, size_t data_len,
			uint8_t *sign, size_t sign_len,
			RSA *key, const EVP_MD *hash)
{
	if (!data || !key || !sign || !hash || sign_len < RSA_size(key)) {
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

	print_hex(data, data_len);

	if (r < 0) {
		return 0;
	}

	return r;
}

/*!
 * Verify Full Domain Hash.
 */
bool openssl_fdh_verify(const uint8_t *data, size_t data_len,
			const uint8_t *sign, size_t sign_len,
			RSA *key, const EVP_MD *hash)
{
	if (!data || !key || !sign || !hash || sign_len != RSA_size(key)) {
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
	if (r < 0 || r != sign_len) {
		return false;
	}

	// compare the result

	return sizeof(mask) == sizeof(decrypted) &&
	       memcmp(mask, decrypted, sizeof(mask)) == 0;
}
