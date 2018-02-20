#include <s3tools/url.h>
#include <s3tools/signing.h>
#include <cassert>

#include <iostream>

int main(){
	using s3tools::URL;
	
	{ //only a scheme and a path
		URL url("file:///foo/bar.baz");
		assert(url.scheme=="file");
		assert(url.path=="/foo/bar.baz");
	}
	{ //only a scheme and a host
		URL url("http://example.com");
		assert(url.scheme=="http");
		assert(url.host=="example.com");
		assert(url.port==80); //port should be 80 for http
		assert(url.path=="/");
	}
	{ //scheme, host and path
		URL url("http://example.com/foo");
		assert(url.scheme=="http");
		assert(url.host=="example.com");
		assert(url.port==80); //port should be 80 for http
		assert(url.path=="/foo");
	}
	{ //scheme, host, port and path
		URL url("http://example.com:8080/foo");
		assert(url.scheme=="http");
		assert(url.host=="example.com");
		assert(url.port==8080);
		assert(url.path=="/foo");
	}
	{ //scheme, host, port, path, and query with one parameter
		URL url("http://example.com:8080/foo?bar=baz");
		assert(url.scheme=="http");
		assert(url.host=="example.com");
		assert(url.port==8080);
		assert(url.path=="/foo");
		assert(url.query.size()==1);
		assert(url.query.begin()->first=="bar");
		assert(url.query.begin()->second=="baz");
	}
	{ //scheme, host, port, path, and query with two parameters
		URL url("http://example.com:8080/foo?bar=baz&quux=xen");
		assert(url.scheme=="http");
		assert(url.host=="example.com");
		assert(url.port==8080);
		assert(url.path=="/foo");
		assert(url.query.size()==2);
		auto it=url.query.begin();
		assert(it->first=="bar");
		assert(it->second=="baz");
		it++;
		assert(it->first=="quux");
		assert(it->second=="xen");
	}
	{ //scheme, host, port, path, query with one parameter, and a fragment
		URL url("http://example.com:8080/foo?bar=baz#quux");
		assert(url.scheme=="http");
		assert(url.host=="example.com");
		assert(url.port==8080);
		assert(url.path=="/foo");
		assert(url.query.size()==1);
		assert(url.query.begin()->first=="bar");
		assert(url.query.begin()->second=="baz");
		assert(url.fragment=="quux");
	}
	{ //scheme, username, host, port and path
		URL url("http://user@example.com:8080/foo");
		assert(url.scheme=="http");
		assert(url.username=="user");
		assert(url.host=="example.com");
		assert(url.port==8080);
		assert(url.path=="/foo");
	}
	{ //scheme, username, password, host, port and path
		URL url("http://user:pass@example.com:8080/foo");
		assert(url.scheme=="http");
		assert(url.username=="user");
		assert(url.password=="pass");
		assert(url.host=="example.com");
		assert(url.port==8080);
		assert(url.path=="/foo");
	}
}