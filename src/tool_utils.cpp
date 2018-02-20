#include "tool_utils.h"

#include <iostream>

s3tools::credential findCredentials(const std::unordered_map<std::string,s3tools::credential>& credentials,
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
