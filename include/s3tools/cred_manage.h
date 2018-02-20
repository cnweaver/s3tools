#ifndef S3TOOLS_CRED_MANAGE_H
#define S3TOOLS_CRED_MANAGE_H

#include <string>
#include <unordered_map>

namespace s3tools{
	
struct credential{
	std::string username;
	std::string key;
};
	
using CredentialCollection=std::unordered_map<std::string,credential>;
	
///Read all credentials from the on-disk credential store
CredentialCollection fetchStoredCredentials();

///Add a credential to the credential store
///\param url the URL with which the credential will be associated
///\param cred the new credential
///\param overwrite where the new credential should replace any old one already 
///                 stored for the same URL
///\return true if the credential was stored, false if it was not due to a collision
bool storeCredential(std::string url, const credential& cred, bool overwrite=false);

///Remove the credential associated with a url from the credential store
///\param url the target url
///\return true if the removal was performed, false if it was not because the
///        target URL was not found in the credential store
bool removeCredential(std::string url);

///Attempt to guess which credentials to use for a given URL via simple prefix 
///matching. The available credential whose associated URL matches the longest 
///prefix of the given URL will be selected. 
///\param credentials the full collection of avaialable credentials
///\param urlStr the URL for which to find matching credentials
///\return the best matching credentials
///\throws std::runtime_error if no match is found
credential findCredentials(const CredentialCollection& credentials,
                           const std::string& urlStr);
	
}

#endif //S3TOOLS_CRED_MANAGE_H