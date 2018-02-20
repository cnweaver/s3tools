#include <s3tools/url.h>

#include <iomanip>

namespace s3tools{

std::string lowercase(std::string s){
	for(char& c : s)
		if(c>='A' && c<='Z')
			c+=('a'-'A');
	return(s);
}

std::string urlencode(const std::string& in, bool allowSlash){
	std::ostringstream out;
	out << std::uppercase << std::hex << std::setfill('0');
	for(char c : in){
		if(c=='/' && !allowSlash)
			out << '%' << std::setw(2) << (int)c;
		else if(c<'*' || c=='+' || c==',' || (c>=':' && c<='@') || (c>='[' && c<='^') || c=='`' || c>'z')
			out << '%' << std::setw(2) << (int)c;
		else
			out << c;
	}
	return(out.str());
}
	
}
