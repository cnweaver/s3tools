#include <iostream>

#include <s3tools/url.h>
#include <s3tools/signing.h>
#include <s3tools/cred_manage.h>
#include "external/cl_options.h"

int main(int argc, char* argv[]){
	std::string usage=
R"(NAME
 s3sign - generate a presigned URL for an object on an S3 server.
	
USAGE
 s3sign URL verb [validity_duration]
    Create a presigned URL valid for the given HTTP verb, and optionally valid
    for the given duration in seconds (if not specified, a default of one day is
    used).)";
	
	OptionParser op;
	op.setBaseUsage(usage);
	auto arguments=op.parseArgs(argc,argv);
	
	if(op.didPrintUsage())
		return(0);
	if(arguments.size()<3){
		std::cout << op.getUsage() << std::endl;
		return(1);
	}
	
	try{
		std::string baseURL=arguments[1];
		std::string verb=arguments[2];
		unsigned long validity=24UL*60*60; //seconds
		if(arguments.size()==4)
			validity=std::stoul(arguments[3]);
		auto credentials=s3tools::fetchStoredCredentials();
		auto cred=findCredentials(credentials,baseURL);
		s3tools::URL signedURL=s3tools::genURL(cred.username,cred.key,verb,baseURL,validity);
		std::cout << signedURL.str() << std::endl;
	}catch(std::exception& ex){
		std::cerr << "s3sign: error: " << ex.what() << std::endl;
		return(1);
	}
}
