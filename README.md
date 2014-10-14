
Merlion: Real-time Network Application Server
============================================

Merlion is an application server for .NET-based real-time network applications 
with an integrated load balancer.

What is it?
-----------

Merlion is designed to handle heavy-load .NET network applications with many
simultaneous connections by distributing the workload across several 
node servers.

Merlion also provides an easy deployment of the application via the integrated
web console. Deployment and upgrade of the application can be done with no
down time. Running more than two versions at once is even possible to evaluate
the newly created version before it gets ready for public use.

Following diagram shows an example configuration of Merlion servers.

```
         +------------------------+  +------------------------+     
         |                        |  |                        |     
         | +---------------+      |  | +---------------+      |     
         | | Version Alpha <--+   |  | | Version Alpha <--+   |     
         | +---------------+  |   |  | +---------------+  |   |     
         |                    |   |  |                    |   |     
         | +---------------+  |   |  | +---------------+  |   |     
         | | Version Beta  <--+   |  | | Version Beta  <--+   |     
         | +---------------+  |   |  | +---------------+  |   |  ...
Nodes    |                    |   |  |                    |   |     
         | +------------------+-+ |  | +------------------+-+ |     
         | |                    | |  | |                    | |     
         | |    Merlion Node    | |  | |    Merlion Node    | |     
         | |                    | |  | |                    | |     
         | +---------^----------+ |  | +---------^----------+ |     
         |           |            |  |           |            |     
         +------------------------+  +------------------------+     
                     |                           |                  
                     +-------+          +--------+                  
                             |          |                           
                   +-------------------------------+                
                   |         |          |          |                
                   |  +------v----------v-------+  |                
                   |  |                         |  |                
Master             |  |  Merlion Master Server  |  |                
                   |  |                         |  |                
                   |  +------------^------------+  |                
                   |               |               |                
                   +-------------------------------+                
                                   |                                
                                   | TLS                            
                                   |                                
                                   v                                
                           So many clients...   
```                    

### Key Elements

* _Master server_ accepts connection from clients, binds each of them to
  a _domain_, and makes a bridge between the client and the domain.
  Master server also manages the lifetime of application versions and 
  associated application domains. Master server owns its own node server 
  called "Local", therefore Merlion can be used on single server configuration 
  before scaling out the server cluster.
* _Node server_ is where the application actually run. It creates application 
  domains on which each version of the application is deployed.
* _Application domain_ is an isolated environment where a certain version of 
  the Merlion application runs. 
* _Application version_ is a package of Merlion application. Each version is
  stored in the master server as a zip file, and transferred to node servers
  and extracted as needed. Each version has a name.
* _Room_ is a kind of token which is created by the application as needed and
  can be used by clients to connect to the certain application domain later.

Screenshots
-----------

![Web Console](https://dl.dropboxusercontent.com/u/37804131/github/Screen%20Shot%202014-10-14%20at%205.35.02%20PM.png)
![Web Console](https://dl.dropboxusercontent.com/u/37804131/github/Screen%20Shot%202014-10-14%20at%205.35.25%20PM.png)


Test Server
-----------

When debugging the Merlion application, a complete server stack might be 
too bulky. So we included a testing server called MerlionSimpleServer which
only supports one application domain and one application version.

Using MerlionSimpleServer for production use is not recommended.

Requirements
------------

* Currently, only Linux and OS X 10.9 Maverics are supported.
* Mono 3.2.8 or later is required.
* OpenSSL is required.
* Boost 1.55.0 or later which was built with the C++ compiler you are going to
  build MerlionServerCore with.
* ISO/IEC 14882:2011 (a.k.a. C++11) compliant compiler.
  Clang is recommended (#1).


Building
--------

1. Run `cmake .` in MerlionServerCore. 
   Using Clang instead of GCC is recommended. To instruct CMake to use Clang,
   pass `-DCMAKE_CXX_COMPILER=/usr/bin/clang++` to CMake.
2. Build MerlionServerCore by running `make`.
3. Run `nuget restore` in the source root to download required libraries.
3. Build .NET solution by running `xbuild` in the source root.
4. Copy (or make a symbolic link) MerlionServerCore/libMerlionServerCore.so to
   MerlionServer/bin/Debug/libMerlionServerCore.so.

Configuring Server
------------------

Merlion server uses the configuration file "MerlionServer.exe.config" which must
be placed at the same location as the executable file (MerlionServer.exe).

### Master

* _EndpointAddress_
* _MasterServerAddress_ is the address where the master server listens for
  nodes at.
* _MasterServerCertificate_ is the certificate file used during TLS handshake.
* _MasterServerPrivateKey_ is the private key file of _MasterServerCertificate_.
* If _DisallowClientSideVersionSpecification_ is set to `True`, client cannot
  specify the application version name.
* _WebInterfaceAddress_ is the address of the web console.

TODO

### Node

* _NodeName_ is the node's name. Preferred to keep it unique.
* When _ForwardLogToMaster_ is set to `True`, log messages recorded by the node
  server are also forwarded to the master server, and are displayed in the
  web console.

TODO

Creating Application
--------------------

Please take a look at an example application MerEcho.

TODO
