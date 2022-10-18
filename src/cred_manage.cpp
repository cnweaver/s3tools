#include <s3tools/cred_manage.h>

#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>

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

CredentialCollection parseCredentialsJSON(std::istream& credFile, const std::string& path){
	//We work with only a subset of JSON: The text must be an array of objects,
	//and the values contained in the objects must be strings. Additionally, we
	//tolerate a superset of the whitespace permitted by the JSON specification.

	auto startPos=credFile.tellg();
	
	auto skipWhitespace=[&credFile,&path]()->void{
		while(credFile && std::isspace(credFile.peek()))
			credFile.get();
	};
	auto checkUnexpectedEnd=[&credFile,&startPos,&path]()->void{
		if(!credFile){
			auto offset=credFile.tellg()-startPos;
			throw std::runtime_error("Unexpected read failure or end of file at offset "
			                         +std::to_string(offset)+" of "+path);
		}
	};
	
	auto parseString=[&credFile,&path,&startPos,&checkUnexpectedEnd]()->std::string{
		auto validHexDigit=[](char c)->bool{
			return (c>='0' && c<='9') || (c>='A' && c<='F') || (c>='a' && c<='f');
		};
		auto hexValue=[](char c)->unsigned int{
			//assumes that validHexDigit(c) is true
			if(c<='9')
				return c-'0';
			if(c<='F')
				return c-'A'+10;
			return c-'a'+10;
		};
		
		enum StringState{
			StringStart,
			InString,
			InEscape,
			InUTF16Escape,
		} stringState=StringStart;
		
		std::string stringData;
		bool stringDone=false;
		char c;
		auto nextChar=[&](){
			credFile.get(c);
			checkUnexpectedEnd();
		};
		while(!stringDone){
			nextChar();
			switch(stringState){
				case StringStart:
					if(c!='"'){
						auto offset=credFile.tellg()-startPos;
						throw std::runtime_error("Unexpected character '"+std::string(1,c)+
						                         "' where open quote for string was expected at offset "+
						                         std::to_string(offset)+" of "+path);
					}
					stringState=InString;
					break;
				case InString:
					if(c=='"')
						stringDone=true;
					else if(c=='\\')
						stringState=InEscape;
					else
						stringData+=c;
					break;
				case InEscape:
					switch(c){
						case '"': stringData+='"'; break;
						case '\\': stringData+='\\'; break;
						case '/': stringData+='/'; break;
						case 'b': stringData+='\b'; break;
						case 'f': stringData+='\f'; break;
						case 'n': stringData+='\n'; break;
						case 'r': stringData+='\r'; break;
						case 't': stringData+='\t'; break;
						case 'u': stringState=InUTF16Escape; break;
						default:{
							auto offset=credFile.tellg()-startPos;
							throw std::runtime_error("Unknown escape sequence character '"+std::string(1,c)+
							                         "' at offset "+std::to_string(offset)+" of "+path);
						}
					}
					break;
				case InUTF16Escape:{
					//grab four hex digits and compute their 16 bit value
					uint16_t accum=0;
					for(unsigned int i=0; i<4; i++){
						if(i)
							nextChar();
						if(!validHexDigit(c)){
							auto offset=credFile.tellg()-startPos;
							throw std::runtime_error("Invalid character '"+std::string(1,c)+
							                         "' for hexadecimal digit in UTF16 escape at offset "+
							                         std::to_string(offset)+" of "+path);
						}
						accum=(accum<<4)+hexValue(c);
					}
					//decompose the 16 bit value into one or two 8-bit characters
					if(accum>0xFF)
						stringData+=(unsigned char)(accum>>8);
					stringData+=(unsigned char)(accum&0xFF);
					stringState=InString;
				} break;
			} //switch
		} //while
		return stringData;
	};
	auto parseRecord=[&]()->std::pair<std::string,credential>{
		//read the JSON into a raw data map
		std::map<std::string,std::string> rawData;
		auto recordStart=credFile.tellg();
		char c;
		while(true){
			//Member key:
			skipWhitespace();
			checkUnexpectedEnd();
			std::string key=parseString();
			//Name separator
			credFile >> c;
			checkUnexpectedEnd();
			if(c!=':'){
				auto offset=credFile.tellg()-startPos;
				throw std::runtime_error("Unexpected character '"+std::string(1,c)+
				                         "' where a name-separator for an object member was expected at offset "+
				                         std::to_string(offset)+" of "+path);
			}
			//Member value:
			skipWhitespace();
			checkUnexpectedEnd();
			std::string value=parseString();
			rawData[key]=value;
			//Value separator or end of object:
			credFile >> c;
			checkUnexpectedEnd();
			if(c=='}')
				break;
			else if(c!=','){
				auto offset=credFile.tellg()-startPos;
				throw std::runtime_error("Unexpected character '"+std::string(1,c)+
				                         "' where a value-separator between object members was expected at offset "+
				                         std::to_string(offset)+" of "+path);
			}
		}
		//turn the raw data map into a credential record, if possible
		auto checkKey=[&](const std::string& key){
			if(rawData.find(key)==rawData.end()){
				auto offset=recordStart-startPos;
				throw std::runtime_error("Missing required key \""+key+
				                         "\" from credential record starting at offset "+
				                         std::to_string(offset)+" of "+path);
			}
		};
		checkKey("url");
		checkKey("username");
		checkKey("key");
		
		return std::make_pair(rawData["url"],credential{rawData["username"],rawData["key"]});
	};
	
	CredentialCollection credentials;
	char c;
	credFile >> c;
	if(!credFile || c!='[')
		throw std::runtime_error(path+" does not contain a JSON array");
	while(true){
		credFile >> c;
		checkUnexpectedEnd();
		if(c=='{'){
			auto record=parseRecord();
			credentials.insert(record);
		}
		else if(c==']')
			break; //if the array ends, we're done
		credFile >> c;
		checkUnexpectedEnd();
		if(c==',')
			continue;
		else if(c==']')
			break;
		else{
			auto offset=credFile.tellg()-startPos;
			throw std::runtime_error("Unexpected character '"+std::string(1,c)+
			                         "' after array item at offset "+
			                         std::to_string(offset)+" of "+path);
		}
	}
	return credentials;
}

CredentialCollection readCredentials(std::istream& credFile, const std::string& path){
	//Per RFC 3986 §3.1, https://datatracker.ietf.org/doc/html/rfc3986#section-3.1 ,
	//a URI scheme cannot contain an opening brace, and in fact must begin with a
	//letter. A JSON array must begin with a bracket, so checking whether the first
	//non-whitespace character is a bracket fully disambiguates between the two formats.
	char c;
	while(credFile && std::isspace(c=credFile.peek()))
		credFile.get();
	if(c=='[')
		return parseCredentialsJSON(credFile, path);
	else if(std::isalpha(c))
		return parseCredentials(credFile, path);
	else
		throw std::runtime_error("Unable to recognize the format of "+path);
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

void writeCredentials(std::ostream& os, const CredentialCollection& credentials){
	for(const auto& record : credentials)
		writeCredentialRecord(os,record.first,record.second);
}

void writeCredentialsJSON(std::ostream& os, const CredentialCollection& credentials){
	auto jsonSafeString=[](const std::string input)->std::string{
		//"characters that MUST be escaped:
		//quotation mark, reverse solidus, and the control characters (U+0000
		//through U+001F)"
		std::ostringstream output;
		output << std::setfill('0');
		for(char c : input){
			if(c=='"')
				output << R"(\")";
			else if(c=='\\')
				output << R"(\\)";
			else if(c<=0x1F){
				output << "\\u00" << std::setw(2) << (int)c;
			}
			else
				output << c;
		}
		return output.str();
	};
	
	os << '[';
	bool first=true;
	for(const auto& record : credentials){
		if(first)
			first=false;
		else
			os << ',';
		os << "{\"url\":\"" << jsonSafeString(record.first) << "\",\"username\":\"" 
		   << jsonSafeString(record.second.username) << "\",\"key\":\""
		   << jsonSafeString(record.second.key) << "\"}";
	}
	os << "]\n";
}

///Overwrite all stored credentials with a different collection
///\pre file permissions requirements must already have been enforced on the
///     credential file (exists, owned by correct user, permissions are 0600).
void writeCredentialsToDefaultLocation(const CredentialCollection& credentials){
	std::string path=getCredFilePath();
	//permissions were already enforced by fetchStoredCredentials
	std::ofstream credFile(path);
	if(!credFile) //this mostly shouldn't happen since we already checked the permissions
		throw std::runtime_error("Failed to open credentials file "+path+" for writing");
	writeCredentials(credFile, credentials);
	if(credFile.fail() || credFile.bad())
		throw std::runtime_error("Failed to write to "+path);
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

//should not be used on an existing file
void setCredentialFilePerms(const std::string& path){
	//File doesn't exist, so we don't need to worry about any existing contents.
	//However, we do need to create it with the right permissions. We should do
	//this before we write anything interesting to it.
	ensureContainingDirectory(path,0700);
	//We need the file to exist to set its permissions, so first open it
	std::ofstream credFile(path);
	int err=chmod(path.c_str(),0600);
	if(err!=0){
		err=errno;
		throw std::runtime_error("Failed to set permissions for "+path);
	}
}
	
bool storeCredential(std::string url, const credential& cred, bool overwrite){
	std::string path=getCredFilePath();
	PermState perms=checkPermissions(path);
	if(perms==PermState::INVALID)
		throw std::runtime_error("Credentials file "+path+" has wrong permissions; should be 0600");
	if(perms==PermState::DOES_NOT_EXIST){
		setCredentialFilePerms(path);
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
	writeCredentialsToDefaultLocation(existingCreds);
	return(true);
}
	
bool removeCredential(std::string url){
	//pull in all existing data
	CredentialCollection credentials=fetchStoredCredentials();
	//try to delete the target
	if(!credentials.erase(url))
		return(false); //if it wasn't there, we're done
	//rewrite remaining data
	writeCredentialsToDefaultLocation(credentials);
	return(true);
}
	
std::pair<std::string,credential> findCredentials(const CredentialCollection& credentials,
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
	return(*credentials.find(bestMatch));
}

void exportCredentials(std::ostream& targetStream,
                       const CredentialCollection& credentials,
                       CredFormat format){
	switch(format){
		case CredFormat::Internal:
			writeCredentials(targetStream, credentials);
			break;
		case CredFormat::JSON:
		   writeCredentialsJSON(targetStream, credentials);
		   break;
	}
}

void exportCredentials(std::ostream& targetStream,
                       const CredentialCollection& credentials,
                       const std::string& selectedURL,
                       CredFormat format){
	auto it=credentials.find(selectedURL);
	if(it==credentials.end())
		throw std::runtime_error("Requested credential does not exist");
	CredentialCollection selected;
	selected.insert(*it);
	exportCredentials(targetStream, selected, format);
}

int importCredentials(std::istream& inputData,
                      const std::string& sourceDesc,
                      bool overwrite){
	//try to read new data
	CredentialCollection newCreds=readCredentials(inputData, sourceDesc);
	
	//pull in all existing data
	CredentialCollection existingCreds=fetchStoredCredentials();
	
	//merge data
	int added=0;
	for(const auto& record : newCreds){
		if(!overwrite){ //if we are not supposed to overwrite, skip existing records
			if(existingCreds.find(record.first)!=existingCreds.end())
				continue;
		}
		existingCreds[record.first]=record.second;
		added++;
	}
	
	//write back out
	if(added){
		std::string path=getCredFilePath();
		PermState perms=checkPermissions(path);
		if(perms==PermState::INVALID)
			throw std::runtime_error("Credentials file "+path+" has wrong permissions; should be 0600");
		if(perms==PermState::DOES_NOT_EXIST)
			setCredentialFilePerms(path);
		writeCredentialsToDefaultLocation(existingCreds);
	}
	return added;
}

} //namespace s3tools
