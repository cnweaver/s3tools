Overview
--------

This library and collection of tools is intended to provide a light-weight and portable means of interacting with Amazon S3 compatible storage systems. The primary areas it covers are producing 'presigned' URLs (much faster than by calling into boto3) and a set of command line tools for convenient interactive access to data. 

Installation
------------

### Dependencies

This software requires:

- [libcurl](https://curl.haxx.se/libcurl/) for making HTTP(S) requests
- [libxml2](http://www.xmlsoft.org) for parsing XML responses
- [Crypto++](https://cryptopp.com) for generating cryptographic signatures

### Building

In the ideal case building and installing the tools should be as simple as

	./configure
	make
	make install

If you wish to install to a prefix other than `/usr/local`, this can be specified by passing `--prefix=/your/preferred/path` to `configure`. It is also possible that `configure` will not manage to automatically detect a dependency (Crypto++ offers very little assistance in this regard, unfortunately), in which case additional arguments will need to be passed to `configure`; see `./configure --help` for details. 

Usage
-----

### Interactive Use

One of the main purposes of s3tools is to provide a basic set of unix tools for interactive manipulation of data stored on an S3 server. These cover only a subset of the total S3 functionality, but are intended to be simple to use, particularly for users who have minimal familiarity with S3. They are not intended for large-scale or prgrammatic data transfer, but rather for day-to-day quick use for simple tasks. 

In order to reduce the amount of typing required when running S3 related commands, s3tools stores credential information (in a file ~/.s3_credentials, and enforcing permissions such that other users of the same system cannot access it). Note that at this time credentials are stored unencrypted, so they could potentially be stolen by a hostile administrator, etc. 

Since all other interactive commands will look for stored credentials to use, a good place to begin is using `s3cred add` to store at least one set of credentials to be used. This command takes no further arguments, and prompts interactively for the information it needs:

	$ s3cred add
	URL: https://example.com
	username: user
	key: 
	key again: 
	$ 

`s3cred list` can be used at any time to list the currently stored credentials. The secret key stored for each credential entry is not displayed. 

	$ s3cred list
	https://example.com: user
	$ 

Stored credentials can be removed again via `s3cred delete`. 

Once credentials are stored, it is possible to interact with a server. The most basic operation is to list objects (files) or buckets using `s3ls`. Continuing from the above example, typical use might look like:

	$ s3ls https://example.com
	bucket1/
	bucket2/
	$ s3ls https://example.com/bucket1
	fileA
	fileB
	$ s3ls -lh https://example.com/bucket1
	fileA	 2018-02-03T21:50:05.397Z	 72B
	fileB	 2018-02-03T22:45:26.988Z	 126B
	$ 

`s3rm` can be used to delete objects. While it can accept multiple arguments to be deleted, it is currently limited to sending a separate request per deletion, and does not support any form of 'wildcard' or 'recursive' (by prefix) deletion. 

`s3cp` can be used to upload and download objects, as well as copying them o the server. Usage is hopefully suitable analogous to `cp` or `scp`, with remove sources or destinations specified as URLs:

	$ echo "ABCdef" > fileC
	$ s3cp fileC https://example.com/bucket1/fileC
	$ s3ls -lh https://example.com/bucket1
	fileA	 2018-02-03T21:50:05.397Z	 72B
	fileB	 2018-02-03T22:45:26.988Z	 126B
	fileC	 2018-02-19T23:17:05.932Z	 7B
	$ 

`s3cp` currently has a few limitations: When uploading it requires the full target object name to be used in the URL (`s3cp fileC https://example.com/bucket1/` is not able to guess that the object name should probably be `fileC` in `bucket1` as one might expect), and it likewise can only transfer one file at a time. 

`s3bucket` is also provided to manipulate whole buckets. It has subcommands `list`, `add`, `delete`, and `info`, which should cover the majority of basic operations. 

Finally, somewhat distinct from the rest of the command line tools, `s3sign` provides direct access to producing presigned URLs for S3 objects. This is useful for providing upload or download URLs to other users, or to cluster jobs which can then read or write data without needing certificates or credentials. The resulting URLs should be usable by any program which can perform HTTP requests. Usage is simple:

	$ s3sign https://example.com/bucket1/fileC GET
https://example.com/bucket1/fileC?X-Amz-Algorithm=AWS4-HMAC-SHA256&X-Amz-Credential=user%2F20180219%2Fus-east-1%2Fs3%2Faws4_request&X-Amz-Date=20180219T233030Z&X-Amz-Expires=86400&X-Amz-Signature=fb28f369b3233bcce2588158ffdb982f916d0857d170f6c341b1475c883de57b&X-Amz-SignedHeaders=host
	$ 

This URL would allow anyone who has it to read https://example.com/bucket1/fileC via an HTTP `GET` request for 24 hours after the URL was generated. The validity duration of URLs can be controlled by passing a duration in seconds as the optional third argument to `s3sign`. Any HTTP verb is allowed, although `GET` and `PUT` are likely to be the most useful. `DELETE` can also be used, for example, to accomplish the same task as `s3rm`. 

### Programmatic Use

Currently the only major task supported by the library portion of s3tools for programatic use is generating presigned URLs. This is exactly the functionality exposed by the `s3sign` tool, however, if one needs to produce hundreds or thousands of URLs it can be desirable to avoid the overhead of starting a separate process to sign each one. The necessary interfaces are provided by the `<s3tools/signing.h>` header, and the only one needed for most cases is 

	s3tools::URL s3tools::genURL(std::string username, std::string secretkey, std::string verb,
           s3tools::URL url, unsigned long exprTime, std::string timestamp="")

This function takes the necessary user credentials, the target URL, a validity time, and optionally a base timestamp at which the URL's validity did/will begin. The alternate `genURLNoQuery` function performs the same task, but instead of using query parameters for authentication, uses request headers. This is more awkward, but is required for certain S3 operations, such as server-side copies. Unless there is a specific need, `genURL` should usually be preferred. 

It may also be useful to use credential storage and retrieval mechanisms via the functions in `<s3tools/cred_manage.h>`. Combining these, the above example using `s3sign` to produce a download URL boils down to the following:

	std::string baseURL="https://example.com/bucket1/fileC";
	std::string verb="GET";
	unsigned long validity=24UL*60*60; //seconds
	auto credentials=s3tools::fetchStoredCredentials();
	auto cred=findCredentials(credentials,baseURL);
	s3tools::URL signedURL=s3tools::genURL(cred.username,cred.key,verb,baseURL,validity);

Features in Detail
------------------

<table>
<tr><th>Feature</th>	<th></th><tr>
<tr><td>List Buckets</td>		<td>Supported</td></tr>
<tr><td>Delete Bucket</td>		<td>Supported</td></tr>
<tr><td>Create Bucket</td>		<td>Supported</td></tr>
<tr><td>Bucket Lifecycle</td>		<td>Not Supported</td></tr>
<tr><td>Policy (Buckets, Objects)</td>	<td>Not Supported</td></tr>
<tr><td>Bucket Website</td>		<td>Not Supported</td></tr>
<tr><td>Bucket ACLs (Get, Put)</td>	<td>Not Supported</td></tr>
<tr><td>Bucket Location</td>		<td>Supported</td></tr>
<tr><td>Bucket Notification</td>	<td>Not Supported</td></tr>
<tr><td>Bucket Object Versions</td>	<td>Not Supported</td></tr>
<tr><td>Get Bucket Info (HEAD)</td>	<td>Not Supported</td></tr>
<tr><td>Put Object</td>			<td>Supported</td></tr>
<tr><td>Delete Object</td>		<td>Supported</td></tr>
<tr><td>Get Object</td>			<td>Supported</td></tr>
<tr><td>Object ACLs (Get, Put)</td>	<td>Not Supported</td></tr>
<tr><td>Get Object Info (HEAD)</td>	<td>Not Supported</td></tr>
<tr><td>POST Object</td>		<td>Not Supported</td></tr>
<tr><td>Copy Object</td>		<td>Supported</td></tr>
<tr><td>Multipart Uploads</td>		<td>Not Supported</td></tr>
</table>

- All authetication is done using AWS signature version 4. 
- All buckets are assumed to be in the 'us-east-1' region at this time. 
- GET requests for ranges of data are not yet supported. 
- The lack of multipart uploads will cause uploads of objects greater than 5GB in size to Amazon S3 to fail. However, other S3 implementations typically do not have this limitation. 
