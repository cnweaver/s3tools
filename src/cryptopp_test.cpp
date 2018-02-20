//A dummy program using Crypto++ used to test whether compiler setting for using 
//that library are correct. 

#include <iostream>
#include <cryptopp/hex.h>
#include <cryptopp/sha.h>
#include <cryptopp/hmac.h>

int main(){
	using namespace CryptoPP;
	const std::string key="kjsnvkjsbvjksdvjksvjsdvjksdnv";
	const std::string message="sjdnvsjvjskdvjksvjksjvs dvjs";
	std::vector<byte> raw_key(key.begin(),key.end());
	std::string digest;
	HMAC<SHA256> hmac(raw_key.data(), raw_key.size());
	StringSource s(message, true, new HashFilter(hmac, new HexEncoder(new StringSink(digest))));
	std::cout << digest << std::endl;
	return(0);
}
