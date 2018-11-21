INTRODUCTION
-------------
	This is a proxy server that can handle HTTP requests from the browser. In this project, the primary 
	responsiblity of this server is to maintain the files in the cache memory such that the requests can be 
	processed at very high speed using the pre-stored files.

BUILD AND RUN STEPS
-------------------
	This project can be built on any Linux platform. Follow the below steps to build and run the
	project,

	1.) Clone this project using the command "git clone https://github.com/sorabhgandhi01/Proxy-Server.git"
	2.) Go into the project folder using the command "cd proxy-server"
	3.) Run make
	4.) Finally, run the server using the command - ./server [Port Number] [TimeOut]


IMPLEMENTATION SUMMARY
----------------------
	This server is limited to handle only GET requests and HTTP requests from the client. The client
	request is parsed to validate the method, version and URL. Then the given URL is resolved and
	for the invalid domain name, it returns 404 error. For invalid method and version, it returns
	400 Bad Request error.

	The server then stores the domain-name and associated IP address in IPcache.txt. It then looks for
	the requested files in pagecache.txt. If the file is not present, then it fetches the files from 
	remote server and then stores in the local cache.

	All prefetched page in cache filesystem is stored with a timestamp. So, if the timeout exceeds the
	timestamp by a threshold value then the pages are re-fetched from the server and are stored back.

	This server also has capablity to pre-fetch the links, for a speedy page processing and delivery.
