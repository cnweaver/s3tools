#include "xml_utils.h"

static class cleanup_t{
public:
	~cleanup_t(){
		xmlCleanupParser();
	}
} cleanupLibXML;

std::string trim(std::string s,const std::string& whitespace){
	s=s.erase(s.find_last_not_of(whitespace)+1);
	return(s.erase(0,s.find_first_not_of(whitespace)));
}

bool hasAttribute(xmlNode* node, const std::string& name){
	xmlAttr* attr=xmlHasProp(node,(const xmlChar*)name.c_str());
	return(attr!=NULL);
}

xmlNode* firstChild(xmlNode* node, const std::string& name, bool notFoundError){
	if(!node)
		throw xmlParseError("Unable to search children of NULL XML element");
	for(xmlNode* child=node->children; child!=NULL; child=child->next){
		if(child->type!=XML_ELEMENT_NODE)
			continue;
		if(name.empty() || (char*)child->name==name)
			return(child);
	}
	if(notFoundError){
		if(name.empty())
			throw xmlParseError(std::string("Node '")+(char*)node->name+"' has no children");
		throw xmlParseError(std::string("Node '")+(char*)node->name+"' has no child with name '"+name+"'");
	}
	return(NULL);
}

xmlNode* nextSibling(xmlNode* node, const std::string& name){
	if(!node)
		throw xmlParseError("Unable to search siblings of NULL XML element");
	for(node=node->next; node; node=node->next){
		if(node->type!=XML_ELEMENT_NODE)
			continue;
		if(name.empty() || (char*)node->name==name)
			return(node);
	}
	return(NULL);
}

template<>
std::string getAttribute<std::string>(xmlNode* node, const std::string& name){
	std::unique_ptr<xmlChar,void(*)(void*)> attr(xmlGetProp(node,(const xmlChar*)(name.c_str())),xmlFree);
	if(!attr)
		throw xmlParseError("Node has no attribute '"+name+"'");
	return(trim((const char*)attr.get()));
}

template<>
std::string getNodeContents<std::string>(xmlNode* node){
	std::unique_ptr<xmlChar,void(*)(void*)> content(xmlNodeGetContent(node),xmlFree);
	if(!content)
		return("");
	return(trim((const char*)content.get()));
}

void handleXMLRepsonse(const std::string& raw, 
                       const std::map<std::string,std::function<void(xmlNode*)>>& handlers, 
                       const std::map<std::string,std::function<void(std::string)>>& errorHandlers){
	std::unique_ptr<xmlDoc,void(*)(xmlDoc*)> tree(xmlReadDoc((const xmlChar*)raw.c_str(),NULL,NULL,XML_PARSE_RECOVER|XML_PARSE_PEDANTIC),&xmlFreeDoc);
	xmlNode* root = xmlDocGetRootElement(tree.get());
	if(!root)
		throw std::runtime_error("Got invalid XML data: \n"+raw);
	std::string nodeName((char*)root->name);
	if(handlers.count(nodeName)){
		handlers.find(nodeName)->second(root);
		return;
	}
	if(nodeName=="Error"){
		xmlNode* code=firstChild(root,"Code");
		std::string codeS="<None>", messageS="<None>";
		if(code)
			codeS=getNodeContents<std::string>(code);
		xmlNode* message=firstChild(root,"Message");
		if(message)
			messageS=getNodeContents<std::string>(message);
		
		if(errorHandlers.count(codeS)){
			errorHandlers.find(codeS)->second(messageS);
			return;
		}
		else
			throw std::runtime_error("Query returned error:\n Code: "+codeS+"\n Message: "+messageS);
	}
	throw std::runtime_error("Unexpected XML node type: "+nodeName);
}
