#include <fstream>
#include <iostream>
#include <string>

#include <termios.h>

#include <s3tools/cred_manage.h>

#include "external/cl_options.h"

int main(int argc, char* argv[]){
	std::string usage=R"(NAME
 s3cred - manage S3 credentials
	
USAGE
 s3cred list|add|delete|help [arguments]

SUBCOMMANDS
 list
    List all stored credentials.
 add
    Add a credential record. 
    Interactive prompts are given for necessary information. 
 delete URL
    Delete any credential record associated with URL.
 import [file]
    Add credential records read from a file,
    or from stdin if no file is specified.
 export [--json] [url]
    Write credential data to stdout.
    If the --json option is specified, data is written as JSON.
    If a URL is specified, only the credential best matching
    that URL is exported.

NOTES
 s3cred performs no validation that credentials are valid or even well-formed.
)";

	OptionParser op(true);
	op.setBaseUsage(usage);
	bool outputJSON=false;
	op.addOption("json", [&]{outputJSON=true;}, "Export credentials as JSON");
	auto arguments=op.parseArgs(argc,argv);
	
	if(op.didPrintUsage())
		return(0);
	if(arguments.size()==1){
		std::cout << op.getUsage() << std::endl;
		return(0);
	}
	std::string subcommand=arguments[1];
	
	if(subcommand!="export" && outputJSON)
		std::cerr << "--json has no effect for actions other than export\n";

	if(subcommand=="list"){
		try{
			auto credentials=s3tools::fetchStoredCredentials();
			for(const auto& cred : credentials)
				std::cout << cred.first << ": " << cred.second.username << std::endl;
			return(0);
		}catch(std::exception& ex){
			std::cerr << "s3cred: Error: " << ex.what() << std::endl;
			return(1);
		}
	}
	if(subcommand=="add"){
		std::string url, username, key, keyAgain;
		std::cout << "URL: "; std::cout.flush();
		std::cin >> url;
		std::cin.ignore(1,'\n'); //discard the terminating newline
		std::cout << "username: "; std::cout.flush();
		std::getline(std::cin,username);
		std::cout << "key: "; std::cout.flush();
		termios tstate;
		tcgetattr(0,&tstate);
		termios tstatem=tstate;
		tstatem.c_lflag&=~ECHO;
		tcsetattr(0,TCSANOW,&tstatem);
		std::getline(std::cin,key);
		//We write a newline since the one entered by the user won't echo
		std::cout << std::endl;
		std::cout << "key again: "; std::cout.flush();
		std::getline(std::cin,keyAgain);
		//Write our own newline again
		std::cout << std::endl;
		tcsetattr(0,TCSANOW,&tstate);
		if(key!=keyAgain){
			std::cerr << "Two versions of key did not match" << std::endl;
			return(1);
		}
		try{
			bool stored=storeCredential(url,s3tools::credential{username,key});
			if(!stored){
				std::cout << "The credential store already contains an entry for "
				<< url << "\nDo you want to overwrite it? [y/N]: "; std::cout.flush();
				char overwrite;
				std::cin >> overwrite;
				if(overwrite=='y' || overwrite=='Y')
					stored=storeCredential(url,s3tools::credential{username,key},true);
			}
			return(0);
		}catch(std::exception& ex){
			std::cerr << "s3cred: Error: " << ex.what() << std::endl;
			return(1);
		}
	}
	if(subcommand=="delete"){
		if(arguments.size()!=3){
			std::cerr << "Usage: s3cred delete URL" << std::endl;
			return(1);
		}
		try{
			std::string url=arguments[2];
			bool success=s3tools::removeCredential(url);
			if(success){
				std::cout << "Removed credential information associated with " << url << std::endl;
				return(0);
			}
			else{
				std::cout << "Found no credential information associated with " << url << std::endl;
				return(1);
			}
		}catch(std::exception& ex){
			std::cerr << "s3cred: Error: " << ex.what() << std::endl;
			return(1);
		}
	}
	if(subcommand=="import"){
		try{
			int added=0;
			if(arguments.size()==3){
				std::ifstream importFile(arguments[2]);
				if(!importFile){
					std::cerr << "s3cred: Error: Unable to open " << arguments[2] << " for reading" << std::endl;
					return(1);
				}
				added=s3tools::importCredentials(importFile, arguments[2]);
			}
			else
				added=s3tools::importCredentials(std::cin, "standard input");
			std::cout << "Imported " << added << " credential" << (added==1?"":"s") << std::endl;
			return(0);
		}catch(std::exception& ex){
			std::cerr << "s3cred: Error: " << ex.what() << std::endl;
			return(1);
		}
	}
	if(subcommand=="export"){
		try{
			s3tools::CredentialCollection credentials=s3tools::fetchStoredCredentials();
			if(arguments.size()==3)
				s3tools::exportCredentials(std::cout, credentials, arguments[2],
				                           outputJSON?s3tools::CredFormat::JSON:s3tools::CredFormat::Internal);
			else
				s3tools::exportCredentials(std::cout, credentials,
				                           outputJSON?s3tools::CredFormat::JSON:s3tools::CredFormat::Internal);
			return(0);
		}catch(std::exception& ex){
			std::cerr << "s3cred: Error: " << ex.what() << std::endl;
			return(1);
		}
	}
	if(subcommand=="help"){
		std::cout << op.getUsage() << std::endl;
		return(0);
	}
	std::cerr << "Unrecognized subcommand" << std::endl;
	return(1);
}
