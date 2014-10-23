/**
 * Copyright (C) 2014 yvt <i@yvt.jp>.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include "Logging.hpp"

namespace mcore
{
	class NodeVersionLoader;
	class NodeVersionManagerDomain;
	class NodeVersionManagerDomainInstance;
	
	class NodeVersionManager:
	boost::noncopyable,
	public std::enable_shared_from_this<NodeVersionManager>
	{
		friend class NodeVersionManagerDomain;
		
		TypedLogger<NodeVersionManager> log;
		std::recursive_mutex mutex;
		
		std::shared_ptr<NodeVersionLoader> const loader;
		
		std::unordered_map<std::string,
		std::shared_ptr<NodeVersionManagerDomain>> domains;
		
	public:
		// NodeVersionLoader must be provided with
		// its onNeedsDownloadFileName connected to the caller-provided function.
		NodeVersionManager(const std::shared_ptr<NodeVersionLoader>&);
		~NodeVersionManager();
		
		void requestLoadVersion(const std::string &);
		void requestUnloadVersion(const std::string &);
		
		// Called when a domain must be loaded.
		// When domain could be loaded, the caller should call
		// <NodeVersionManagerDomain::loaded>.
		// If the version needs to be downloaded,
		// <NodeVersionManagerDomain::needsDownload> should be
		// called.
		// <onStartDomain> and <onNeedToDeployVersion> will only be c
		// called from one thread.
		boost::signals2::signal<void(std::shared_ptr<NodeVersionManagerDomain>)> onStartDomain;
		
		// Called when a version was downloaded, and
		// needs to be deployed.
		// The handler can throw a exception, and if it does so,
		// <NodeVersionManager> will output an error and handles it
		// appropriately.
		// <onStartDomain> and <onNeedToDeployVersion> will only be c
		// called from one thread.
		boost::signals2::signal<void(const std::string&)> onNeedToDeployVersion;
	};
	
	class NodeVersionManagerDomain:
	boost::noncopyable,
	public std::enable_shared_from_this<NodeVersionManagerDomain>
	{
		friend class NodeVersionManager;
		
		TypedLogger<NodeVersionManagerDomain> log;
		
		std::string const _version;
		std::weak_ptr<NodeVersionManager> const manager;
		
		enum class State
		{
			// The domain is being loaded, or
			// it might be needs to be downloaded.
			Waiting,
			
			// The domain is being downloaded.
			Downloading,
			
			// The domain is loaded.
			Loaded,
			
			// The domain could not be loaded.
			Error,
			
			// The domain was unloaded.
			Unloaded
		};
		State state;
		
		std::shared_ptr<NodeVersionManagerDomainInstance> instance;
		
		std::shared_ptr<NodeVersionManager> managerOrError();
		
		void requestStartDomain();
		void unload();
		void downloadDone();
		void downloadFailed();
	public:
		NodeVersionManagerDomain(const std::shared_ptr<NodeVersionManager>&,
								 const std::string&);
		~NodeVersionManagerDomain();
		
		const std::string& version() const { return _version; }
		
		void loaded(const std::shared_ptr<NodeVersionManagerDomainInstance> &instance);
		void loadFailed();
		void needsDownload();
		
		boost::signals2::signal<void()> onUnloaded;
	};
	
	class NodeVersionManagerDomainInstance:
	boost::noncopyable,
	public std::enable_shared_from_this<NodeVersionManagerDomainInstance>
	{
	protected:
		NodeVersionManagerDomainInstance() { }
		virtual ~NodeVersionManagerDomainInstance() { }
	public:
		virtual void unload() = 0;
	};
}