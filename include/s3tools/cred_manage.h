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
///\param overwrite whether the new credential should replace any old one 
///                 already stored for the same URL
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
///\return a pair containing the root URL for the best matching credential,
///        and the credential itself
///\throws std::runtime_error if no match is found
std::pair<std::string,credential> findCredentials(const CredentialCollection& credentials,
                                                  const std::string& urlStr);

enum class CredFormat{
	Internal,
	JSON
};

///Write credential data to an output stream for external use.
///\param targetStream the stream to which to write
///\param credentials the full set of credentials
///\param format the format for writing
void exportCredentials(std::ostream& targetStream,
                       const CredentialCollection& credentials,
                       CredFormat format=CredFormat::Internal);

///Write credential data to an output stream for external use.
///\param targetStream the stream to which to write
///\param credentials the full set of credentials
///\param format the format for writing
///\param selectedURL if non-empty, only the credential with the exactly
///                   matching URL will be written, otherwise all credentials
///                   are written
void exportCredentials(std::ostream& targetStream,
                       const CredentialCollection& credentials,
                       const std::string& selectedURL,
                       CredFormat format=CredFormat::Internal);

///Read one or more credentials from an external source and store them.
///\param inputData the stream from which to read credentials. The internal data
///                 format and JSON are both supported and automatically
///                 detected.
///\param sourceDesc a description of the data source, e.g. the input file path
///\param overwrite whether new credentials should replace any old ones already 
///                 stored for the same URLs
///\return true if any new credential was added
int importCredentials(std::istream& inputData,
                      const std::string& sourceDesc,
                      bool overwrite=false);

}

#endif //S3TOOLS_CRED_MANAGE_H
