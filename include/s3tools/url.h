#ifndef S3TOOLS_URL_H
#define S3TOOLS_URL_H

#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>

namespace s3tools{

///Replace all uppercase ASCII characters with their lowercase equivalents
std::string lowercase(std::string s);
std::string urlencode(const std::string& in, bool allowSlash=false);

struct URL{
	std::string verb; //should ideally be an enum, don't care right now
	std::string scheme;
	std::string username;
	std::string password;
	std::string host;
	unsigned int port;
	std::string path;
	struct queryEntry : public std::string{
		bool urlEncoded;
		queryEntry():urlEncoded(false){}
		queryEntry(const char* s, bool e=false):std::string(s),urlEncoded(e){}
		queryEntry(std::string s, bool e=false):std::string(s),urlEncoded(e){}
	};
	std::map<queryEntry,queryEntry> query;
	std::string fragment;
	//headers are not really part of a URL, but some S3 operations cannot work 
	//without them, and require them to be part of the canonicalization process
	std::map<std::string,std::string> headers;
	
	URL():verb("GET"),scheme("http"),port(80){}
	
	//TODO: in general default port should depend on the scheme
	URL(const std::string raw):verb("GET"),port(80){
		std::regex url_regex(R"(^([^:]+)://(([^:]+)(:[^@]+)?@)?(([^:/]+)(:[0-9]+)?)?(/[^?#]*)?(\?[^#]*)?(#.*)?)",std::regex::extended);
		std::smatch matches;
		if(!std::regex_match(raw,matches,url_regex))
			throw std::runtime_error("String '"+raw+"' not recognized as a valid URL (no match)");
		if(matches.size()!=11)
			throw std::runtime_error("String '"+raw+"' not recognized as a valid URL");
		scheme=matches[1];
		if(scheme.empty())
			throw std::runtime_error("Did not find a valid scheme in '"+raw+"'");
		username=matches[3].str();
		if(!matches[4].str().empty()){
			password=matches[4].str().substr(1);
		}
		host=matches[6];
		if(scheme!="file" && host.empty())
			throw std::runtime_error("Did not find a valid host in '"+raw+"' scheme="+scheme);
		if(!matches[7].str().empty()){
			port=std::stoul(matches[7].str().substr(1));
			if(port==0)
				throw std::runtime_error("Invalid port number '"+matches[7].str().substr(1)+"'");
		}
		path=matches[8];
		if(path.empty())
			path="/";
			//throw std::runtime_error("Did not find a valid path in '"+raw+"'");
		if(!matches[9].str().empty())
			query=parseQuery(matches[9].str());
		if(!matches[10].str().empty())
			fragment=matches[10].str().substr(1);
	}
	
	///\pre query shall be a string beginning with '?'
	//static std::map<std::string,std::string> parseQuery(const std::string& query){
	//	std::map<std::string,std::string> results;
	static std::map<queryEntry,queryEntry> parseQuery(const std::string& query){
		std::map<queryEntry,queryEntry> results;
		std::size_t pos=1; //skip the initial '?'
		while(pos<query.size()){
			std::size_t nextPos=query.find('&',pos);
			std::size_t valuePos=query.find('=',pos);
			if(valuePos<nextPos){ //could be a valid key=value pair
				if(valuePos==pos+1) //is there a non-empty key?
					throw std::runtime_error("Invalid query string: value without key: '"+query+"'");
				std::string key=query.substr(pos,valuePos-pos); //valuePos is never npos
				std::string value;
				if(valuePos+1<query.size())
					value=query.substr(valuePos+1,nextPos-valuePos-1);
				//results[key]=value;
				results[queryEntry(key,true)]=queryEntry(value,true);
			}
			else{ //no value? For now, get angry
				throw std::runtime_error("Invalid query string: key without value: '"+query+"'");
			}
			if(nextPos==std::string::npos)
				break;
			pos=nextPos+1;
		}
		return(results);
	}
	
	std::string str() const{
		std::ostringstream result;
		result << scheme << "://";
		if(!username.empty()){
			result << username;
			if(!password.empty())
				result << ':' << password;
			result << '@';
		}
		result << host;
		if(port!=80)
			result << ':' << port;
		result << urlencode(path,true);
		if(!query.empty()){
			bool first=true;
			for(auto p : query){
				if(first){
					result << '?';
					first=false;
				}
				else
					result << '&';
				if(p.first.urlEncoded)
					result << p.first;
				else
					result << urlencode(p.first);
				result << '=';
				if(p.second.urlEncoded)
					result << p.second;
				else
					result << urlencode(p.second);
			}
		}
		if(!fragment.empty())
			result << '#' << fragment;
		return(result.str());
	}
	
	std::string canonicalRequest() const{
		std::ostringstream result;
		result << verb << '\n';
		//Does not attempt to canonicalize the path. This doesn't appear to matter?
		result << urlencode(path,true) << '\n';
		//TODO: query items should be sorted after encoding; current code may yield wrong order
		//if some parameters are unencoded
		{//query string
			bool first=true;
			for(auto p : query){
				if(first)
					first=false;
				else
					result << '&';
				//result << urlencode(p.first) << '=' << urlencode(p.second);
				if(p.first.urlEncoded)
					result << p.first;
				else
					result << urlencode(p.first);
				result << '=';
				if(p.second.urlEncoded)
					result << p.second;
				else
					result << urlencode(p.second);
			}
			result << '\n';
		}
		std::map<std::string,std::string> allHeaders;
		for(const auto& header : headers)
			allHeaders.insert(std::make_pair(lowercase(header.first),header.second));
		allHeaders["host"]=((port==80) ? host : (host+":"+std::to_string(port)));
		/*{//'canonical headers'
			result << "host:" << host;
			if(port!=80)
				result << ':' << port;
			result << '\n';
		}*/
		//'canonical headers'
		for(const auto& header : allHeaders)
			result << header.first << ':' << header.second << '\n';
		result << '\n';
		/*//'signed' headers
		result << "host";
		result << '\n';*/
		{//'signed' headers
			int remaining=allHeaders.size();
			for(const auto& header : allHeaders)
				result << lowercase(header.first) << ((--remaining)>0 ? ';' : '\n');
		}
		if(headers.count("x-amz-content-sha256"))
			result << headers.find("x-amz-content-sha256")->second;
		else
			result << "UNSIGNED-PAYLOAD";
		return(result.str());
	}
	
	std::set<std::string> getSignedHeaders() const{
		std::set<std::string> allHeaders;
		allHeaders.insert("host");
		for(const auto& header : headers)
			allHeaders.insert(lowercase(header.first));
		return(allHeaders);
	}
};
	
}

#endif //S3TOOLS_URL_H
