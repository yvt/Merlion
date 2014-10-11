
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
  called "Local" therefore Merlion can be used on single server configuration 
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

Test Server
-----------

When debugging the Merlion application, a complete server stack might be 
too bulky. So we included a testing server called MerlionSimpleServer which
only supports one application domain and one application version.

Using MerlionSimpleServer for production use is not recommended.

Building
--------

TODO

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
  server is also forwarded to the master server, and are displayed in the
  web console.

TODO

Creating Application
--------------------

Please take a look at an example application MerEcho.

TODO