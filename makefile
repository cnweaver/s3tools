include settings.mk

STATLIB:=lib/libs3tools.a
LIBOBJECTS=build/url.o build/signing.o build/cred_manage.o
PROGRAMS=bin/s3bucket bin/s3cred bin/s3cp bin/s3ls bin/s3rm bin/s3sign
TESTS=tests/url_tests

all : $(STATLIB) $(PROGRAMS) settings.mk

settings.mk :
	@echo "  You need to run ./configure before make  "
	@false

$(STATLIB) : $(LIBOBJECTS)
	ar -rcs $(STATLIB) $(LIBOBJECTS)

build/url.o : $(SOURCE_DIR)/src/url.cpp $(SOURCE_DIR)/include/s3tools/url.h settings.mk
	$(CXX) $(CXXFLAGS) -c $(SOURCE_DIR)/src/url.cpp -o build/url.o

build/signing.o : $(SOURCE_DIR)/src/signing.cpp $(SOURCE_DIR)/include/s3tools/signing.h $(SOURCE_DIR)/include/s3tools/url.h settings.mk
	$(CXX) $(CXXFLAGS) $(CRYPTOPP_CFLAGS) -c $(SOURCE_DIR)/src/signing.cpp -o build/signing.o

build/cred_manage.o : $(SOURCE_DIR)/src/cred_manage.cpp $(SOURCE_DIR)/include/s3tools/cred_manage.h settings.mk
	$(CXX) $(CXXFLAGS) -c $(SOURCE_DIR)/src/cred_manage.cpp -o build/cred_manage.o

bin/s3cred : build/s3cred.o $(STATLIB)
	$(CXX) build/s3cred.o $(STATLIB) $(LDFLAGS) -o bin/s3cred

build/s3cred.o : $(SOURCE_DIR)/src/s3cred.cpp $(SOURCE_DIR)/include/s3tools/cred_manage.h settings.mk
	$(CXX) $(CXXFLAGS) -c $(SOURCE_DIR)/src/s3cred.cpp -o build/s3cred.o

build/curl_utils.o : $(SOURCE_DIR)/src/curl_utils.cpp $(SOURCE_DIR)/src/curl_utils.h settings.mk
	$(CXX) $(CXXFLAGS) $(LIBCURL_CFLAGS) -c $(SOURCE_DIR)/src/curl_utils.cpp -o build/curl_utils.o

build/xml_utils.o : $(SOURCE_DIR)/src/xml_utils.cpp $(SOURCE_DIR)/src/xml_utils.h settings.mk
	$(CXX) $(CXXFLAGS) $(LIBXML2_CFLAGS) -c $(SOURCE_DIR)/src/xml_utils.cpp -o build/xml_utils.o

bin/s3bucket : build/s3bucket.o build/curl_utils.o build/xml_utils.o $(STATLIB)
	$(CXX) build/s3bucket.o build/curl_utils.o build/xml_utils.o $(STATLIB) $(CRYPTOPP_LDFLAGS) $(LIBCURL_LDFLAGS) $(LIBXML2_LDFLAGS) $(LDFLAGS) -o bin/s3bucket

build/s3bucket.o : $(SOURCE_DIR)/src/s3bucket.cpp $(SOURCE_DIR)/include/s3tools/cred_manage.h $(SOURCE_DIR)/include/s3tools/signing.h $(SOURCE_DIR)/include/s3tools/url.h $(SOURCE_DIR)/src/xml_utils.h settings.mk
	$(CXX) $(CXXFLAGS) $(LIBCURL_CFLAGS) $(LIBXML2_CFLAGS) -c $(SOURCE_DIR)/src/s3bucket.cpp -o build/s3bucket.o

bin/s3cp : build/s3cp.o build/curl_utils.o build/xml_utils.o $(STATLIB)
	$(CXX) build/s3cp.o build/curl_utils.o build/xml_utils.o $(STATLIB) $(CRYPTOPP_LDFLAGS) $(LIBCURL_LDFLAGS) $(LIBXML2_LDFLAGS) $(LDFLAGS) -o bin/s3cp

build/s3cp.o : $(SOURCE_DIR)/src/s3cp.cpp $(SOURCE_DIR)/include/s3tools/cred_manage.h $(SOURCE_DIR)/include/s3tools/signing.h $(SOURCE_DIR)/include/s3tools/url.h settings.mk
	$(CXX) $(CXXFLAGS) $(LIBCURL_CFLAGS) $(LIBXML2_CFLAGS) -c $(SOURCE_DIR)/src/s3cp.cpp -o build/s3cp.o

bin/s3ls : build/s3ls.o build/curl_utils.o build/xml_utils.o $(STATLIB)
	$(CXX) build/s3ls.o build/curl_utils.o build/xml_utils.o $(STATLIB) $(CRYPTOPP_LDFLAGS) $(LIBCURL_LDFLAGS) $(LIBXML2_LDFLAGS) $(LDFLAGS) -o bin/s3ls

build/s3ls.o : $(SOURCE_DIR)/src/s3ls.cpp $(SOURCE_DIR)/include/s3tools/cred_manage.h $(SOURCE_DIR)/include/s3tools/signing.h $(SOURCE_DIR)/include/s3tools/url.h $(SOURCE_DIR)/src/xml_utils.h settings.mk
	$(CXX) $(CXXFLAGS) $(LIBCURL_CFLAGS) $(LIBXML2_CFLAGS) -c $(SOURCE_DIR)/src/s3ls.cpp -o build/s3ls.o

bin/s3rm : build/s3rm.o build/curl_utils.o build/xml_utils.o $(STATLIB)
	$(CXX) build/s3rm.o build/curl_utils.o build/xml_utils.o $(STATLIB) $(CRYPTOPP_LDFLAGS) $(LIBCURL_LDFLAGS) $(LIBXML2_LDFLAGS) $(LDFLAGS) -o bin/s3rm

build/s3rm.o : $(SOURCE_DIR)/src/s3rm.cpp $(SOURCE_DIR)/include/s3tools/cred_manage.h $(SOURCE_DIR)/include/s3tools/signing.h $(SOURCE_DIR)/include/s3tools/url.h $(SOURCE_DIR)/src/curl_utils.h $(SOURCE_DIR)/src/xml_utils.h settings.mk
	$(CXX) $(CXXFLAGS) $(LIBCURL_CFLAGS) $(LIBXML2_CFLAGS) -c $(SOURCE_DIR)/src/s3rm.cpp -o build/s3rm.o

bin/s3sign : build/s3sign.o $(STATLIB)
	$(CXX) build/s3sign.o $(STATLIB) $(CRYPTOPP_LDFLAGS) $(LDFLAGS) -o bin/s3sign

build/s3sign.o : $(SOURCE_DIR)/src/s3sign.cpp $(SOURCE_DIR)/include/s3tools/cred_manage.h $(SOURCE_DIR)/include/s3tools/signing.h $(SOURCE_DIR)/include/s3tools/url.h settings.mk
	$(CXX) $(CXXFLAGS) -c $(SOURCE_DIR)/src/s3sign.cpp -o build/s3sign.o

tests/url_tests : build/url_tests.o $(STATLIB)
	$(CXX) build/url_tests.o $(STATLIB) $(CRYPTOPP_LDFLAGS) $(LIBCURL_LDFLAGS) $(LIBXML2_LDFLAGS) $(LDFLAGS) -o tests/url_tests

build/url_tests.o : $(SOURCE_DIR)/tests/url_tests.cpp include/s3tools/url.h
	$(CXX) $(CXXFLAGS) -c $(SOURCE_DIR)/tests/url_tests.cpp -o build/url_tests.o

test : $(TESTS)
	./tests/url_tests

clean : 
	rm -f build/*.o
	rm -f $(STATLIB)
	rm -f $(PROGRAMS)
	rm -f $(TESTS)
	rm -rf linux-static-build

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

linux-static-build : 
	mkdir linux-static-build

linux-static-build/cryptopp/cryptopp.zip : linux-static-build
	mkdir -p linux-static-build/cryptopp
	curl -L $(CRYPTOPP_SRC_URL) -o linux-static-build/cryptopp/cryptopp.zip
	
linux-static-build/cryptopp/Readme.txt : linux-static-build/cryptopp/cryptopp.zip
	unzip -u -d linux-static-build/cryptopp/ linux-static-build/cryptopp/cryptopp.zip

.PHONY : docker-build-env
docker-build-env : $(SOURCE_DIR)/scripts/static_build_env.docker
	docker build -q -t s3tools-env - < $(SOURCE_DIR)/scripts/static_build_env.docker

linux-static-build/bin/s3cp : linux-static-build/cryptopp/Readme.txt docker-build-env
	docker run --rm -v $(SOURCE_DIR):/s3tools s3tools-env /s3tools/scripts/build-static.sh

.PHONY : linux-static
linux-static : linux-static-build/bin/s3cp
