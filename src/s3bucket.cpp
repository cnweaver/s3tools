#include <iostream>
#include <string>

#include <curl/curl.h>

#include <s3tools/cred_manage.h>
#include <s3tools/signing.h>
#include <s3tools/url.h>

#include "curl_utils.h"
#include "xml_utils.h"
#include "external/cl_options.h"

//Duplicate code from s3ls, factor out!
struct optionsType{
	bool verbose;
	bool readableSizes;
};
		
///\return the continuation token, if any
std::string parseListAllBucketsResult(xmlNode* node, const optionsType& options){
	xmlNode* buckets=firstChild(node,"Buckets");
	if(!buckets)
		return("");
	for(xmlNode* bucket=firstChild(buckets,"Bucket"); bucket; bucket=nextSibling(bucket,"Bucket")){
		xmlNode* name=firstChild(bucket,"Name");
		if(name)
			std::cout << getNodeContents<std::string>(name);
		if(options.verbose){
			xmlNode* ctime=firstChild(bucket,"CreationDate");
			if(ctime)
				std::cout << "\t " << getNodeContents<std::string>(ctime);
		}
		std::cout << std::endl;
	}
	xmlNode* truncated=firstChild(node,"IsTruncated");
	if(truncated && getNodeContents<std::string>(truncated)=="true"){
		xmlNode* continuation=firstChild(node,"NextContinuationToken");
		if(!continuation)
			throw std::runtime_error("Result contains <IsTruncated> but not <NextContinuationToken>");
		return(getNodeContents<std::string>(continuation));
	}
	return("");
}

bool listBuckets(const std::string rawURL, optionsType options){
	auto credentials=s3tools::fetchStoredCredentials();
	auto cred=findCredentials(credentials,rawURL).second;
	s3tools::URL basicURL(rawURL);
	
	std::string continuation;
	std::unique_ptr<CURL,void (*)(CURL*)> curlSession(curl_easy_init(),curl_easy_cleanup);
	CURLcode err;
	std::unique_ptr<char[]> errBuf(new char[CURL_ERROR_SIZE]);
	errBuf[0]=0;
	err=curl_easy_setopt(curlSession.get(), CURLOPT_ERRORBUFFER, errBuf.get());
	if(err!=CURLE_OK){
		std::cerr << "Failed to set curl error buffer" << std::endl;
		return(false);
	}
#ifdef USE_CURLOPT_CAINFO
	std::string caBundlePath=detectCABundlePath();
#endif
	do{
		s3tools::URL signedURL=s3tools::genURL(cred.username,cred.key,"GET",basicURL.str(),60);
		std::string resultData;
		err=curl_easy_setopt(curlSession.get(), CURLOPT_URL, signedURL.str().c_str());
		if(err!=CURLE_OK)
			reportCurlError("Failed to set curl URL option",err,errBuf.get());
		err=curl_easy_setopt(curlSession.get(), CURLOPT_HTTPGET, 1);
		if(err!=CURLE_OK)
			reportCurlError("Failed to set curl GET option",err,errBuf.get());
		err=curl_easy_setopt(curlSession.get(), CURLOPT_WRITEFUNCTION, collectOutput);
		if(err!=CURLE_OK)
			reportCurlError("Failed to set curl output callback",err,errBuf.get());
		err=curl_easy_setopt(curlSession.get(), CURLOPT_WRITEDATA, &resultData);
		if(err!=CURLE_OK)
			reportCurlError("Failed to set curl output callback data",err,errBuf.get());
#ifdef USE_CURLOPT_CAINFO
		if(!caBundlePath.empty()){
			err=curl_easy_setopt(curlSession.get(), CURLOPT_CAINFO, caBundlePath.c_str());
			if(err!=CURLE_OK)
				reportCurlError("Failed to set curl CA bundle path",err,errBuf.get());
		}
#endif
		err=curl_easy_perform(curlSession.get());
		if(err!=CURLE_OK)
			reportCurlError("curl perform GET failed",err,errBuf.get());
		
		handleXMLRepsonse(resultData,
		         {
					 {"ListAllMyBucketsResult",[&](xmlNode* node){continuation=parseListAllBucketsResult(node,options);}}
				 });
		if(!continuation.empty())
			basicURL.query["continuation-token"]=continuation;
	}while(!continuation.empty());
	return(true);
}
	
///Compare a potential bucket name to the rules for allowed names:
///https://docs.aws.amazon.com/AmazonS3/latest/dev/BucketRestrictions.html
bool validateBucketName(const std::string& bucket){
	//"Bucket names must be at least 3 and no more than 63 characters long."
	if(bucket.size()<3 || bucket.size()>63)
		return(false);
	//"Bucket names must be a series of one or more labels. Adjacent labels are 
	//separated by a single period (.). Bucket names can contain lowercase 
	//letters, numbers, and hyphens. Each label must start and end with a 
	//lowercase letter or a number."
	//We will ensure this by forbidding characters outside [a-z0-9-.], that the
	//name neither starts, nor ends with '.', and does not contain ".."
	static const char* allowed_characters="abcdefghijklmnopqrstuvwxyz0123456789-.";
	if(bucket.find_first_not_of(allowed_characters)!=std::string::npos)
		return(false);
	if(bucket.front()=='.' || bucket.back()=='.')
		return(false);
	if(bucket.find("..")!=std::string::npos)
		return(false);
	//"Bucket names must not be formatted as an IP address (for example, 192.168.5.4)."
	//Assumedly IPv4 decimal notation is referenced directly because the allowed 
	//character rule forbids standard IPv6 represenations due to their use of ':'?
	//We check only for IPv4 decimal notation, anyway. 
	static const std::regex ipPattern(R"([0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3})",std::regex::extended);
	if(std::regex_match(bucket,ipPattern))
		return(false);
	//"We recommend that you do not use periods (".") in bucket names."
	//This is only a recommendation, so we do not assess it here.
	return(true);
}
		
bool addBucket(std::string rawURL, const std::string bucket){
	if(!validateBucketName(bucket)){
		std::cerr << "Invalid bucket name: " << bucket << std::endl;
		std::cerr << " See https://docs.aws.amazon.com/AmazonS3/latest/dev/BucketRestrictions.html\n";
		return(false);
	}
	
//	std::unique_ptr<xmlDoc,void(*)(xmlDoc*)> tree(xmlNewDoc("1.0"),&xmlFreeDoc);;
//	std::unique_ptr<xmlNode,void(*)(xmlNode*)> root(xmlNewNode(nullptr,"CreateBucketConfiguration"),&xmlFreeNode);
//	xmlDocSetRootElement(tree.get(),root.release());
	s3tools::URL url(rawURL);
	url.path="/"+bucket;
	auto credentials=s3tools::fetchStoredCredentials();
	auto cred=findCredentials(credentials,rawURL).second;
	s3tools::URL signedURL=s3tools::genURL(cred.username,cred.key,"PUT",url.str(),60);
	
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
#ifdef USE_CURLOPT_CAINFO
	std::string caBundlePath=detectCABundlePath();
	if(!caBundlePath.empty()){
		err=curl_easy_setopt(curlSession.get(), CURLOPT_CAINFO, caBundlePath.c_str());
		if(err!=CURLE_OK)
			reportCurlError("Failed to set curl CA bundle path",err,errBuf.get());
	}
#endif
	err=curl_easy_perform(curlSession.get());
	if(err!=CURLE_OK)
		reportCurlError("curl perform PUT failed",err,errBuf.get());
	
	if(!resultData.empty())
		handleXMLRepsonse(resultData,{});
	return(true);
}
		
bool deleteBucket(std::string rawURL, const std::string bucket){
	s3tools::URL url(rawURL);
	url.path="/"+bucket;
	auto credentials=s3tools::fetchStoredCredentials();
	auto cred=findCredentials(credentials,rawURL).second;
	s3tools::URL signedURL=s3tools::genURL(cred.username,cred.key,"DELETE",url.str(),60);
	
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
		reportCurlError("Failed to set curl DELETE/CUSTOMREQUEST option",err,errBuf.get());
	err=curl_easy_setopt(curlSession.get(), CURLOPT_INFILESIZE_LARGE, (curl_off_t)0);
	if(err!=CURLE_OK)
		reportCurlError("Failed to set curl input data size",err,errBuf.get());
	err=curl_easy_setopt(curlSession.get(), CURLOPT_WRITEFUNCTION, collectOutput);
	if(err!=CURLE_OK)
		reportCurlError("Failed to set curl output callback",err,errBuf.get());
	err=curl_easy_setopt(curlSession.get(), CURLOPT_WRITEDATA, &resultData);
	if(err!=CURLE_OK)
		reportCurlError("Failed to set curl output callback data",err,errBuf.get());
#ifdef USE_CURLOPT_CAINFO
	std::string caBundlePath=detectCABundlePath();
	if(!caBundlePath.empty()){
		err=curl_easy_setopt(curlSession.get(), CURLOPT_CAINFO, caBundlePath.c_str());
		if(err!=CURLE_OK)
			reportCurlError("Failed to set curl CA bundle path",err,errBuf.get());
	}
#endif
	err=curl_easy_perform(curlSession.get());
	if(err!=CURLE_OK)
		reportCurlError("curl perform DELETE failed",err,errBuf.get());
	
	if(!resultData.empty())
		handleXMLRepsonse(resultData,{},
				 {{"BucketNotEmpty",[&](std::string){std::cerr << "Error: Bucket " << bucket << " cannot be deleted because it is not empty.\n";}},
				  {"NoSuchBucket",[&](std::string){std::cerr << "Error: Bucket " << bucket << " does not exist.\n";}}});
	return(true);
}

bool bucketInfo(std::string rawURL, const std::string bucket){
	s3tools::URL url(rawURL);
	url.path="/"+bucket;
	auto credentials=s3tools::fetchStoredCredentials();
	auto cred=findCredentials(credentials,rawURL).second;
	
#ifdef USE_CURLOPT_CAINFO
	std::string caBundlePath=detectCABundlePath();
#endif
	
	std::unique_ptr<CURL,void (*)(CURL*)> curlSession(curl_easy_init(),curl_easy_cleanup);
	CURLcode err;
	std::unique_ptr<char[]> errBuf(new char[CURL_ERROR_SIZE]);
	errBuf[0]=0;
	
	auto querySubresource=[&](s3tools::URL url, std::string subresource)->std::string{
		url.query[subresource]="";
		s3tools::URL signedURL=s3tools::genURL(cred.username,cred.key,"GET",url.str(),60);
		std::string resultData;
		err=curl_easy_setopt(curlSession.get(), CURLOPT_ERRORBUFFER, errBuf.get());
		if(err!=CURLE_OK)
			std::cerr << "Failed to set curl error buffer" << std::endl;
		err=curl_easy_setopt(curlSession.get(), CURLOPT_URL, signedURL.str().c_str());
		if(err!=CURLE_OK)
			reportCurlError("Failed to set curl URL option",err,errBuf.get());
		err=curl_easy_setopt(curlSession.get(), CURLOPT_INFILESIZE_LARGE, (curl_off_t)0);
		if(err!=CURLE_OK)
			reportCurlError("Failed to set curl input data size",err,errBuf.get());
		err=curl_easy_setopt(curlSession.get(), CURLOPT_WRITEFUNCTION, collectOutput);
		if(err!=CURLE_OK)
			reportCurlError("Failed to set curl output callback",err,errBuf.get());
		err=curl_easy_setopt(curlSession.get(), CURLOPT_WRITEDATA, &resultData);
		if(err!=CURLE_OK)
			reportCurlError("Failed to set curl output callback data",err,errBuf.get());
#ifdef USE_CURLOPT_CAINFO
		if(!caBundlePath.empty()){
			err=curl_easy_setopt(curlSession.get(), CURLOPT_CAINFO, caBundlePath.c_str());
			if(err!=CURLE_OK)
				reportCurlError("Failed to set curl CA bundle path",err,errBuf.get());
		}
#endif
		err=curl_easy_perform(curlSession.get());
		if(err!=CURLE_OK)
			reportCurlError("curl perform GET failed",err,errBuf.get());
		
		return(resultData);
	};
	
	std::string location;
	//TODO: "When the bucket's region is US East (N. Virginia), Amazon S3 returns 
	//an empty string for the bucket's region"
	handleXMLRepsonse(querySubresource(url,"location"),
	  {{"LocationConstraint",[&](xmlNode* node){location=getNodeContents<std::string>(node);}}});
	std::string versioning;
	handleXMLRepsonse(querySubresource(url,"versioning"),
	  {{"VersioningConfiguration",[&](xmlNode* node){
		versioning=getNodeContents<std::string>(node);
		if(versioning.empty())
			versioning="Not enabled";
	  }}},
	  {{"NotImplemented",[&](std::string){versioning="Not supported";}}});
	
	std::cout << "Bucket: " << bucket << "\n\t";
	std::cout << "Location: " << location << "\n\t";
	std::cout << "Versioning: " << versioning << "\n";
	
	return(true);
}

int main(int argc, char* argv[]){
	std::string usage=R"(NAME
 s3bucket - list and manipulate S3 buckets
	
USAGE
 s3bucket list|add|delete|help [arguments]

SUBCOMMANDS
 list URL
    List all buckets at URL.
 add URL bucket
    Create bucket at URL.
 delete URL bucket
    Delete bucket from URL.
 info URL bucket
    List information about bucket at URL.
    Currently only the location and versioning status are shown.)";
	OptionParser op(true);
	op.setBaseUsage(usage);
	auto arguments=op.parseArgs(argc,argv);
	
	if(op.didPrintUsage())
		return(0);
	if(arguments.size()==1){
		std::cout << op.getUsage() << std::endl;
		return(0);
	}
	std::string subcommand=arguments[1];
	if(subcommand=="list"){
		if(arguments.size()!=3){
			std::cout << "Usage: s3bucket list URL" << std::endl;
			return(1);
		}
		optionsType options;
		options.verbose=false;
		options.readableSizes=false;
		std::string url=arguments[2];
		try{
			return(listBuckets(url,options) ? 0 : 1);
		}catch(std::exception& ex){
			std::cerr << "Error: " << ex.what() << std::endl;
			return(1);
		}
	}
	if(subcommand=="add"){
		if(arguments.size()!=4){
			std::cout << "Usage: s3bucket add URL bucket" << std::endl;
			return(1);
		}
		std::string url=arguments[2];
		std::string bucket=arguments[3];
		try{
			return(addBucket(url,bucket) ? 0 : 1);
		}catch(std::exception& ex){
			std::cerr << "Error: " << ex.what() << std::endl;
			return(1);
		}
	}
	if(subcommand=="delete"){
		if(arguments.size()!=4){
			std::cout << "Usage: s3bucket delete URL bucket" << std::endl;
			return(1);
		}
		std::string url=arguments[2];
		std::string bucket=arguments[3];
		try{
			return(deleteBucket(url,bucket) ? 0 : 1);
		}catch(std::exception& ex){
			std::cerr << "Error: " << ex.what() << std::endl;
			return(1);
		}
	}
	if(subcommand=="info"){
		if(arguments.size()!=4){
			std::cout << "Usage: s3bucket info URL bucket" << std::endl;
			return(1);
		}
		std::string url=arguments[2];
		std::string bucket=arguments[3];
		try{
			return(bucketInfo(url,bucket) ? 0 : 1);
		}catch(std::exception& ex){
			std::cerr << "Error: " << ex.what() << std::endl;
			return(1);
		}
	}
	if(subcommand=="help"){
		std::cout << op.getUsage() << std::endl;
		return(0);
	}
	std::cerr << "Unrecognized subcommand" << std::endl;
	return(1);
}
