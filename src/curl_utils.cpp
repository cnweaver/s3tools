#include "curl_utils.h"

#include <iostream>
#include <stdexcept>

size_t collectOutput(void* buffer, size_t size, size_t nmemb, void* userp){
	std::string* out=static_cast<std::string*>(userp);
	//curl can't tolerate exceptions, so stop them and log them to stderr here
	try{
		out->append((char*)buffer,size*nmemb);
	}catch(std::exception& ex){
		std::cerr << "Exception thrown while collecting output: " << ex.what() << std::endl;
		return(size*nmemb?0:1); //return a different number to indicate error
	}catch(...){
		std::cerr << "Exception thrown while collecting output" << std::endl;
		return(size*nmemb?0:1); //return a different number to indicate error
	}
	return(size*nmemb);//return full size to indicate success
}

void reportCurlError(std::string expl, CURLcode err, const char* errBuf){
	if(errBuf[0]!=0)
		throw std::runtime_error(expl+"\n curl error: "+errBuf);
	else
		throw std::runtime_error(expl+"\n curl error: "+curl_easy_strerror(err));
}
