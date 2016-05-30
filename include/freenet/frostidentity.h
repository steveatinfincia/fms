#ifndef _frost_identity_
#define _frost_identity_

#include <string>
#include <vector>
#include <mbedtls/rsa.h>

class FrostIdentity
{
public:
	FrostIdentity();
	~FrostIdentity();

	const bool FromPublicKey(const std::string &publickey);
	
	const bool VerifyAuthor(const std::string &author);
	const bool VerifySignature(const std::vector<unsigned char> &data, const std::string &signature);

private:

	std::string m_publickey;
	mbedtls_rsa_context m_rsa;

};

#endif	// _frost_identity_
