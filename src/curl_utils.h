#ifndef S3TOOLS_CURL_UTILS_H
#define S3TOOLS_CURL_UTILS_H

#include <string>

#include <curl/curl.h>

#if ! ( __APPLE__ && __MACH__ )
	//Whether to use CURLOPT_CAINFO to specifiy a CA bundle path.
	//According to https://curl.haxx.se/libcurl/c/CURLOPT_CAINFO.html
	//this should not be used on Mac OS
	#define USE_CURLOPT_CAINFO
#endif

size_t collectOutput(void* buffer, size_t size, size_t nmemb, void* userp);

void reportCurlError(std::string expl, CURLcode err, const char* errBuf);

#ifdef USE_CURLOPT_CAINFO
std::string detectCABundlePath();
#endif

#endif //S3TOOLS_CURL_UTILS_H
