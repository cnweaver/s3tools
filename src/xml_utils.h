#ifndef S3TOOLS_XML_UTILS_H
#define S3TOOLS_XML_UTILS_H

#include <map>
#include <memory>
#include <string>
#include <sstream>
#include <typeinfo>

#include <libxml/parser.h>
#include <libxml/tree.h>

//a knockoff boost::lexical_cast
class bad_lexical_cast:public std::bad_cast{
public:
	bad_lexical_cast(std::string m):m(m){}
	virtual ~bad_lexical_cast() throw(){}
	virtual const char* what() const throw(){ return(m.c_str()); }
private:
	std::string m;
};

template<typename Target, typename Source>
Target lexical_cast(const Source& s){
	std::stringstream ss;
	ss << s;
	Target t;
	ss >> t;
	if(ss.fail())
		throw bad_lexical_cast(std::string("Unable to convert ")
							   +typeid(Source).name()+" to "
							   +typeid(Target).name()+"; intermediate form: "
							   +ss.str());
	return(t);
}

struct xmlParseError : public std::runtime_error{
public:
	xmlParseError(const std::string& m):std::runtime_error(m){}
};

std::string trim(std::string s,const std::string& whitespace=" \t\n\r");

bool hasAttribute(xmlNode* node, const std::string& name);

template <typename T>
T getAttribute(xmlNode* node, const std::string& name){
	std::unique_ptr<xmlChar,void(*)(void*)> attr(xmlGetProp(node,(const xmlChar*)(name.c_str())),xmlFree);
	if(!attr)
		throw xmlParseError("Node has no attribute '"+name+"'");
	try{
		return(lexical_cast<T>(trim((const char*)attr.get())));
	}catch(bad_lexical_cast& blc){
		throw xmlParseError("Failed to parse attribute '"+name+"' as type "+typeid(T).name()+" (value was "+(char*)attr.get()+")");
	}
}

//specialization for strings
template<>
std::string getAttribute<std::string>(xmlNode* node, const std::string& name);

template <typename T>
T getNodeContents(xmlNode* node){
	std::unique_ptr<xmlChar,void(*)(void*)> content(xmlNodeGetContent(node),xmlFree);
	try{
		if(!content)
			throw bad_lexical_cast("");
		return(lexical_cast<T>(trim((const char*)content.get())));
	}catch(bad_lexical_cast& blc){
		throw xmlParseError(std::string("Failed to parse node contents as type ")+typeid(T).name()+" (value was "+(char*)content.get()+")");
	}
}

//specialization for strings
template<>
std::string getNodeContents<std::string>(xmlNode* node);

xmlNode* firstChild(xmlNode* node, const std::string& name="", bool notFoundError=false);
xmlNode* nextSibling(xmlNode* node, const std::string& name="");

///\param raw the input data to be prsed as XML
///\param handlers a collection of callback functions to be invoked on particular node types.
///                If the root node does not match any type listed in handlers, an exception will be thrown.
///                If a match is found, the callback will be invoked with the node pointer.
///\param errorHandlers a collection of callback functions to be invoked on error types.
///                     Errors are represented by the 'Error' node type (which may also 
///                     be explicitly handled by an entry in handlers), and distinguished
///                     by their 'Code' child node (if they have one). If a match for the
///                     obtained code is found, the correpsonding callback will be invoked
///                     with the error's message, which may be empty. If the error has no
///                     code or the code is not listed in errorHandlers, an exception will
///                     be thrown.
void handleXMLRepsonse(const std::string& raw, 
                       const std::map<std::string,std::function<void(xmlNode*)>>& handlers, 
                       const std::map<std::string,std::function<void(std::string)>>& errorHandlers={});

#endif //S3TOOLS_XML_UTILS_H