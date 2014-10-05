#pragma once

#ifdef __cplusplus
extern "C" {
#endif
	
	enum MSCLogSeverity
	{
		MSCLS_Debug = -1,
		MSCLS_Info = 0,
		MSCLS_Warn,
		MSCLS_Error,
		MSCLS_Fatal
	};

	enum MSCResult
	{
		MSCR_Success = 0,
		MSCR_Unknown,
		MSCR_InvalidArgument,
		MSCR_InvalidOperation,
		MSCR_InvalidFormat,
		MSCR_InvalidData,
		MSCR_SystemError,
		MSCR_NotImplemented
	};

	extern const char *MSCGetLastErrorMessage();
	
	typedef void (*MSCLogSink)
	(MSCLogSeverity severity, const char *source, const char *host, const char *channel, const char *message, void *userdata);
	typedef void *MSCLogSinkHandle;
	
	extern MSCResult MSCAddLogSink(MSCLogSink sink, void *userdata, MSCLogSinkHandle *outHandle);
	extern MSCResult MSCRemoveLogSink(MSCLogSinkHandle handle);

	typedef void *MSCLibrary;

	extern MSCResult MSCLibraryCreate(MSCLibrary *libOut);
	extern MSCResult MSCLibraryDestroy(MSCLibrary library);

	typedef void *MSCMaster;
		
	typedef std::uint32_t (*MSCGetVersionPackagePathCallback)
	(const char *versionName, char *outPackagePath, std::uint32_t outPackagePathBufferSize, void *userdata);
	
	typedef std::uint32_t (*MSCDeployPackageCallback)
	(const char *versionName, void *userdata);
	
	enum MSCMasterFlags : std::uint32_t
	{
		MSCMF_None = 0,
		MSCMF_DisallowVersionSpecification = 1 << 0
	};

	struct MSCMasterParameters
	{
		const char *nodeEndpoint;
		const char *clientEndpoint;
		const char *sslCertificateFile;
		const char *sslPrivateKeyFile;
		const char *sslPassword;
		
		MSCGetVersionPackagePathCallback packagePathCallback;
		void *packagePathCallbackUserData;
		
		MSCMasterFlags flags;
	};
	
	typedef void (*MSCMasterEnumerateNodesNodeCallback)
	(const char *nodeName, void *userdata);
	typedef void (*MSCMasterEnumerateNodesDomainCallback)
	(const char *nodeName, const char *versionName,
	 std::uint32_t numClients, std::uint32_t numRooms, void *userdata);
	
	extern MSCResult MSCMasterCreate(MSCLibrary library,
			MSCMasterParameters *params, MSCMaster *master);
	extern MSCResult MSCMasterDestroy(MSCMaster master);
	extern MSCResult MSCMasterAddVersion(MSCMaster master, const char *versionName);
	extern MSCResult MSCMasterRemoveVersion(MSCMaster master, const char *versionName);
	extern MSCResult MSCMasterSetVersionThrottle(MSCMaster master,
												 const char *versionName, double throttle);
	extern MSCResult MSCMasterSetNodeThrottle(MSCMaster master,
											  const char *nodeName, double throttle);
	extern MSCResult MSCMasterEnumerateNodes
	(MSCMaster master,
	 MSCMasterEnumerateNodesNodeCallback nodeCallback,
	 MSCMasterEnumerateNodesDomainCallback domainCallback,
	 void *userdata);
	
	typedef void *MSCNode;
	
	typedef std::uint32_t (*MSCLoadVersionCallback)
	(const char *versionName, void *userdata);
	
	struct MSCClientSetup
	{
		int readStream;
		int writeStream;
	};
	
	typedef std::uint32_t (*MSCAcceptClientCallback)
	(std::uint64_t clientId, const char *version, const char *room, std::int32_t roomNameLen, void *userdata);
	
	typedef std::uint32_t (*MSCSetupClientCallback)
	(std::uint64_t clientId, const MSCClientSetup *setup, void *userdata);
	
	typedef std::uint32_t (*MSCDiscardClientCallback)
	(std::uint64_t clientId, void *userdata);
	
	struct MSCNodeParameters
	{
		const char *nodeName;
		const char *masterEndpoint;
		
		MSCGetVersionPackagePathCallback packagePathCallback;
		void *packagePathCallbackUserData;
		
		MSCGetVersionPackagePathCallback packageDownloadPathCallback;
		void *packageDownloadPathCallbackUserData;
		
		MSCDeployPackageCallback deployPackageCallback;
		void *deployPackageCallbackUserData;
		
		MSCLoadVersionCallback loadVersionCallback;
		void *loadVersionCallbackUserData;
		
		MSCLoadVersionCallback unloadVersionCallback;
		void *unloadVersionCallbackUserData;
		
		MSCAcceptClientCallback acceptClientCallback;
		void *acceptClientCallbackUserData;
		
		MSCSetupClientCallback setupClientCallback;
		void *setupClientCallbackUserData;
		
		MSCDiscardClientCallback discardClientCallback;
		void *discardClientCallbackUserData;
	};
	
	extern MSCResult MSCNodeCreate(MSCLibrary library,
								   MSCNodeParameters *params, MSCNode *node);
	extern MSCResult MSCNodeDestroy(MSCNode node);
	extern MSCResult MSCNodeVersionLoaded(MSCNode node, const char *version);
	extern MSCResult MSCNodeVersionUnloaded(MSCNode node, const char *version);
	extern MSCResult MSCNodeBindRoom(MSCNode node, const char *room,
									 std::int32_t roomNameLen, const char *version);
	extern MSCResult MSCNodeUnbindRoom(MSCNode node, const char *room,
									 std::int32_t roomNameLen);
	
#ifdef __cplusplus
};
#endif
