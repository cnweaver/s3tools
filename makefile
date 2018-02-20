include settings.mk

STATLIB:=lib/libs3tools.a
LIBOBJECTS=build/url.o build/signing.o build/cred_manage.o
PROGRAMS=bin/s3bucket bin/s3cred bin/s3cp bin/s3ls bin/s3rm bin/s3sign
TESTS=tests/url_tests

all : $(STATLIB) $(PROGRAMS)

$(STATLIB) : $(LIBOBJECTS)
	ar -rcs $(STATLIB) $(LIBOBJECTS)

build/url.o : src/url.cpp include/s3tools/url.h settings.mk
	$(CXX) $(CXXFLAGS) -c src/url.cpp -o build/url.o

build/signing.o : src/signing.cpp include/s3tools/signing.h include/s3tools/url.h settings.mk
	$(CXX) $(CXXFLAGS) $(CRYPTOPP_CFLAGS) -c src/signing.cpp -o build/signing.o

build/cred_manage.o : src/cred_manage.cpp include/s3tools/cred_manage.h settings.mk
	$(CXX) $(CXXFLAGS) -c src/cred_manage.cpp -o build/cred_manage.o

bin/s3cred : build/s3cred.o $(STATLIB)
	$(CXX) build/s3cred.o $(STATLIB) $(LDFLAGS) -o bin/s3cred

build/s3cred.o : src/s3cred.cpp include/s3tools/cred_manage.h settings.mk
	$(CXX) $(CXXFLAGS) -c src/s3cred.cpp -o build/s3cred.o

build/curl_utils.o : src/curl_utils.cpp src/curl_utils.h settings.mk
	$(CXX) $(CXXFLAGS) $(LIBCURL_CFLAGS) -c src/curl_utils.cpp -o build/curl_utils.o

build/xml_utils.o : src/xml_utils.cpp src/xml_utils.h settings.mk
	$(CXX) $(CXXFLAGS) $(LIBXML2_CFLAGS) -c src/xml_utils.cpp -o build/xml_utils.o

bin/s3bucket : build/s3bucket.o build/curl_utils.o build/xml_utils.o $(STATLIB)
	$(CXX) build/s3bucket.o build/curl_utils.o build/xml_utils.o $(STATLIB) $(LDFLAGS) $(CRYPTOPP_LDFLAGS) $(LIBCURL_LDFLAGS) $(LIBXML2_LDFLAGS) -o bin/s3bucket

build/s3bucket.o : src/s3bucket.cpp include/s3tools/cred_manage.h include/s3tools/signing.h include/s3tools/url.h src/xml_utils.h settings.mk
	$(CXX) $(CXXFLAGS) $(LIBCURL_CFLAGS) $(LIBXML2_CFLAGS) -c src/s3bucket.cpp -o build/s3bucket.o

bin/s3cp : build/s3cp.o build/curl_utils.o build/xml_utils.o $(STATLIB)
	$(CXX) build/s3cp.o build/curl_utils.o build/xml_utils.o $(STATLIB) $(LDFLAGS) $(CRYPTOPP_LDFLAGS) $(LIBCURL_LDFLAGS) $(LIBXML2_LDFLAGS) -o bin/s3cp

build/s3cp.o : src/s3cp.cpp include/s3tools/cred_manage.h include/s3tools/signing.h include/s3tools/url.h settings.mk
	$(CXX) $(CXXFLAGS) $(LIBCURL_CFLAGS) $(LIBXML2_CFLAGS) -c src/s3cp.cpp -o build/s3cp.o

bin/s3ls : build/s3ls.o build/curl_utils.o build/xml_utils.o $(STATLIB)
	$(CXX) build/s3ls.o build/curl_utils.o build/xml_utils.o $(STATLIB) $(LDFLAGS) $(CRYPTOPP_LDFLAGS) $(LIBCURL_LDFLAGS) $(LIBXML2_LDFLAGS) -o bin/s3ls

build/s3ls.o : src/s3ls.cpp include/s3tools/cred_manage.h include/s3tools/signing.h include/s3tools/url.h src/xml_utils.h settings.mk
	$(CXX) $(CXXFLAGS) $(LIBCURL_CFLAGS) $(LIBXML2_CFLAGS) -c src/s3ls.cpp -o build/s3ls.o

bin/s3rm : build/s3rm.o build/curl_utils.o build/xml_utils.o $(STATLIB)
	$(CXX) build/s3rm.o build/curl_utils.o build/xml_utils.o $(STATLIB) $(LDFLAGS) $(CRYPTOPP_LDFLAGS) $(LIBCURL_LDFLAGS) $(LIBXML2_LDFLAGS) -o bin/s3rm

build/s3rm.o : src/s3rm.cpp include/s3tools/cred_manage.h include/s3tools/signing.h include/s3tools/url.h src/curl_utils.h src/xml_utils.h settings.mk
	$(CXX) $(CXXFLAGS) $(LIBCURL_CFLAGS) $(LIBXML2_CFLAGS) -c src/s3rm.cpp -o build/s3rm.o

bin/s3sign : build/s3sign.o $(STATLIB)
	$(CXX) build/s3sign.o $(STATLIB) $(LDFLAGS) $(CRYPTOPP_LDFLAGS) -o bin/s3sign

build/s3sign.o : src/s3sign.cpp include/s3tools/cred_manage.h include/s3tools/signing.h include/s3tools/url.h settings.mk
	$(CXX) $(CXXFLAGS) -c src/s3sign.cpp -o build/s3sign.o

tests/url_tests : build/url_tests.o $(STATLIB)
	$(CXX) build/url_tests.o $(STATLIB) $(LDFLAGS) $(CRYPTOPP_LDFLAGS) $(LIBCURL_LDFLAGS) $(LIBXML2_LDFLAGS) -o tests/url_tests

build/url_tests.o : tests/url_tests.cpp include/s3tools/url.h
	$(CXX) $(CXXFLAGS) -c tests/url_tests.cpp -o build/url_tests.o

test : $(TESTS)
	./tests/url_tests

clean : 
	rm -f build/*.o
	rm -f $(STATLIB)
	rm -f $(PROGRAMS)
	rm -f $(TESTS)

install: $(STATLIB) $(PROGRAMS)
	@echo Installing headers in $(PREFIX)/include/s3tools
	@mkdir -p $(PREFIX)/include/s3tools
	@cp include/s3tools/*.h $(PREFIX)/include/s3tools/
	@echo Installing library in $(PREFIX)/lib
	@mkdir -p $(PREFIX)/lib
	@cp $(STATLIB) $(PREFIX)/lib/
	@echo Installing tools in $(PREFIX)/bin
	@mkdir -p $(PREFIX)/bin
	@cp bin/* $(PREFIX)/bin/

uninstall:
	@echo Removing headers from $(PREFIX)/include/s3tools
	@rm -rf $(PREFIX)/include/s3tools
	@echo Removing library from $(PREFIX)/lib
	@rm -f $(PREFIX)/$(STATLIB)
	@echo Removing tools from $(PREFIX)/bin
	@echo $(PROGRAMS) | xargs -n 1 echo | sed 's|\(.*\)|'$(PREFIX)'/\1|' | xargs rm -f

.PHONY : all clean install uninstall test
