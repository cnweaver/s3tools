#include <iostream>
#include <fstream>

#include <sys/stat.h> //for stat
#include <sys/errno.h> //for errno

#include <curl/curl.h>

#include <s3tools/url.h>
#include <s3tools/signing.h>
#include <s3tools/cred_manage.h>

#include "curl_utils.h"
#include "xml_utils.h"
#include "external/cl_options.h"

bool isURL(const std::string& s){
	try{
		s3tools::URL url(s);
		return(true);
	}catch(...){}
	return(false);
}

///Checks whther the path exists and is a directory
bool isDirectory(const std::string& path){
	struct stat data;
	int err=stat(path.c_str(),&data);
	if(err!=0){
		//Treat all errors as indicating that the ath is not a directory.
		//This is not entirely right since it might be an inaccessible regular 
		//file or there might have been a true failure of stat(), but for our 
		//use it mostly doesn't matter; we'll just fail when we try to write to 
		//the file
		return(false);
	}
	return((data.st_mode&S_IFMT)==S_IFDIR);
}

auto writeOutput(void* buffer, size_t size, size_t nmemb, void* userp)->size_t{
	std::ostream* out=static_cast<std::ostream*>(userp);
	out->write((char*)buffer,size*nmemb);
	if(!out){
		std::cerr << "Error writing output data" << std::endl;
		return(size*nmemb?0:1); //return a different number to indicate error
	}
	return(size*nmemb);//return full size to indicate success
};
		
auto readInput(char* buffer, size_t size, size_t nitems, void* instream)->size_t{
	std::istream* in=static_cast<std::istream*>(instream);
	if(in->eof())
		return(0);
	in->read((char*)buffer, size*nitems);
	if(in->fail() && !in->eof()){
		std::cerr << "Error reading input data" << std::endl;
		return(CURL_READFUNC_ABORT);
	}
	return(in->gcount());
}
		
void serversideCopy(std::string src, std::string dest, 
                    const s3tools::CredentialCollection& credentials, bool verbose){
	auto cred=findCredentials(credentials,dest).second;
	s3tools::URL destURL(dest);
	s3tools::URL sourceURL(src);
	if(destURL.host != sourceURL.host)
		throw std::runtime_error("Cannot do a server-side copy between two different hosts");
	//TODO: in real S3 if the source object is greater than 5GB in size this
	//will fail and multipart uploading should be used instead
	destURL.headers["x-amz-copy-source"]=sourceURL.path;
	destURL.headers["x-amz-content-sha256"]=s3tools::lowercase(s3tools::SHA256Hash(""));
	s3tools::URL signedURL=s3tools::genURLNoQuery(cred.username,cred.key,"PUT",destURL,60);
	
	std::string resultData;
	std::unique_ptr<CURL,void (*)(CURL*)> curlSession(curl_easy_init(),curl_easy_cleanup);
	CURLcode err;
	std::unique_ptr<char[]> errBuf(new char[CURL_ERROR_SIZE]);
	errBuf[0]=0;
	
	err=curl_easy_setopt(curlSession.get(), CURLOPT_ERRORBUFFER, errBuf.get());
	if(err!=CURLE_OK)
		throw std::runtime_error("Failed to set curl error buffer");
	err=curl_easy_setopt(curlSession.get(), CURLOPT_URL, signedURL.str().c_str());
	if(err!=CURLE_OK)
		reportCurlError("Failed to set curl URL option",err,errBuf.get());
	err=curl_easy_setopt(curlSession.get(), CURLOPT_UPLOAD, 1);
	if(err!=CURLE_OK)
		reportCurlError("Failed to set curl PUT/upload option",err,errBuf.get());
	err=curl_easy_setopt(curlSession.get(), CURLOPT_INFILESIZE_LARGE, (curl_off_t)0);
	if(err!=CURLE_OK)
		reportCurlError("Failed to set curl input data size",err,errBuf.get());
	err=curl_easy_setopt(curlSession.get(), CURLOPT_WRITEFUNCTION, collectOutput);
	if(err!=CURLE_OK)
		reportCurlError("Failed to set curl output callback",err,errBuf.get());
	err=curl_easy_setopt(curlSession.get(), CURLOPT_WRITEDATA, &resultData);
	if(err!=CURLE_OK)
		reportCurlError("Failed to set curl output callback data",err,errBuf.get());
	if(verbose){
		err=curl_easy_setopt(curlSession.get(), CURLOPT_NOPROGRESS, 0);
		if(err!=CURLE_OK)
			reportCurlError("Failed to set curl progress indicator",err,errBuf.get());
	}
	std::unique_ptr<curl_slist,void (*)(curl_slist*)> headerList(nullptr,curl_slist_free_all);
	for(const auto& header : signedURL.headers)
		headerList.reset(curl_slist_append(headerList.release(),(header.first+":"+header.second).c_str()));
	//suppress unwanted 'Accept' header
	headerList.reset(curl_slist_append(headerList.release(),"Accept:"));
	err=curl_easy_setopt(curlSession.get(), CURLOPT_HTTPHEADER, headerList.get());
	if(err!=CURLE_OK)
		reportCurlError("Failed to set request headers",err,errBuf.get());
	err=curl_easy_perform(curlSession.get());
	if(err!=CURLE_OK)
		reportCurlError("curl perform GET failed",err,errBuf.get());
	
	handleXMLRepsonse(resultData,
	                  {{"CopyObjectResult",[](xmlNode*){/*Cool. Nothing to do.*/}}},
	                  {{"NoSuchKey",[&](std::string){std::cerr << "Error: The source object "+src+" does not exist.\n";}}}
	                  );
}
		
void downloadFile(std::string src, std::string dest, 
                  const s3tools::CredentialCollection& credentials, bool verbose){
	auto cred=findCredentials(credentials,src).second;
	s3tools::URL signedURL=s3tools::genURL(cred.username,cred.key,"GET",src,60);
	
	//if writing to a directory, figure out the basename of the source file
	//and append it. 
	if(isDirectory(dest)){
		std::string src=signedURL.path;
		//just assume POSIX path syntax
		size_t lastSlash=src.rfind('/');
		if(lastSlash==std::string::npos) //no slash, take the whole thing
			dest+="/"+src;
		else if(lastSlash+1==src.size()){
			//we don't yet know how to download 'directories'!
			throw std::runtime_error("Source path does not appear to be a single file");
		}
		else
			dest+=src.substr(lastSlash);
	}
	
	std::ofstream outfile(dest,std::ios::out|std::ios::binary);
	if(!outfile)
		throw std::runtime_error("Unable to open "+dest+" for writing");
	
	std::unique_ptr<CURL,void (*)(CURL*)> curlSession(curl_easy_init(),curl_easy_cleanup);
	CURLcode err;
	std::unique_ptr<char[]> errBuf(new char[CURL_ERROR_SIZE]);
	errBuf[0]=0;
	
	err=curl_easy_setopt(curlSession.get(), CURLOPT_ERRORBUFFER, errBuf.get());
	if(err!=CURLE_OK)
		throw std::runtime_error("Failed to set curl error buffer");
	err=curl_easy_setopt(curlSession.get(), CURLOPT_URL, signedURL.str().c_str());
	if(err!=CURLE_OK)
		reportCurlError("Failed to set curl URL option",err,errBuf.get());
	err=curl_easy_setopt(curlSession.get(), CURLOPT_HTTPGET, 1);
	if(err!=CURLE_OK)
		reportCurlError("Failed to set curl GET option",err,errBuf.get());
	err=curl_easy_setopt(curlSession.get(), CURLOPT_WRITEFUNCTION, writeOutput);
	if(err!=CURLE_OK)
		reportCurlError("Failed to set curl output callback",err,errBuf.get());
	err=curl_easy_setopt(curlSession.get(), CURLOPT_WRITEDATA, (std::ostream*)&outfile);
	if(err!=CURLE_OK)
		reportCurlError("Failed to set curl output callback data",err,errBuf.get());
	if(verbose){
		err=curl_easy_setopt(curlSession.get(), CURLOPT_NOPROGRESS, 0);
		if(err!=CURLE_OK)
			reportCurlError("Failed to set curl progress indicator",err,errBuf.get());
	}
	err=curl_easy_perform(curlSession.get());
	if(err!=CURLE_OK)
		reportCurlError("curl perform GET failed",err,errBuf.get());
}
		
void uploadFile(std::string src, std::string dest, 
                const s3tools::CredentialCollection& credentials, bool verbose){
	auto cred=findCredentials(credentials,dest).second;
	s3tools::URL signedURL=s3tools::genURL(cred.username,cred.key,"PUT",dest,60);
	
	//we don't yet know how to upload directories!
	if(isDirectory(src))
		throw std::runtime_error("Source path does not appear to be a single file");
	
	std::ifstream infile(src,std::ios::out|std::ios::binary);
	if(!infile)
		throw std::runtime_error("Unable to open "+src+" for reading");
	infile.seekg(0,std::ios_base::end);
	std::streampos file_size=infile.tellg();
	infile.seekg(0,std::ios_base::beg);
	
	std::unique_ptr<CURL,void (*)(CURL*)> curlSession(curl_easy_init(),curl_easy_cleanup);
	CURLcode err;
	std::unique_ptr<char[]> errBuf(new char[CURL_ERROR_SIZE]);
	errBuf[0]=0;
	
	err=curl_easy_setopt(curlSession.get(), CURLOPT_ERRORBUFFER, errBuf.get());
	if(err!=CURLE_OK)
		throw std::runtime_error("Failed to set curl error buffer");
	err=curl_easy_setopt(curlSession.get(), CURLOPT_URL, signedURL.str().c_str());
	if(err!=CURLE_OK)
		reportCurlError("Failed to set curl URL option",err,errBuf.get());
	err=curl_easy_setopt(curlSession.get(), CURLOPT_UPLOAD, 1);
	if(err!=CURLE_OK)
		reportCurlError("Failed to set curl PUT/upload option",err,errBuf.get());
	err=curl_easy_setopt(curlSession.get(), CURLOPT_READFUNCTION, readInput);
	if(err!=CURLE_OK)
		reportCurlError("Failed to set curl input callback",err,errBuf.get());
	err=curl_easy_setopt(curlSession.get(), CURLOPT_READDATA, (std::istream*)&infile);
	if(err!=CURLE_OK)
		reportCurlError("Failed to set curl input callback data",err,errBuf.get());
	err=curl_easy_setopt(curlSession.get(), CURLOPT_INFILESIZE_LARGE, (curl_off_t)file_size);
	if(err!=CURLE_OK)
		reportCurlError("Failed to set curl input data size",err,errBuf.get());
	if(verbose){
		err=curl_easy_setopt(curlSession.get(), CURLOPT_NOPROGRESS, 0);
		if(err!=CURLE_OK)
			reportCurlError("Failed to set curl progress indicator",err,errBuf.get());
	}
	err=curl_easy_perform(curlSession.get());
	if(err!=CURLE_OK)
		reportCurlError("curl perform PUT failed",err,errBuf.get());
}

int main(int argc, char* argv[]){
	std::string usage=R"(NAME
 s3cp - copy files to or from an S3 server
	
USAGE
 s3ls [-v] source destination
    One of source and destination must be a remote URL, and both may be also (a
    server-side copy).

OPTIONS)";
	bool verbose=false;
	OptionParser op;
	op.setBaseUsage(usage);
	op.addOption({"v","verbose"},[&]{verbose=true;},
				 "Show incremental progress.");
	op.allowsOptionTerminator(true);
	auto arguments=op.parseArgs(argc,argv);
	
	if(op.didPrintUsage())
		return(0);
	if(arguments.size()!=3){
		std::cerr << "Wrong number of arguments" << std::endl;
		std::cout << op.getUsage() << std::endl;
		return(1);
	}
	
	std::string src=arguments[1];
	std::string dest=arguments[2];
	bool srcIsURL=isURL(src);
	bool destIsURL=isURL(dest);
	if(!srcIsURL && !destIsURL){
		std::cerr << "Either the source or the destination must be a URL" << std::endl;
		return(1);
	}
	
	auto credentials=s3tools::fetchStoredCredentials();
	
	if(srcIsURL && destIsURL){ //server side copy
		try{
			serversideCopy(src,dest,credentials,verbose);
		}catch(std::exception& ex){
			std::cerr << ex.what() << std::endl;
			return(1);
		}
	}
	else if(srcIsURL){ //downloading
		try{
			downloadFile(src,dest,credentials,verbose);
		}catch(std::exception& ex){
			std::cerr << ex.what() << std::endl;
			return(1);
		}
	}
	else{ //uploading
		try{
			uploadFile(src,dest,credentials,verbose);
		}catch(std::exception& ex){
			std::cerr << ex.what() << std::endl;
			return(1);
		}
	}
}
