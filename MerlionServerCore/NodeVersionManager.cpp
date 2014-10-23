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
#include "Prefix.pch"
#include "NodeVersionManager.hpp"
#include "Exceptions.hpp"
#include "NodeVersionLoader.hpp"

using boost::format;

namespace mcore
{
	
	NodeVersionManager::NodeVersionManager
	(const std::shared_ptr<NodeVersionLoader> &loader):
	loader(loader)
	{
		if (!loader) {
			MSCThrow(InvalidArgumentException("loader"));
		}
		
		loader->onVersionAboutToBeDownloaded.connect([=](const std::string &version, bool &cancel) {
			auto self = shared_from_this();
			std::lock_guard<std::recursive_mutex> lock(mutex);
			
			auto it = domains.find(version);
			
			// We don't need a version which we don't even know...
			if (it == domains.end()) {
				cancel = true;
				return;
			}
			
			auto domain = it->second;
			
			// Don't download a version until we want to have it downloaded
			if (domain->state != NodeVersionManagerDomain::State::Downloading) {
				cancel = true;
				return;
			}
		});
		loader->onVersionDownloaded.connect([=](const std::string &version) {
			auto self = shared_from_this();
			
			// Deploy
			try {
				onNeedToDeployVersion(version);
			} catch(...) {
				BOOST_LOG_SEV(log, LogLevel::Error) <<
				format("Failed to deploy version '%s'.: ") % version
				<< boost::current_exception_diagnostic_information();
				
				std::lock_guard<std::recursive_mutex> lock(mutex);
				auto it = domains.find(version);
				
				if (it != domains.end()) {
					auto domain = it->second;
					domain->downloadFailed();
				}
				
				return;
			}
			
			BOOST_LOG_SEV(log, LogLevel::Info) <<
			format("Deployed version '%s'.: ") % version;
			
			// Report that version was downloaded.
			std::lock_guard<std::recursive_mutex> lock(mutex);
			
			auto it = domains.find(version);
			
			if (it != domains.end()) {
				auto domain = it->second;
				domain->downloadDone();
			}
			
		});
		loader->onVersionDownloadFailed.connect([=](const std::string& version) {
			auto self = shared_from_this();
			
			std::lock_guard<std::recursive_mutex> lock(mutex);
			auto it = domains.find(version);
			
			if (it != domains.end()) {
				auto domain = it->second;
				domain->downloadFailed();
			}
		});
	}
	NodeVersionManager::~NodeVersionManager()
	{ }
	
	void NodeVersionManager::requestLoadVersion(const std::string &version)
	{
		auto self = shared_from_this();
		std::lock_guard<std::recursive_mutex> lock(mutex);
		
		try {
			if (domains.find(version) != domains.end()) {
				BOOST_LOG_SEV(log, LogLevel::Debug) <<
				format("Not loading version '%s' because it is already loaded (or being loaded).") % version;
				return;
			}
			
			BOOST_LOG_SEV(log, LogLevel::Info) <<
			format("Loading version '%s'.") % version;
			
			auto domain = std::make_shared<NodeVersionManagerDomain>
			(self, version);
			domains.emplace(version, domain);
			
			std::weak_ptr<NodeVersionManagerDomain> wDomain {domain};
			domain->onUnloaded.connect([wDomain] {
				auto domain = wDomain.lock();
				if (!domain) {
					return;
				}
				
				
				auto manager = domain->manager.lock();
				if (!manager) {
					return;
				}
				
				std::lock_guard<std::recursive_mutex> lock(manager->mutex);
				manager->domains.erase(domain->version());
			});
			
			domain->requestStartDomain();
			
		} catch (...) {
			BOOST_LOG_SEV(log, LogLevel::Error) <<
			format("Failed to load version '%s'.: ") % version <<
			boost::current_exception_diagnostic_information();
		}
	}
	void NodeVersionManager::requestUnloadVersion(const std::string &version)
	{
		std::lock_guard<std::recursive_mutex> lock(mutex);
		
		try {
			auto it = domains.find(version);
			if (it == domains.end()) {
				BOOST_LOG_SEV(log, LogLevel::Debug) <<
				format("Not unloading version '%s' because it is not loaded.") % version;
				return;
			}
			
			BOOST_LOG_SEV(log, LogLevel::Info) <<
			format("Unloading version '%s'.") % version;
			
			auto domain = it->second;
			domain->unload();
		} catch (...) {
			BOOST_LOG_SEV(log, LogLevel::Error) <<
			format("Failed to unload version '%s'.: ") % version <<
			boost::current_exception_diagnostic_information();
		}
	}
	
	
	NodeVersionManagerDomain::NodeVersionManagerDomain
	(const std::shared_ptr<NodeVersionManager> &manager,
	 const std::string &version):
	manager(manager),
	_version(version),
	state(State::Waiting)
	{
		log.setChannel("Version:" + version);
	}
	NodeVersionManagerDomain::~NodeVersionManagerDomain()
	{
		
	}
	
	std::shared_ptr<NodeVersionManager>
	NodeVersionManagerDomain::managerOrError()
	{
		auto locked = manager.lock();
		if (!locked) {
			MSCThrow(InvalidOperationException("NodeVersionManager is alredy disposed."));
		}
		return locked;
	}
	
	void NodeVersionManagerDomain::requestStartDomain()
	{
		auto self = shared_from_this();
		auto man = managerOrError();
		std::lock_guard<std::recursive_mutex> lock(man->mutex);
		
		if (state != State::Waiting) {
			MSCThrow(InvalidOperationException("requestStartDomain called with an invalid state."));
		}
		
		try {
			man->onStartDomain(self);
		} catch (...) {
			state = State::Error;
			
			BOOST_LOG_SEV(log, LogLevel::Error) <<
			"Failed to start domain.: " <<
			boost::current_exception_diagnostic_information();
			
			try {
				onUnloaded();
			} catch (...) {
				BOOST_LOG_SEV(log, LogLevel::Fatal) <<
				"Unhandlded exception occured while signaling onUnloaded.: " <<
				boost::current_exception_diagnostic_information();
			}
		}
		
	}
	
	void NodeVersionManagerDomain::loaded
	(const std::shared_ptr<NodeVersionManagerDomainInstance> &instance)
	{
		auto self = shared_from_this();
		auto man = managerOrError();
		std::lock_guard<std::recursive_mutex> lock(man->mutex);
		
		if (state != State::Waiting) {
			MSCThrow(InvalidOperationException("onStartDomain handled twice."));
		}
		
		if (!instance) {
			MSCThrow(InvalidArgumentException("instance"));
		}
		
		this->instance = instance;
		state = State::Loaded;
		
	}
	void NodeVersionManagerDomain::needsDownload()
	{
		auto self = shared_from_this();
		auto man = managerOrError();
		std::lock_guard<std::recursive_mutex> lock(man->mutex);
		
		if (state != State::Waiting) {
			MSCThrow(InvalidOperationException("onStartDomain handled twice."));
		}
		
		state = State::Downloading;
		
		auto loader = man->loader;
		loader->download(version());
	}
	
	void NodeVersionManagerDomain::downloadDone()
	{
		auto self = shared_from_this();
		auto man = managerOrError();
		std::lock_guard<std::recursive_mutex> lock(man->mutex);
		
		if (state != State::Downloading) {
			MSCThrow(InvalidOperationException("downloadDone called with invalid state."));
		}
		
		state = State::Waiting;
		requestStartDomain();
	}
	
	void NodeVersionManagerDomain::downloadFailed()
	{
		auto self = shared_from_this();
		auto man = managerOrError();
		std::lock_guard<std::recursive_mutex> lock(man->mutex);
		
		if (state != State::Downloading) {
			MSCThrow(InvalidOperationException("downloadFailed called with invalid state."));
		}
		
		unload();
	}
	
	void NodeVersionManagerDomain::loadFailed()
	{
		auto self = shared_from_this();
		auto man = managerOrError();
		std::lock_guard<std::recursive_mutex> lock(man->mutex);
		
		if (state != State::Waiting) {
			MSCThrow(InvalidOperationException("loadFailed called with invalid state."));
		}
		
		unload();
	}
	
	void NodeVersionManagerDomain::unload()
	{
		auto self = shared_from_this();
		auto man = managerOrError();
		std::lock_guard<std::recursive_mutex> lock(man->mutex);
		
		switch (state) {
			case State::Waiting:
				// When <loaded> or <needsDownload> is called,
				// unload request will be processed...
				break;
			case State::Downloading:
				// Don't care
				break;
			case State::Loaded:
				try {
					instance->unload();
				} catch (...) {
					BOOST_LOG_SEV(log, LogLevel::Fatal) <<
					"Failed to unload domain.: " <<
					boost::current_exception_diagnostic_information();
				}
				break;
			case State::Error:
			case State::Unloaded:
				// Already unloaded...
				return;
		}
		
		state = State::Unloaded;
		
		try {
			onUnloaded();
		} catch (...) {
			BOOST_LOG_SEV(log, LogLevel::Fatal) <<
			"Unhandlded exception occured while signaling onUnloaded.: " <<
			boost::current_exception_diagnostic_information();
		}
	}
}
