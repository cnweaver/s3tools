#include <s3tools/signing.h>

#include <chrono>
#include <ctime>
#include <iomanip>
#include <map>
#include <regex>
#include <sstream>

#include <cryptopp/hex.h>
#include <cryptopp/sha.h>
#include <cryptopp/hmac.h>

#include <iostream>

namespace s3tools{

std::string get_timestamp(){
	std::chrono::system_clock::time_point cur_time=std::chrono::system_clock::now();
	time_t cur_time_tt=std::chrono::system_clock::to_time_t(cur_time);
	struct tm* cur_time_tm=gmtime(&cur_time_tt);
	std::string cur_time_s(32,'\0');
	size_t len=strftime(&cur_time_s[0], 32, "%Y%m%dT%H%M%SZ",cur_time_tm);
	cur_time_s.resize(len);
	return(cur_time_s);
}

std::string extractDate(const std::string& timestamp){
	std::size_t pos=timestamp.find('T');
	if(pos==std::string::npos)
		throw std::runtime_error("Internal date format error");
	return(timestamp.substr(0,pos));
}

//hex encoded SHA-256 hash
std::string SHA256Hash(const std::string& message){
	using namespace CryptoPP;
	std::string digest;
	SHA256 hash;
	StringSource s(message, true, new HashFilter(hash, new HexEncoder(new StringSink(digest))));
	return(digest);
}

//compute a SHA-256 HMAC
std::vector<CryptoPP::byte> SHA256HMAC(const std::vector<CryptoPP::byte>& key, const std::string& message){
	using namespace CryptoPP;
	std::vector<byte> digest(HMAC<SHA256>::DIGESTSIZE);
	HMAC<SHA256> hmac(key.data(), key.size());
	std::vector<byte> input(message.begin(),message.end());
	hmac.CalculateDigest(digest.data(), input.data(), input.size());
	return(digest);
}

//compute a SHA-256 HMAC with a string key
std::vector<CryptoPP::byte> SHA256HMAC(const std::string& key, const std::string& message){
	std::vector<CryptoPP::byte> raw_key(key.begin(),key.end());
	return(SHA256HMAC(raw_key,message));
}

//compute a SHA-256 HMAC and render the result as a hex encoded string
std::string SHA256HMAC_hex(const std::vector<CryptoPP::byte>& key, const std::string& message){
	using namespace CryptoPP;
	std::string digest;
	HMAC<SHA256> hmac(key.data(), key.size());
	StringSource s(message, true, new HashFilter(hmac, new HexEncoder(new StringSink(digest))));
	return(digest);
}

URL genURL(std::string username, std::string secretkey, std::string verb, URL url, unsigned long exprTime, std::string timestamp){
	url.verb=verb;

	// Procedure from http://docs.aws.amazon.com/AmazonS3/latest/API/sigv4-query-string-auth.html
	if(timestamp.empty())
		timestamp=get_timestamp();
	std::string date=extractDate(timestamp);
	std::string awsregion="us-east-1";
	std::string service="s3";
	std::string scope=date+"/"+awsregion+"/"+service+"/aws4_request";
	
	std::string signedHeaders;
	{
		std::ostringstream sh;
		auto headers=url.getSignedHeaders();
		int remaining=headers.size();
		for(auto& header : headers)
			sh << lowercase(header) << ((--remaining)>0 ? ";" : "");
		signedHeaders=sh.str();
	}
	
	url.query["X-Amz-Algorithm"]="AWS4-HMAC-SHA256";
	url.query["X-Amz-Credential"]=username+"/"+scope;
	url.query["X-Amz-Date"]=timestamp;
	url.query["X-Amz-Expires"]=std::to_string(exprTime);
	url.query["X-Amz-SignedHeaders"]=signedHeaders;//"host";
	
	//std::cout << "Canonical request: \n" << url.canonicalRequest() << std::endl;
	std::string stringToSign="AWS4-HMAC-SHA256\n"+timestamp+"\n"+scope+"\n"+lowercase(SHA256Hash(url.canonicalRequest()));
	
	auto dateKey=SHA256HMAC("AWS4"+secretkey,date);
	auto dateRegionKey=SHA256HMAC(dateKey,awsregion);
	auto dateRegionServiceKey=SHA256HMAC(dateRegionKey,service);
	auto signingKey=SHA256HMAC(dateRegionServiceKey,"aws4_request");
	
	std::string signature=lowercase(SHA256HMAC_hex(signingKey,stringToSign));
	url.query["X-Amz-Signature"]=signature;
	
	return(url);
}
	
URL genURLNoQuery(std::string username, std::string secretkey, std::string verb, URL url, unsigned long exprTime, std::string timestamp){
	url.verb=verb;
	
	// Procedure from https://docs.aws.amazon.com/AmazonS3/latest/API/sig-v4-header-based-auth.html
	if(timestamp.empty())
		timestamp=get_timestamp();
	std::string date=extractDate(timestamp);
	std::string awsregion="us-east-1";
	std::string service="s3";
	std::string scope=date+"/"+awsregion+"/"+service+"/aws4_request";
	
	url.headers["x-amz-date"]=timestamp;
	
	std::string signedHeaders;
	{
		std::ostringstream sh;
		auto headers=url.getSignedHeaders();
		int remaining=headers.size();
		for(auto& header : headers)
			sh << lowercase(header) << ((--remaining)>0 ? ";" : "");
		signedHeaders=sh.str();
	}
	
	//std::cout << "Canonical request: \n" << url.canonicalRequest() << std::endl;
	std::string stringToSign="AWS4-HMAC-SHA256\n"+timestamp+"\n"+scope+"\n"+lowercase(SHA256Hash(url.canonicalRequest()));
	
	auto dateKey=SHA256HMAC("AWS4"+secretkey,date);
	auto dateRegionKey=SHA256HMAC(dateKey,awsregion);
	auto dateRegionServiceKey=SHA256HMAC(dateRegionKey,service);
	auto signingKey=SHA256HMAC(dateRegionServiceKey,"aws4_request");
	
	std::string signature=lowercase(SHA256HMAC_hex(signingKey,stringToSign));
	
	{
		std::ostringstream auth;
		auth << "AWS4-HMAC-SHA256 Credential=" << username << '/' << scope << ',';
		auth << "SignedHeaders=" << signedHeaders << ',';
		auth << "Signature=" << signature;
		
		url.headers["Authorization"]=auth.str();
	}
	
	return(url);
}
	
} //namespace s3tools
