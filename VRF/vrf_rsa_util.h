#include <iostream>
#include <string>


// class to hold private keys and public keys
// This can be replace with "RSAFunction" class in crypto++
class RSA_key {
  long long int n; // public modulus
  long long int e; // public exponent

};

/*!
 * Sign the input data, called by hasher
 *
 * \param[in]  data      Input data.
 * \param[out] sign      Output buffer.
 * \param[in]  key       RSA private key.
 * \param[in]  hash      Hash function, eg: SHA-256
 *
 * \return Size of the Full Domain Hash, zero on error.
 */
size_t rsa_sign(const std::string &data, std::string &sign, RSA &key,
    const std::string hash_type);

/*!
 * Verify signature, called by the verifier
 *
 * \param[in] data      Input data.
 * \param[in] sign      Signature to verify.
 * \param[in] key       RSA public/private key.
 * \param[in] hash      Hash function.
 *
 * \return True if the signature was validated successfully.
 */
bool rsa_verify(const std::string &data,	const std::string &sign, RSA *key,
    const std::string hash_type);
