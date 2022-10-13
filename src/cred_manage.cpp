#include <s3tools/cred_manage.h>

#include <fstream>

#include <cstdlib> //for getenv
#include <unistd.h> //for getuid
#include <sys/stat.h> //for stat
#include <sys/types.h> //
#include <sys/errno.h> //for errno, error codes

namespace s3tools{

///Get the path to the user's credential file
std::string getCredFilePath(){
	char* envResult;
	envResult=getenv("S3_CRED_PATH");
	if(envResult)
		return(envResult);
	
	envResult=getenv("XDG_CONFIG_HOME");
	if(envResult){
		std::string path=envResult;
		if(!path.empty()){
			if(path.back()!='/')
				path+='/';
			path+="s3tools/credentials";
			return(path);
		}
	}
	
	envResult=getenv("HOME");
	if(!envResult)
		throw std::runtime_error("Unable to locate home directory");
	std::string path=envResult;
	if(path.empty())
		throw std::runtime_error("Got an empty home directory path");
	if(path.back()!='/')
		path+='/';
	path+=".config/s3tools/credentials";
	return(path);
}

enum class PermState{
	VALID,
	INVALID,
	DOES_NOT_EXIST //This really is FILE_NOT_FOUND
};
	
///Ensure that the given path is readable only by the owner
PermState checkPermissions(const std::string& path){
	struct stat data;
	int err=stat(path.c_str(),&data);
	if(err!=0){
		err=errno;
		if(err==ENOENT)
			return(PermState::DOES_NOT_EXIST);
		//TODO: more detail on what failed?
		throw std::runtime_error("Unable to stat "+path);
	}
	//check that the current user is actually the file's owner
	if(data.st_uid!=getuid())
		return(PermState::INVALID);
	return((data.st_mode&((1<<9)-1))==0600 ? PermState::VALID : PermState::INVALID);
}

CredentialCollection parseCredentials(std::istream& credFile, const std::string& path){
	CredentialCollection credentials;
	unsigned int entry=0;
	std::string url;
	while(credFile >> url){
		entry++;
		char delim;
		credFile >> delim;
		if(delim!='"' || !credFile)
			throw std::runtime_error("Failed to find expected delimiter (\") for start of username after url in entry "
									 +std::to_string(entry)+" of "+path);
		std::string username;
		std::getline(credFile,username,'"');
		if(!credFile)
			throw std::runtime_error("Failed to find expected delimiter (\") for end of username in entry "
									 +std::to_string(entry)+" of "+path);
		credFile >> delim;
		if(delim!='"' || !credFile)
			throw std::runtime_error("Failed to find expected delimiter (\") for start of key after username in entry "
									 +std::to_string(entry)+" of "+path);
		std::string key;
		std::getline(credFile,key,'"');
		if(!credFile)
			throw std::runtime_error("Failed to find expected delimiter (\") for end of key in entry "
									 +std::to_string(entry)+" of "+path);
		credentials.emplace(url,credential{username,key});
	}
	if(credFile.bad())
		throw std::runtime_error("Error reading from "+path);
	return(credentials);
}
	
CredentialCollection fetchStoredCredentials(){
	std::string path=getCredFilePath();
	PermState perms=checkPermissions(path);
	if(perms==PermState::INVALID)
		throw std::runtime_error("Credentials file "+path+" has wrong permissions; should be 0600 and owned by the current user");
	CredentialCollection credentials;
	if(perms==PermState::DOES_NOT_EXIST)
		return(credentials); //nothing to read, we're done
	
	std::ifstream credFile(path);
	if(!credFile) //this mostly shouldn't happen since we already checked the permissions
		throw std::runtime_error("Failed to open credentials file "+path+" for reading");
	
	credentials=parseCredentials(credFile,path);
	return(credentials);
}
	
std::ostream& writeCredentialRecord(std::ostream& os, const std::string& url, const credential& cred){
	return(os << url << "\n\t\"" << cred.username << "\"\n\t\"" << cred.key << "\"\n");
}

///Overwrite all stored credentials with a different collection
///\pre file permissions requirements must already have been enforced on the
///     credential file (exists, owned by correct user, permissions are 0600).
void writeCredentials(const CredentialCollection& credentials){
	std::string path=getCredFilePath();
	//permissions were already enforced by fetchStoredCredentials
	std::ofstream credFile(path);
	if(!credFile) //this mostly shouldn't happen since we already checked the permissions
		throw std::runtime_error("Failed to open credentials file "+path+" for writing");
	for(const auto& record : credentials){
		writeCredentialRecord(credFile,record.first,record.second);
		if(credFile.fail() || credFile.bad())
			throw std::runtime_error("Failed to write to "+path);
	}
}

void mkdir_p(const std::string& path, uint16_t mode){
	auto makeIfNonexistent=[=](const std::string& path){
		bool make=false;
		struct stat data;
		int err=stat(path.c_str(),&data);
		if(err!=0){
			err=errno;
			if(err!=ENOENT)
				throw std::runtime_error("Unable to stat "+path);
			else
				make=true;
		}
		
		if(make){
			int err=mkdir(path.c_str(),mode);
			if(err){
				err=errno;
				throw std::runtime_error("Unable to create directory "+path+": error "+std::to_string(err));
			}
		}
	};
	
	if(path.empty())
		throw std::logic_error("The empty path is not a valid argument to mkdir");
	if(mode>0777)
		throw std::logic_error("mkdir does not permit setting any mode bits above the lowest 9");
	size_t slashPos=path.find('/',1); //if the first character is a slash we don't care
	while(slashPos!=std::string::npos){
		std::string parentPath=path.substr(0,slashPos);
		makeIfNonexistent(parentPath);
		slashPos=path.find('/',slashPos+1);
	}
	if(path.back()!='/') //only necessary if the loop didn't already get everything
		makeIfNonexistent(path);
}

void ensureContainingDirectory(const std::string& path, uint16_t mode){
	auto slashPos=path.rfind('/');
	if(slashPos==std::string::npos) //no directory portion in path; can't do anything useful
		return;
	mkdir_p(path.substr(0,slashPos+1), mode);
}
	
bool storeCredential(std::string url, const credential& cred, bool overwrite){
	std::string path=getCredFilePath();
	PermState perms=checkPermissions(path);
	if(perms==PermState::INVALID)
		throw std::runtime_error("Credentials file "+path+" has wrong permissions; should be 0600");
	if(perms==PermState::DOES_NOT_EXIST){
		//File doesn't exist, so we don't need to worry about any existing contents.
		//However, we do need to create it with the right permissions. We should do
		//this before we write anything interesting to it.
		{
			ensureContainingDirectory(path,0700);
			//We need the file to exist to set its permissions, so first open it
			std::ofstream credFile(path);
			int err=chmod(path.c_str(),0600);
			if(err!=0){
				err=errno;
				throw std::runtime_error("Failed to set permissions for "+path);
			}
		}
		std::fstream credFile(path,std::ios_base::in|std::ios_base::out);
		writeCredentialRecord(credFile,url,cred);
		if(credFile.fail() || credFile.bad())
			throw std::runtime_error("Failed to write to "+path);
		return(true);
	}
	//otherwise, permissions are good, but we need to first scan the existing
	//file contents to see if the url is already present
	std::fstream credFile(path);
	if(!credFile) //this mostly shouldn't happen since we already checked the permissions
		throw std::runtime_error("Failed to open credentials file "+path+" for reading and writing");
	auto existingCreds=parseCredentials(credFile,path);
	credFile.clear();
	if(existingCreds.count(url)==0){
		//the url we want to write data for does not appear, so we can just append
		credFile.seekp(0,std::ios_base::end);
		writeCredentialRecord(credFile,url,cred);
		if(credFile.fail() || credFile.bad())
			throw std::runtime_error("Failed to write to "+path);
		return(true);
	}
	//otherwise, the target URL is already in the file
	if(!overwrite)
		return(false); //if we aren't supposed to overwrite it, just stop and complain
	//This leaves the annoying case: we need to remove the old entry before adding
	//the new one. The simplest way to do this is to update our in memory data, 
	//since we already read it all, and then rewrite it all to disk. 
	existingCreds[url]=cred;
	writeCredentials(existingCreds);
	return(true);
}
	
bool removeCredential(std::string url){
	//pull in all existing data
	CredentialCollection credentials=fetchStoredCredentials();
	//try to delete the target
	if(!credentials.erase(url))
		return(false); //if it wasn't there, we're done
	//rewrite remaining data
	writeCredentials(credentials);
	return(true);
}
	
credential findCredentials(const CredentialCollection& credentials,
                           const std::string& urlStr){
	std::string bestMatch;
	unsigned int maxPrefixLen=0;
	for(const auto& cred : credentials){
		if(urlStr.find(cred.first)==0 && cred.first.size()>maxPrefixLen){
			maxPrefixLen=cred.first.size();
			bestMatch=cred.first;
		}
	}
	if(!maxPrefixLen)
		throw std::runtime_error("No stored credentials found for URL "+urlStr);
	return(credentials.find(bestMatch)->second);
}
	
} //namespace s3tools
