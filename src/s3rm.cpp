#include <iostream>

#include <curl/curl.h>

#include <s3tools/url.h>
#include <s3tools/signing.h>
#include <s3tools/cred_manage.h>

#include "curl_utils.h"
#include "xml_utils.h"
#include "external/cl_options.h"

void removeObject(std::string target){
	auto credentials=s3tools::fetchStoredCredentials();
	auto cred=findCredentials(credentials,target).second;
	s3tools::URL signedURL=s3tools::genURL(cred.username,cred.key,"DELETE",target,60);
	
	std::string resultData;
	std::unique_ptr<CURL,void (*)(CURL*)> curlSession(curl_easy_init(),curl_easy_cleanup);
	CURLcode err;
	std::unique_ptr<char[]> errBuf(new char[CURL_ERROR_SIZE]);
	errBuf[0]=0;
	err=curl_easy_setopt(curlSession.get(), CURLOPT_ERRORBUFFER, errBuf.get());
	if(err!=CURLE_OK)
		std::cerr << "Failed to set curl error buffer" << std::endl;
	err=curl_easy_setopt(curlSession.get(), CURLOPT_URL, signedURL.str().c_str());
	if(err!=CURLE_OK)
		reportCurlError("Failed to set curl URL option",err,errBuf.get());
	err=curl_easy_setopt(curlSession.get(), CURLOPT_CUSTOMREQUEST, "DELETE");
	if(err!=CURLE_OK)
		reportCurlError("Failed to set curl DELETE option",err,errBuf.get());
	err=curl_easy_setopt(curlSession.get(), CURLOPT_WRITEFUNCTION, collectOutput);
	if(err!=CURLE_OK)
		reportCurlError("Failed to set curl output callback",err,errBuf.get());
	err=curl_easy_setopt(curlSession.get(), CURLOPT_WRITEDATA, &resultData);
	if(err!=CURLE_OK)
		reportCurlError("Failed to set curl output callback data",err,errBuf.get());
	err=curl_easy_perform(curlSession.get());
	if(err!=CURLE_OK)
		reportCurlError("curl perform DELETE failed",err,errBuf.get());
	
	if(!resultData.empty())
		handleXMLRepsonse(resultData,
		                  {},
		                  {{"NoSuchKey",[&](std::string){std::cerr << "Error: "+target+" does not exist.\n";}}}
		                  );
}

int main(int argc, char* argv[]){
	std::string usage=
R"(NAME
 s3rm - remove files from an S3 server
	
USAGE
 s3rm url [additional urls...]
    Erase each listed url from its respective server. 
	
NOTES
 Currently a separate request is made for each erasure. 

OPTIONS)";
	
	OptionParser op;
	op.setBaseUsage(usage);
	op.allowsOptionTerminator(true);
	auto arguments=op.parseArgs(argc,argv);
	
	if(op.didPrintUsage())
		return(0);
	if(arguments.size()<2){
		std::cout << op.getUsage() << std::endl;
		return(1);
	}
	//ignore the program name
	arguments.erase(arguments.begin());
	
	for(const std::string& target : arguments){
		try{
			removeObject(target);
		}catch(std::runtime_error& ex){
			std::cerr << "Error: " << ex.what() << std::endl;
			return(1);
		}
	}
}
