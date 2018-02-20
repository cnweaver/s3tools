#include <iostream>
#include "genS3URL.h"

int main(int argc, char* argv[]){
	if(argc!=5 && argc!=6){
		std::cerr << "Usage: genS3URL username secret_key verb target_url [expiration_time]\n";
		std::cerr << "    verb should be 'GET' or 'PUT'\n";
		std::cerr << "    target_url is the URL to be accessed, for which a signed query string will be generated\n";
		std::cerr << "    expiration_time is the length of validity of the URL, in seconds; if omitted the default is one week\n";
		return(1);
	}
	std::string username=argv[1];
	std::string secretkey=argv[2];
	std::string verb=argv[3];
	std::string baseURL=argv[4];
	unsigned long exprTime=604800; //one week
	if(argc==6){
		std::string exprTime_s=argv[5];
		try{
			exprTime=std::stoul(exprTime_s);
		}catch(...){
			std::cerr << "Invalid expiration time/validity duration: '" << exprTime_s << "'" << std::endl;
			return(1);
		}
		if(exprTime==0 || exprTime>31536000/*one year*/){
			std::cerr << "Invalid expiration time/validity duration: '" << exprTime_s << "'" << std::endl;
			return(1);
		}
	}

	URL url=genURL(username,secretkey,verb,baseURL,exprTime);
	std::cout << url.str() << std::endl;
	return(0);
}