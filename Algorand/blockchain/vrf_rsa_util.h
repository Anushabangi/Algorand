#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>


/*!
 * generate RSA private key
 *
 * \param[in]	 key_length   Length of key. Can't be smaller than 1024
 * \param[out] pri_key      Generated private key.
 */
RSA* generate_pri_key(int key_length);

/*!
 * generate corresponding public key according to private key
 * \param[in]  pri_key      The private key
 */
RSA *privkey_to_pubkey(RSA *pri_key);

/* !
 * convert private key in RSA to public key in C string
 * \param[in]	 pri_key      The private key (RSA*)
 * \param[out]              The public key (char*)
 */
char* key_to_string(RSA* pri_key);

/* !
 * Load RSA key from a pem key string
 * \param[in]	 key_str      pem key string
 * \param[in]  is_public    for public key or not
 * \param[out]              RSA* key
 */
RSA* string_to_key(const char* key_str, bool is_public);

/*!
 * Get size of Full Domain Hash result.
 */
size_t get_key_len(RSA *key);

/*!
 * Compute Full Domain Hash.
 *
 * \param[in]  data      Input data.
 * \param[in]  data_len  Length of the input data.
 * \param[out] sign      Output buffer.
 * \param[in]  sign_len  Capacity of the output buffer.
 * \param[in]  key       RSA private key.
 * \param[in]  hash      Hash function.
 *
 * \return Size of the Full Domain Hash, zero on error.
 */
size_t vrf_rsa_sign(const uint8_t *data, size_t data_len,
			uint8_t *sign, size_t sign_len,
			RSA *key, const EVP_MD *hash);

/*!
 * Compute hash output using proof as an input
 * \param[in]  proof
 * \param[in]  proof_len
 * \param[in]  hash              hash function
 * \param[out] hash_output
 * \param[out] hash_output_len   return value, -1 if any error
 */
 size_t proof_to_output(const uint8_t* proof, size_t proof_len,
 			uint8_t* hash_output, const EVP_MD* hash);

/*!
 * Verify Full Domain Hash.
 *
 * \param[in] data      Input data.
 * \param[in] data_len  Length of the input data.
 * \param[in] sign      Signature to verify.
 * \param[in] sign_len  Length of the signature.
 * \param[in] key       RSA public/private key.
 * \param[in] hash      Hash function.
 *
 * \return True if the signature was validated successfully.
 */
bool vrf_rsa_verify(const uint8_t *data, size_t data_len,
			const uint8_t *sign, size_t sign_len,
			RSA *key, const EVP_MD *hash);
