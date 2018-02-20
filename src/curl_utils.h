#ifndef S3TOOLS_CURL_UTILS_H
#define S3TOOLS_CURL_UTILS_H

#include <string>

#include <curl/curl.h>

size_t collectOutput(void* buffer, size_t size, size_t nmemb, void* userp);

void reportCurlError(std::string expl, CURLcode err, const char* errBuf);

#endif //S3TOOLS_CURL_UTILS_H
