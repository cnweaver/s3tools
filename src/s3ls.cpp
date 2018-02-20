#include <cmath>
#include <iostream>
#include <fstream>
#include <sstream>

#include <curl/curl.h>

#include <s3tools/url.h>
#include <s3tools/signing.h>
#include <s3tools/cred_manage.h>

#include "curl_utils.h"
#include "xml_utils.h"
#include "external/cl_options.h"
	
struct optionsType{
	bool verbose;
	bool readableSizes;
};
		
///\return the continuation token, if any
void parseListAllBucketsResult(xmlNode* node, const optionsType& options){
	xmlNode* buckets=firstChild(node,"Buckets");
	if(!buckets)
		return;
	for(xmlNode* bucket=firstChild(buckets,"Bucket"); bucket; bucket=nextSibling(bucket,"Bucket")){
		xmlNode* name=firstChild(bucket,"Name");
		if(name)
			std::cout << getNodeContents<std::string>(name) << '/';
		if(options.verbose){
			xmlNode* ctime=firstChild(bucket,"CreationDate");
			if(ctime)
				std::cout << "\t " << getNodeContents<std::string>(ctime);
		}
		std::cout << std::endl;
	}
}
	
///\return the continuation token, if any
void parseListBucketResult(xmlNode* node, const optionsType& options){
	for(xmlNode* prefixes=firstChild(node,"CommonPrefixes"); prefixes; prefixes=nextSibling(prefixes,"CommonPrefixes")){
		xmlNode* prefix=firstChild(prefixes,"Prefix");
		if(prefix)
			std::cout << getNodeContents<std::string>(prefix) << std::endl;
	}
	if(options.readableSizes)
		std::cout.precision(2);
	for(xmlNode* content=firstChild(node,"Contents"); content; content=nextSibling(content,"Contents")){
		xmlNode* key=firstChild(content,"Key");
		if(key)
			std::cout << getNodeContents<std::string>(key);
		if(options.verbose){
			xmlNode* mtime=firstChild(content,"LastModified");
			if(mtime)
				std::cout << "\t " << getNodeContents<std::string>(mtime);
			xmlNode* size=firstChild(content,"Size");
			if(size){
				if(options.readableSizes){
					unsigned long isize=getNodeContents<unsigned long>(size);
					const static std::string suffixes="BKMGTPE";
					unsigned int index=0;
					double frac=0;
					while(isize>=(1UL<<10)){
						index++;
						frac=fmod(isize+frac,1024.)/1024.;
						isize>>=10;
					}
					if(index)
						std::cout << "\t " << isize+frac << suffixes[index];
					else
						std::cout << "\t " << isize << suffixes[index];
				}
				else
					std::cout << "\t " << getNodeContents<std::string>(size);
			}
		}
		std::cout << std::endl;
	}
}
		
///\return the continuation token, if any
std::string parseXML(const std::string& raw, const optionsType& options){
	//xmlSetStructuredErrorFunc(this,&xmlErrorCallback);
	std::unique_ptr<xmlDoc,void(*)(xmlDoc*)> tree(xmlReadDoc((const xmlChar*)raw.c_str(),NULL,NULL,XML_PARSE_RECOVER|XML_PARSE_PEDANTIC),&xmlFreeDoc);
	/*if(!tree || fatalErrorOccurred){
		errors.addError(mbID,error(FATAL_ERROR,"Failed to parse '"+file+"'"));
		return(data);
	}*/
	xmlNode* root = xmlDocGetRootElement(tree.get());
	if(!root){
		std::cerr << "Got invalid XML data: " << std::endl;
		std::cerr << raw << std::endl;
		return("");
	}
	std::string nodeName((char*)root->name);
	if(nodeName=="ListAllMyBucketsResult")
		parseListAllBucketsResult(root,options);
	else if(nodeName=="ListBucketResult")
		parseListBucketResult(root,options);
	else if(nodeName=="Error"){
		std::cout << "Error: " << std::endl;
		xmlNode* code=firstChild(root,"Code");
		if(code)
			std::cout << " Code: " << getNodeContents<std::string>(code) << std::endl;
		xmlNode* message=firstChild(root,"Message");
		if(message)
			std::cout << " Message: " << getNodeContents<std::string>(message) << std::endl;
	}
	
	xmlNode* truncated=firstChild(root,"IsTruncated");
	if(truncated && getNodeContents<std::string>(truncated)=="true"){
		xmlNode* continuation=firstChild(root,"NextContinuationToken");
		if(!continuation)
			throw std::runtime_error("Result contains <IsTruncated> but not <NextContinuationToken>");
		return(getNodeContents<std::string>(continuation));
	}
	return("");
}

void list(const std::string& target, const s3tools::CredentialCollection& credentials, 
          const optionsType& options, CURL* curlSession, char* errBuf){
	auto cred=findCredentials(credentials,target);
	s3tools::URL basicURL(target);
	basicURL.query["list-type"]="2";
	basicURL.query["delimiter"]="/";
	{ //try to pick apart the path into bucket name and prefix parts
		std::regex path_regex(R"((/[^/]*)(/(.*))?)",std::regex::extended);
		std::smatch matches;
		if(!std::regex_match(basicURL.path,matches,path_regex))
			std::cerr << "Oops, path regex did not match" << std::endl;
		if(matches.size()==4){
			basicURL.query["prefix"]=matches[3].str(); //the prefix
			basicURL.path=matches[1].str(); //the bucket
		}
	}
	std::string continuation;
	
	do{
		s3tools::URL signedURL=s3tools::genURL(cred.username,cred.key,"GET",basicURL.str(),60);
		std::string resultData;
		
		CURLcode err;
		err=curl_easy_setopt(curlSession, CURLOPT_URL, signedURL.str().c_str());
		if(err!=CURLE_OK)
			reportCurlError("Failed to set curl URL option",err,errBuf);
		err=curl_easy_setopt(curlSession, CURLOPT_HTTPGET, 1);
		if(err!=CURLE_OK)
			reportCurlError("Failed to set curl GET option",err,errBuf);
		err=curl_easy_setopt(curlSession, CURLOPT_WRITEFUNCTION, collectOutput);
		if(err!=CURLE_OK)
			reportCurlError("Failed to set curl output callback",err,errBuf);
		err=curl_easy_setopt(curlSession, CURLOPT_WRITEDATA, &resultData);
		if(err!=CURLE_OK)
			reportCurlError("Failed to set curl output callback data",err,errBuf);
		err=curl_easy_perform(curlSession);
		if(err!=CURLE_OK)
			reportCurlError("curl perform GET failed",err,errBuf);
		
		continuation=parseXML(resultData,options);
		if(!continuation.empty())
			basicURL.query["continuation-token"]=continuation;
	}while(!continuation.empty());
}

int main(int argc, char* argv[]){
	optionsType options;
	options.verbose=false;
	options.readableSizes=false;
	bool didPrintHelp=false;
	std::string usage=
R"(NAME
 s3ls - list files on an S3 server
	
USAGE
 s3ls [-hl] url [additional urls...]

OPTIONS)";
	
	OptionParser op(false);
	op.setBaseUsage(usage);
	op.addOption({"?","help","usage"},[&]{
		didPrintHelp=true;
		std::cout << op.getUsage() << std::endl;
	},"Print usage information.");
	op.addOption({"l","long"},[&]{options.verbose=true;},
				 "List in long format including sizes and modification times");
	op.addOption('h',[&]{options.readableSizes=true;},
				 "Use unit suffixes for sizes");
	op.allowsShortOptionCombination(true);
	op.allowsOptionTerminator(true);
	auto arguments=op.parseArgs(argc,argv);
	
	if(didPrintHelp)
		return(0);
	if(arguments.size()<2){
		std::cout << op.getUsage() << std::endl;
		return(1);
	}
	//ignore the program name
	arguments.erase(arguments.begin());
	
	std::cout.precision(3);
	std::cout.setf(std::ios::floatfield,std::ios::fixed);
	auto credentials=s3tools::fetchStoredCredentials();
	
	std::unique_ptr<CURL,void (*)(CURL*)> curlSession(curl_easy_init(),curl_easy_cleanup);
	CURLcode err;
	std::unique_ptr<char[]> errBuf(new char[CURL_ERROR_SIZE]);
	errBuf[0]=0;
	err=curl_easy_setopt(curlSession.get(), CURLOPT_ERRORBUFFER, errBuf.get());
	if(err!=CURLE_OK){
		std::cerr << "Failed to set curl error buffer" << std::endl;
		return(1);
	}

	for(const std::string& target : arguments){
		try{
			list(target,credentials,options,curlSession.get(),errBuf.get());
		}catch(std::exception& err){
			std::cerr << err.what() << std::endl;
		}
	}
}
