// Copyright (c) 2017, Christopher Weaver
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
// 
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
// 
//  2. Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
// 
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
//  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
//  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
//  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
//  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
//  NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
//  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
//  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
//  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
//  POSSIBILITY OF SUCH DAMAGE.

#ifndef CL_OPTIONS_H
#define CL_OPTIONS_H

#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

///A simple utility for parsing command line options. 
///
/// Assuming an object of this class:
/// \code
/// OptionParser op;
/// \endcode
/// Options may be flags (like `ls -l`) which are handled via a callback function:
/// \code
/// bool longFormat=false;
/// op.addOption('l', [&]{longFormat=true;}, "List in long format.");
/// \endcode
/// or may take values, which can be directly stored in a variable 
/// (like `tar -f archive.tar`):
/// \code
/// std::string archiveFile;
/// op.addOption('f', archiveFile, 
///      "Read the archive from or write the archive to the specified file.");
/// \endcode
/// Options taking values may also do so via a callback, although in this case
/// it is unfortunately necessary to explicit specify the datatype expected by
/// the callback:
/// \code
/// std::string archiveFile;
/// op.addOption<std::string>('f',
///      [&archiveFile](std::string file){ archiveFile=file; },
///      "Read the archive from or write the archive to the specified file.");
/// \endcode
/// Note that options which take value may do so either by consuming the 
/// following argument, or may have the value specifier 'inline' with an equals 
/// sign so that both of the following invocations of a program with the above 
/// option are equivalent:
/// \code
/// tar -f archive.tar
/// tar -f=archive.tar
/// \endcode
///
/// Options may be either short ('-l') or long ('--long'). A single option may
/// have multiple synonymous forms:
/// \code
/// bool longFormat=false;
/// op.addOption({"l","long","long-listing"}, 
///      [&]{longFormat=true;}, "List in long format.");
/// \endcode
///
/// Positional arguments are also supported and are separated out from options
/// and their values. A more complete 'tar' example:
/// \code
/// OptionParser op;
/// bool create=false, extract=false;
/// op.addOption('c', [&]{create=true}, 
///      "Create a new archive containing the specified items.");
/// op.addOption('x', [&]{extract=true}, 
///      "Extract to disk from the archive.");
/// std::string archiveFile;
/// op.addOption('f', archiveFile, 
///      "Read the archive from or write the archive to the specified file.");
/// auto positionals = op.parseArgs(argc, argv);
/// \endcode
/// With this code in place one could invoke the hypothetical 'tar' program as
/// \code
/// tar -c -f my_files.tar file1 file2
/// \endcode
/// and `create` would be set to `true`, while `positionals` would be an
/// `std::vector<std::string>` containing `{"tar","file1","file2"}`. 
///
/// This class also supports generating simple usage information, and by default
/// the options '-h', '-?', '--help', and '--usage' will write it to standard 
/// output. (These default options may be suppressed when the OptionParser is 
/// constructed.) Each option is summarized using the description string provided 
/// when it is added, and custom introductory text can be added before this. 
/// Adding to the above example:
/// \code
/// op.setBaseUsage("This is toy example of the tar interface");
/// \endcode
/// Running this program with any of the built-in help options produces:
/// \verbatim
/// This is toy example of the tar interface
///  -c: Create a new archive containing the specified items.
///  -x: Extract to disk from the archive.
///  -f: Read the archive from or write the archive to the specified file.
/// \endverbatim
/// The generation of this help message is currently very simple, and makes no 
/// effort to reflow text, so users are advised to format description strings 
/// themselves. 
class OptionParser{
private:
	///short options which take a value
	std::map<char,std::function<void(std::string)>> shortOptions;
	///short options which do not take a value
	std::map<char,std::function<void()>> shortOptionsNoStore;
	///long options which take a value
	std::map<std::string,std::function<void(std::string)>> longOptions;
	///long options which do not take a value
	std::map<std::string,std::function<void()>> longOptionsNoStore;
	///whether the help message was automatically printed
	bool printedUsage;
	///the help text
	std::string usageMessage;
	///Whether a short option taking a value may be directly followed by its 
	///value without a separating equals sign
	bool allowShortValueWithoutEquals;
	///Whether to use ANSI escape codes when generating text to print
	bool useANSICodes;
	///Whether multiple short options may be run together
	bool allowShortOptionCombination;
	///Whether the special option '--' ends option parsing
	bool allowOptionTerminator;
	
	///check whether an identifier is a valid option name
	void checkIdentifier(std::string ident){
		if(ident.empty())
			throw std::logic_error("Invalid option name: '': options may not be empty");
		if(ident.find('=')!=std::string::npos)
			throw std::logic_error("Invalid option name: '"+ident+"': options may not contain '='");
		if(ident.find('-')==0)
			throw std::logic_error("Invalid option name: '"+ident+"': options may not begin with '-'");
		//valid identifier, do nothing
	}
	
	///check whether a short option already exists
	bool optionKnown(char ident){
		return(shortOptions.count(ident) || shortOptionsNoStore.count(ident));
	}
	///check whether a long option already exists
	bool optionKnown(std::string ident){
		return(longOptions.count(ident) || longOptionsNoStore.count(ident));
	}
	
	///ensure that a value is a string
	static std::string asString(std::string s){ return s; }
	///ensure that a value is a string
	static std::string asString(char c){ return std::string(1,c); }
	
	///Add an option which stores a value to a variable
	template<typename IDType, typename DestType>
	void addOption(IDType ident, DestType& destination, std::map<IDType,std::function<void(std::string)>>& storeMap){
		if(optionKnown(ident))
			throw std::logic_error("Attempt to redefine option '"+asString(ident)+"'");
		storeMap.emplace(ident,
		                 [ident,&destination](std::string optData)->void{
		                 	std::istringstream ss(optData);
		                 	ss >> destination;
		                 	if(ss.fail()){
		                 		throw std::runtime_error("Failed to parse \""+optData+"\" as argument to '"
		                 		                         +asString(ident)+"' option");
		                 	}
		                 });
	}
	///Add an option with a callback which takes nothing
	template<typename IDType>
	void addOption(IDType ident, std::function<void()> action, std::map<IDType,std::function<void()>>& storeMap){
		if(optionKnown(ident))
			throw std::logic_error("Attempt to redefine option '"+asString(ident)+"'");
		storeMap.emplace(ident,action);
	}
	///Add an option with a callback which takes a value
	template<typename DestType, typename IDType>
	void addOption(IDType ident, std::function<void(DestType)> action, std::map<IDType,std::function<void(std::string)>>& storeMap){
		if(optionKnown(ident))
			throw std::logic_error("Attempt to redefine option '"+asString(ident)+"'");
		storeMap.emplace(ident,std::function<void(std::string)>(
						 [ident,action](std::string optData)->void{
							 std::istringstream ss(optData);
							 DestType destination;
							 ss >> destination;
							 if(ss.fail()){
								 throw std::runtime_error("Failed to parse \""+optData+"\" as argument to '"
														  +asString(ident)+"' option");
							 }
							 action(destination);
						 }));
	}
	
	struct ArgumentState{
		const enum ArgumentStateType{
			///a self-contained option
			Option, 
			///an option which requires an associated value
			OptionNeedsValue, 
			///not an option, a positional argument
			NonOption, 
			///the special option  which ends option parsing
			OptionTerminator
		} type;
		const std::string option;
		ArgumentState(ArgumentStateType t):type(t){
			if(type==OptionNeedsValue)
				throw std::logic_error("OptionNeedsValue state must have an option name");
		}
		ArgumentState(ArgumentStateType t, std::string opt):type(t),option(opt){}
	};
	
	///Process one argument as a short option
	///\param arg the argument
	///\param startIdx the character index within arg where the option should begin
	ArgumentState handleShortOption(const std::string& arg, const size_t startIdx){
		static const auto& npos=std::string::npos;
		size_t endIdx, valueOffset=0;
		if(allowShortValueWithoutEquals || allowShortOptionCombination){
			endIdx=startIdx+1;
			if(endIdx==arg.size())
				endIdx=npos;
			else if(arg[endIdx]=='=')
				valueOffset=1;
		}
		else{
			endIdx=arg.find('=',startIdx);
			valueOffset=1;
		}
		std::string opt=arg.substr(startIdx,(endIdx==npos?npos:endIdx-startIdx));
		
		if(opt.empty())
			throw std::runtime_error("Invalid option: '"+arg+"'");
		
		if(opt.size()>1)
			throw std::runtime_error("Malformed option: '"+arg+"' (wrong number of leading dashes)");
		
		char optC=opt[0];
		if(shortOptionsNoStore.count(optC)){
			if(endIdx!=npos && !allowShortOptionCombination)
				throw std::runtime_error("Malformed option: '"+arg+"' (no value expected for this flag)");
			shortOptionsNoStore.find(optC)->second();
			//if stuff remains in the argument, recurse to process it
			if(allowShortOptionCombination && endIdx!=npos)
				return(handleShortOption(arg,startIdx+1)); 
		}
		else if(shortOptions.count(optC)){
			if(endIdx==npos)
				return(ArgumentState{ArgumentState::OptionNeedsValue,opt});
			std::string value=arg.substr(endIdx+valueOffset);
			shortOptions.find(optC)->second(value);
		}
		else
			throw std::runtime_error("Unknown option: '"+opt+"' in '"+arg+"'");
		
		return(ArgumentState::Option);
	}
	
	///Process one argument as a long option
	///\param arg the argument
	///\param startIdx the character index within arg where the option should begin
	ArgumentState handleLongOption(const std::string& arg, const size_t startIdx){
		static const auto& npos=std::string::npos;
		size_t endIdx=arg.find('=',startIdx);
		std::string opt=arg.substr(startIdx,(endIdx==npos?npos:endIdx-startIdx));
		
		if(opt.empty())
			throw std::runtime_error("Invalid option: '"+arg+"'");
		
		if(opt.size()==1)
			throw std::runtime_error("Malformed option: '"+arg+"' (wrong number of leading dashes)");
		
		std::string value;
		if(endIdx!=npos && endIdx!=arg.size()-1)
			value=arg.substr(endIdx+1);
		
		if(longOptions.count(opt)){
			if(endIdx==npos)
				return(ArgumentState{ArgumentState::OptionNeedsValue,opt});
			longOptions.find(opt)->second(value);
		}
		else if(longOptionsNoStore.count(opt)){
			if(endIdx!=npos)
				throw std::runtime_error("Malformed option: '"+arg+"' (no value expected for this flag)");
			longOptionsNoStore.find(opt)->second();
		}
		else
			throw std::runtime_error("Unknown option: '"+arg+"'");
		
		return(ArgumentState::Option);
	}
	
	///Process one argument in isolation
	///\return the type of the argument and whether it was consumed
	ArgumentState handleNextArg(const std::string& arg){
		if(arg.size()<2) //not an option, skip it
			return(ArgumentState::NonOption);
		if(arg[0]!='-') //not an option, skip it
			return(ArgumentState::NonOption);
		if(allowOptionTerminator && arg=="--")
			return(ArgumentState::OptionTerminator);
		size_t startIdx=arg.find_first_not_of('-');
		if(startIdx>2) //not an option, skip it
			return(ArgumentState::NonOption);
		if(startIdx==1) //dealing with a short option
			return(handleShortOption(arg,startIdx));
		else //dealing with a long option
			return(handleLongOption(arg,startIdx));
	}
	
	///Process an argument which takes a value
	///\param option the name of the value consuming option previously encountered
	///\param value the next argument, taken to be the value
	///\pre the argument has aready been classified and sanity checked by handleNextArg
	void handleOptWithValue(const std::string& opt, const std::string& value){
		if(opt.size()==1){
			char optC=opt[0];
			if(shortOptions.count(optC)){
				shortOptions.find(optC)->second(value);
			}
			else
				throw std::runtime_error("Internal logic error handling option: '"+opt+"'");
		}
		else{
			if(longOptions.count(opt)){
				longOptions.find(opt)->second(value);
			}
			else
				throw std::runtime_error("Internal logic error handling option: '"+opt+"'");
		}
	}
	
	///Construct a string describing all of the synonyms for an option
	static std::string synonymList(const std::initializer_list<std::string>& list){
		std::ostringstream ss;
		for(auto begin=list.begin(), it=begin, end=list.end(); it!=end; it++){
			if(it!=begin)
				ss << ", ";
			if(it->size()==1)
				ss << '-';
			else
				ss << "--";
			ss << *it;
		}
		return(ss.str());
	}
	
	///Add whitespace to indent text following newlines
	static std::string indentDescription(std::string description){
		size_t pos=0;
		while((pos=description.find('\n',pos))!=std::string::npos)
			description.replace(pos++,1,"\n    ");
		return(description);
	}
	
	std::string underline(std::string s) const{
		if(useANSICodes)
			return("\x1B[4m"+s+"\x1B[24m");
		return(s);
	}
	
public:
	///Construct an OptionParser
	///\param automaticHelp automatically add '-h', '-?', "--help" and "--usage"
	///                     as options which trigger printing the autogenerated
	///                     help message
	explicit OptionParser(bool automaticHelp=true):printedUsage(false),
	allowShortValueWithoutEquals(false),useANSICodes(true),
	allowShortOptionCombination(false),allowOptionTerminator(false){
		if(automaticHelp)
			addOption({"h","?","help","usage"},
					  [this](){
						  std::cout << getUsage() << std::endl;
						  printedUsage=true;
					  },
					  "Print usage information.");
	}
	
	///Set the base usage message, printed before the per-option usage information
	///\param usageMessage_ the message to be shown to the user
	///\note This function overwrites the entire internal message buffer, so it
	///\should be called before any calls to `addOption`. 
	void setBaseUsage(std::string usageMessage_){
		usageMessage=usageMessage_+'\n';
	}
	
	///Get the usage message
	///\return the usage message including both any message set by `setBaseUsage`
	///        and any information about individual options appended by `addOption`.
	std::string getUsage(){
		return(usageMessage);
	}
	
	///Whether the help message was automatically printed
	bool didPrintUsage() const{
		return(printedUsage);
	}
	
	///Whether a short option taking a value may be directly followed by its 
	///value without a separating equals sign, e.g. -lfoo as a linking option
	///to specify linking against 'foo'.
	bool allowsShortValueWithoutEquals() const{
		return(allowShortValueWithoutEquals); 
	}
	
	///Set whether a short option taking a value may be directly followed by its 
	///value without a separating equals sign. 
	///\param allow whether this form is allowed.
	void allowsShortValueWithoutEquals(bool allow){
		allowShortValueWithoutEquals=allow;
	}
	
	///Whether multiple short options may be written together in a single 
	///argument
	bool allowsShortOptionCombination() const{ 
		return(allowShortOptionCombination);
	}
	
	///Set whether multiple short options may be written together in a single 
	///argument
	///\param allow whether this usage is allowed
	void allowsShortOptionCombination(bool allow){
		allowShortOptionCombination=allow;
	}
	
	///Whether the special option '--' ends option parsing
	bool allowsOptionTerminator() const{ return(allowOptionTerminator); }
	
	///Set whether the special option '--' ends option parsing
	///\param allow whether this feature is enabled
	void allowsOptionTerminator(bool allow){
		allowOptionTerminator=allow;
		if(allowOptionTerminator)
			usageMessage+=" --: Treat all subsequent arguments as postional.";
	}
	
	///Whether help text will use ANSI escape sequences for fancier text rendering
	bool usesANSICodes() const{ return(useANSICodes); }
	
	///Set whether help text will use ANSI escape sequences for fancier text 
	///rendering
	///\param use whether escape codes will be used
	void usesANSICodes(bool use){ useANSICodes=use; }
	
	///Add a short option which stores a value to a variable
	///\param ident the name of the option
	///\param destination the variable to which the option's value will be stored
	///\param description the description of the option
	template<typename T>
	void addOption(char ident, T& destination, std::string description, std::string valueName="value"){
		checkIdentifier(std::string(1,ident));
		addOption(ident,destination,shortOptions);
		description=indentDescription(description);
		std::ostringstream ss;
		ss  << " -" << ident << ' ' << underline(valueName) << ": " << description << '\n';
		usageMessage+=ss.str();
	}
	///Add a short option which invokes a callback which takes no argument
	///\param ident the name of the option
	///\param action the callback function
	///\param description the description of the option
	void addOption(char ident, std::function<void()> action, std::string description){
		checkIdentifier(std::string(1,ident));
		addOption(ident,std::move(action),shortOptionsNoStore);
		description=indentDescription(description);
		std::ostringstream ss;
		ss  << " -" << ident << ": " << description << '\n';
		usageMessage+=ss.str();
	}
	///Add a short option which invokes a callback which takes a value
	///\param ident the name of the option
	///\param action the callback function
	///\param description the description of the option
	template<typename DataType>
	void addOption(char ident, std::function<void(DataType)> action, std::string description, std::string valueName="value"){
		checkIdentifier(std::string(1,ident));
		addOption<DataType>(ident,action,shortOptions);
		description=indentDescription(description);
		std::ostringstream ss;
		ss  << " -" << ident << ' ' << underline(valueName) << ": " << description << '\n';
		usageMessage+=ss.str();
	}
	///Add a long option which stores a value to a variable
	///\param ident the name of the option
	///\param destination the variable to which the option's value will be stored
	///\param description the description of the option
	template<typename T>
	void addOption(std::string ident, T& destination, std::string description, std::string valueName="value"){
		checkIdentifier(ident);
		addOption(ident,destination,longOptions);
		description=indentDescription(description);
		std::ostringstream ss;
		ss  << " --" << ident << ' ' << underline(valueName) << ": " << description << '\n';
		usageMessage+=ss.str();
	}
	///Add a long option which invokes a callback which takes no argument
	///\param ident the name of the option
	///\param action the callback function
	///\param description the description of the option
	void addOption(std::string ident, std::function<void()> action, std::string description){
		checkIdentifier(ident);
		addOption(ident,std::move(action),longOptionsNoStore);
		description=indentDescription(description);
		std::ostringstream ss;
		ss  << " --" << ident << ": " << description << '\n';
		usageMessage+=ss.str();
	}
	///Add a long option which invokes a callback which takes a value
	///\param ident the name of the option
	///\param action the callback function
	///\param description the description of the option
	template<typename DataType>
	void addOption(std::string ident, std::function<void(DataType)> action, std::string description, std::string valueName="value"){
		checkIdentifier(ident);
		addOption<DataType>(ident,std::move(action),longOptions);
		description=indentDescription(description);
		std::ostringstream ss;
		ss  << " --" << ident << ' ' << underline(valueName) << ": " << description << '\n';
		usageMessage+=ss.str();
	}
	///Add an option with multiple synonyms which stores a value to a variable
	///\param idents all of the names for the option
	///\param destination the variable to which the option's value will be stored
	///\param description the description of the option
	template<typename T>
	void addOption(std::initializer_list<std::string> idents, T& destination, std::string description, std::string valueName="value"){
		for(auto ident : idents)
			checkIdentifier(ident);
		for(auto ident : idents){
			if(ident.size()==1)
				addOption(ident[0],destination,shortOptions);
			else
				addOption(ident,destination,longOptions);
		}
		description=indentDescription(description);
		std::ostringstream ss;
		ss << ' ' << synonymList(idents) << ' ' << underline(valueName) << ": " << description << '\n';
		usageMessage+=ss.str();
	}
	///Add an option with multiple synonyms which invokes a callback which takes no argument
	///\param idents all of the names for the option
	///\param destination the variable to which the option's value will be stored
	///\param description the description of the option
	void addOption(std::initializer_list<std::string> idents, std::function<void()> action, std::string description){
		for(auto ident : idents)
			checkIdentifier(ident);
		for(auto ident : idents){
			if(ident.size()==1)
				addOption(ident[0],action,shortOptionsNoStore);
			else
				addOption(ident,action,longOptionsNoStore);
		}
		description=indentDescription(description);
		std::ostringstream ss;
		ss << ' ' << synonymList(idents) << ": " << description << '\n';
		usageMessage+=ss.str();
	}
	///Add an option with multiple synonyms which invokes a callback which takes a value
	///\param idents all of the names for the option
	///\param destination the variable to which the option's value will be stored
	///\param description the description of the option
	template<typename DataType>
	void addOption(std::initializer_list<std::string> idents, std::function<void(DataType)> action, std::string description, std::string valueName="value"){
		for(auto ident : idents)
			checkIdentifier(ident);
		for(auto ident : idents){
			if(ident.size()==1)
				addOption<DataType>(ident[0],action,shortOptions);
			else
				addOption<DataType>(ident,action,longOptions);
		}
		description=indentDescription(description);
		std::ostringstream ss;
		ss << ' ' << synonymList(idents) << ' ' << underline(valueName) << ": " << description << '\n';
		usageMessage+=ss.str();
	}
	
	///Parse a collection of arguments
	///\param argBegin an iterator referencing the first argument
	///\param argEnd an iterator referencing the point after all arguments
	///\return the positional arguments in the order they were encountered in 
	///        the input
	template<typename Iterator>
	std::vector<std::string> parseArgs(Iterator argBegin, Iterator argEnd){
		std::vector<std::string> positionals;
		while(argBegin!=argEnd){
			std::string arg=*argBegin;
			ArgumentState state=handleNextArg(arg);
			switch(state.type){
				case ArgumentState::Option:
					//nothing left to do
					break;
				case ArgumentState::NonOption:
					//treat as a positional argument
					positionals.push_back(arg);
					break;
				case ArgumentState::OptionNeedsValue:
					argBegin++;
					if(argBegin==argEnd)
						throw std::runtime_error("Missing value for '"+arg+"'");
					handleOptWithValue(state.option,*argBegin);
					break;
				case ArgumentState::OptionTerminator:
					//no more option parsing should be done; shove all remaining
					//arguments into positionals
					for(argBegin++; argBegin!=argEnd; argBegin++)
						positionals.push_back(*argBegin);
					break;
			}
			
			//move to the next argument, unless we know all arguments have 
			//already been consumed
			if(state.type!=ArgumentState::OptionTerminator)
				argBegin++;
		}
		return(positionals);
	}
	///Parse a collection of arguments
	///\param argc the number of arguments
	///\param argv the array of arguments
	///\return the positional arguments in the order they were encountered in 
	///        the input
	std::vector<std::string> parseArgs(int argc, char* argv[]){
		return(parseArgs(argv,argv+argc));
	}
	///Parse a collection of arguments
	///\param argc the number of arguments
	///\param argv the array of arguments
	///\return the positional arguments in the order they were encountered in 
	///        the input
	std::vector<std::string> parseArgs(int argc, const char* argv[]){
		return(parseArgs(argv,argv+argc));
	}
};

#endif //CL_OPTIONS_H
