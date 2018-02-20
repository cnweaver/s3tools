#ifndef S3TOOLS_SIGNING_H
#define S3TOOLS_SIGNING_H

#include <string>
#include <s3tools/url.h>

namespace s3tools{

///Compute the SHA256 hash of a message
///\param message the data to hash
///\return the hash encoded as a hexadecimal string
std::string SHA256Hash(const std::string& message);
	
///Generate a presigned URL (authentication using query parameters)
///\param username the 'Access Key ID' used for authorization
///\param secretkey the 'Secret Access Key' used for authorization
///\param verb the HTTP VERB for use with which the URL will be signed
///\param url the target URL, including all query parameters and headers
///\param exprTime the number of seconds for which the signature will be valid
///\param timestamp the base validity time of the signature. If empty, this will
///                 be the time at which the signature is computed.
///\return A URL with additional query parameters which encode the authentication 
///        information
URL genURL(std::string username, std::string secretkey, std::string verb,
           URL url, unsigned long exprTime, std::string timestamp="");
	
///Generate an authenticated URL (authentication using HTTP headers)
///\param username the 'Access Key ID' used for authorization
///\param secretkey the 'Secret Access Key' used for authorization
///\param verb the HTTP VERB for use with which the URL will be signed
///\param url the target URL, including all query parameters and headers
///\param exprTime the number of seconds for which the signature will be valid
///\param timestamp the base validity time of the signature. If empty, this will
///                 be the time at which the signature is computed.
///\return A URL with additional HTTP headers which encode the authentication 
///        information
URL genURLNoQuery(std::string username, std::string secretkey, std::string verb,
		   URL url, unsigned long exprTime, std::string timestamp="");
	
}

#endif //S3TOOLS_SIGNING_H