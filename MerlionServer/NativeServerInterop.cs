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

using System;
using System.Runtime.InteropServices;
using System.Collections.Generic;
using log4net;

namespace Merlion.Server
{
	unsafe static class NativeServerInterop
	{

		enum MSCResult : int
		{
			MSCR_Success = 0,
			MSCR_Unknown,
			MSCR_InvalidArgument,
			MSCR_InvalidOperation,
			MSCR_InvalidFormat,
			MSCR_InvalidData,
			MSCR_SystemError,
			MSCR_NotImplemented
		}

		enum MSCLogSeverity : int
		{
			MSCLS_Debug = -1,
			MSCLS_Info = 0,
			MSCLS_Warn,
			MSCLS_Error,
			MSCLS_Fatal
		}

		delegate void MSCLogSink(MSCLogSeverity severity, string source, string host, 
			string channel, string message, IntPtr userdata);

		[DllImport("MerlionServerCore")]
		static extern MSCResult MSCAddLogSink([MarshalAs(UnmanagedType.FunctionPtr)]MSCLogSink sink, IntPtr userdata, out MSCLogSinkSafeHandle outHandle);

		[DllImport("MerlionServerCore")]
		static extern MSCResult MSCRemoveLogSink(MSCLogSinkSafeHandle handle);

		[DllImport("MerlionServerCore")]
		static extern IntPtr MSCGetLastErrorMessage();

		[Flags]
		enum MSCMasterFlags : uint
		{
			MSCMF_None = 0,
			MSCMF_DisallowVersionSpecification = 1 << 0
		}

		struct MSCMasterParameters
		{
			[MarshalAs(UnmanagedType.LPStr)]
			public string nodeEndpoint;

			[MarshalAs(UnmanagedType.LPStr)]
			public string clientEndpoint;

			[MarshalAs(UnmanagedType.LPStr)]
			public string sslCertificateFile;

			[MarshalAs(UnmanagedType.LPStr)]
			public string sslPrivateKeyFile;

			[MarshalAs(UnmanagedType.LPStr)]
			public string sslPassword;

			[MarshalAs(UnmanagedType.FunctionPtr)]
			public MSCGetVersionPackagePathCallback packagePathCallback;
			public IntPtr packagePathCallbackUserData;

			public MSCMasterFlags flags;
		}

		delegate uint MSCGetVersionPackagePathCallback 
		(string versionName, byte *outPackagePath, int bufferSize, IntPtr userdata);

		delegate uint MSCDeployPackageCallback
		(string versionName, IntPtr userdata);

		[DllImport("MerlionServerCore")]
		static extern MSCResult MSCLibraryCreate(out MSCLibrarySafeHandle libOut);

		[DllImport("MerlionServerCore")]
		static extern MSCResult MSCLibraryDestroy(MSCLibrarySafeHandle library);

		[DllImport("MerlionServerCore")]
		static extern MSCResult MSCMasterCreate(
			MSCLibrarySafeHandle library,
			[MarshalAs(UnmanagedType.LPStruct)] MSCMasterParameters param,
			out MSCMasterSafeHandle master);

		[DllImport("MerlionServerCore")]
		static extern MSCResult MSCMasterDestroy(MSCMasterSafeHandle master);

		[DllImport("MerlionServerCore")]
		static extern MSCResult MSCMasterAddVersion( 
			MSCMasterSafeHandle master, [MarshalAs(UnmanagedType.LPStr)] string version);
		[DllImport("MerlionServerCore")]
		static extern MSCResult MSCMasterRemoveVersion( 
			MSCMasterSafeHandle master, [MarshalAs(UnmanagedType.LPStr)] string version);
		[DllImport("MerlionServerCore")]
		static extern MSCResult MSCMasterSetVersionThrottle( 
			MSCMasterSafeHandle master, [MarshalAs(UnmanagedType.LPStr)] string version,
			double throttle);
		[DllImport("MerlionServerCore")]
		static extern MSCResult MSCMasterSetNodeThrottle( 
			MSCMasterSafeHandle master, [MarshalAs(UnmanagedType.LPStr)] string nodeName,
			double throttle);

		delegate void MSCMasterEnumerateNodesNodeCallback
		(string nodeName, IntPtr userdata);
		delegate void MSCMasterEnumerateNodesDomainCallback
		(string nodeName, string versionName, int numClients, int numRooms, IntPtr userdata);

		[DllImport("MerlionServerCore")]
		static extern MSCResult MSCMasterEnumerateNodes( 
			MSCMasterSafeHandle master,
			[MarshalAs(UnmanagedType.FunctionPtr)] MSCMasterEnumerateNodesNodeCallback nodeCallback,
			[MarshalAs(UnmanagedType.FunctionPtr)] MSCMasterEnumerateNodesDomainCallback domainCallback,
			IntPtr userdata);

		delegate uint MSCLoadVersionCallback
		(string versionName, IntPtr userdata);

		struct MSCClientSetup
		{
			public int readStream;
			public int writeStream;
		}

		delegate uint MSCAcceptClientCallback(
			ulong clientId,
			[MarshalAs(UnmanagedType.LPStr)] string version,
			IntPtr room,
			int roomNameLen,
			IntPtr userdata);

		delegate uint MSCSetupClientCallback(
			ulong clientId,
			ref MSCClientSetup setup,
			IntPtr userdata);

		delegate uint MSCDiscardClientCallback(
			ulong clientId,
			IntPtr userdata);

		struct MSCNodeParameters
		{
			[MarshalAs(UnmanagedType.LPStr)]
			public string nodeName;

			[MarshalAs(UnmanagedType.LPStr)]
			public string masterEndpoint;

			[MarshalAs(UnmanagedType.FunctionPtr)]
			public MSCGetVersionPackagePathCallback packagePathCallback;

			public IntPtr packagePathCallbackUserData;

			[MarshalAs(UnmanagedType.FunctionPtr)]
			public MSCGetVersionPackagePathCallback packageDownloadPathCallback;

			public IntPtr packageDownloadPathCallbackUserData;

			[MarshalAs(UnmanagedType.FunctionPtr)]
			public MSCDeployPackageCallback deployPackageCallback;

			public IntPtr deployPackageCallbackUserData;

			[MarshalAs(UnmanagedType.FunctionPtr)]
			public MSCLoadVersionCallback loadVersionCallback;

			public IntPtr loadVersionCallbackUserData;

			[MarshalAs(UnmanagedType.FunctionPtr)]
			public MSCLoadVersionCallback unloadVersionCallback;

			public IntPtr unloadVersionCallbackUserData;

			[MarshalAs(UnmanagedType.FunctionPtr)]
			public MSCAcceptClientCallback acceptClientCallback;

			public IntPtr acceptClientCallbackUserData;

			[MarshalAs(UnmanagedType.FunctionPtr)]
			public MSCSetupClientCallback setupClientCallback;

			public IntPtr setupClientCallbackUserData;

			[MarshalAs(UnmanagedType.FunctionPtr)]
			public MSCDiscardClientCallback discardClientCallback;

			public IntPtr discardClientCallbackUserData;
		}

		[DllImport("MerlionServerCore")]
		static extern MSCResult MSCNodeCreate(
			MSCLibrarySafeHandle library,
			[MarshalAs(UnmanagedType.LPStruct)] MSCNodeParameters param,
			out MSCNodeSafeHandle master);

		[DllImport("MerlionServerCore")]
		static extern MSCResult MSCNodeDestroy(MSCNodeSafeHandle master);

		[DllImport("MerlionServerCore")]
		static extern MSCResult MSCNodeVersionLoaded(
			MSCNodeSafeHandle node, [MarshalAs(UnmanagedType.LPStr)] string version);
		[DllImport("MerlionServerCore")]
		static extern MSCResult MSCNodeVersionUnloaded(
			MSCNodeSafeHandle node, [MarshalAs(UnmanagedType.LPStr)] string version);

		[DllImport("MerlionServerCore")]
		static extern MSCResult MSCNodeBindRoom (
			MSCNodeSafeHandle node, IntPtr room, int roomNameLen, 
			[MarshalAs (UnmanagedType.LPStr)] string version);
		[DllImport("MerlionServerCore")]
		static extern MSCResult MSCNodeUnbindRoom (
			MSCNodeSafeHandle node, IntPtr room, int roomNameLen);

		static unsafe string GetLastError()
		{
			IntPtr msg = MSCGetLastErrorMessage ();
			if (msg == IntPtr.Zero) {
				throw new InvalidOperationException ("MSCGetLastErrorMessage returned nullptr.");
			}
			return Marshal.PtrToStringAnsi(msg);
		}

		static void CheckResult(MSCResult result)
		{
			switch (result) {
			case MSCResult.MSCR_Success:
				break;
			case MSCResult.MSCR_Unknown:
				throw new InvalidOperationException (GetLastError());
			case MSCResult.MSCR_InvalidArgument:
				throw new ArgumentException (GetLastError(), "(unknown)");
			case MSCResult.MSCR_InvalidOperation:
				throw new InvalidOperationException (GetLastError());
			case MSCResult.MSCR_InvalidFormat:
				throw new FormatException (GetLastError());
			case MSCResult.MSCR_InvalidData:
				throw new System.IO.InvalidDataException (GetLastError());
			case MSCResult.MSCR_SystemError:
				throw new SystemException (GetLastError());
			case MSCResult.MSCR_NotImplemented:
				throw new NotImplementedException (GetLastError());
			default:
				throw new InvalidOperationException (GetLastError());
			}
		}

		public sealed class MSCLogSinkSafeHandle : Microsoft.Win32.SafeHandles.SafeHandleZeroOrMinusOneIsInvalid
		{
			public MSCLogSinkSafeHandle():
			this(true) { }
			public MSCLogSinkSafeHandle(bool ownsHandle):
			base(ownsHandle) { }
			public MSCLogSinkSafeHandle(IntPtr preexistingHandle, bool ownsHandle):
			base(ownsHandle) { base.SetHandle(preexistingHandle); }
			protected override bool ReleaseHandle ()
			{
				try {
					CheckResult (MSCRemoveLogSink (this));
				} catch (Exception) {  return false; }
				return true;
			}
		}

		public sealed class MSCLibrarySafeHandle : Microsoft.Win32.SafeHandles.SafeHandleZeroOrMinusOneIsInvalid
		{
			public MSCLibrarySafeHandle():
			this(true) { }
			public MSCLibrarySafeHandle(bool ownsHandle):
			base(ownsHandle) { }
			public MSCLibrarySafeHandle(IntPtr preexistingHandle, bool ownsHandle):
			base(ownsHandle) { base.SetHandle(preexistingHandle); }
			protected override bool ReleaseHandle ()
			{
				try {
					CheckResult (MSCLibraryDestroy (this));
				} catch (Exception) {  return false; }
				return true;
			}
		}

		public sealed class MSCMasterSafeHandle : Microsoft.Win32.SafeHandles.SafeHandleZeroOrMinusOneIsInvalid
		{
			public MSCMasterSafeHandle():
			this(true) { }
			public MSCMasterSafeHandle(bool ownsHandle):
			base(ownsHandle) { }
			public MSCMasterSafeHandle(IntPtr preexistingHandle, bool ownsHandle):
			base(ownsHandle) { base.SetHandle(preexistingHandle); }
			protected override bool ReleaseHandle ()
			{
				try {
					CheckResult (MSCMasterDestroy (this));
				} catch (Exception) {  return false; }
				return true;
			}
		}

		public sealed class MSCNodeSafeHandle : Microsoft.Win32.SafeHandles.SafeHandleZeroOrMinusOneIsInvalid
		{
			public MSCNodeSafeHandle():
			this(true) { }
			public MSCNodeSafeHandle(bool ownsHandle):
			base(ownsHandle) { }
			public MSCNodeSafeHandle(IntPtr preexistingHandle, bool ownsHandle):
			base(ownsHandle) { base.SetHandle(preexistingHandle); }
			protected override bool ReleaseHandle ()
			{
				try {
					CheckResult (MSCNodeDestroy (this));
				} catch (Exception) {  return false; }
				return true;
			}
		}


		public static class Logging
		{
			static MSCLogSink sink;
			static MSCLogSinkSafeHandle handle;
			static readonly ILog log = LogManager.GetLogger(typeof(Logging));

			public static void RegisterLogger()
			{
				sink = HandleMSCLogSink;
				MSCAddLogSink (sink, IntPtr.Zero, out handle);
			}

			static void HandleMSCLogSink (MSCLogSeverity severity, 
				[MarshalAs(UnmanagedType.LPStr)] string source, 
				[MarshalAs(UnmanagedType.LPStr)] string host, 
				[MarshalAs(UnmanagedType.LPStr)] string channel, 
				[MarshalAs(UnmanagedType.LPStr)] string message, IntPtr userdata)
			{
				try {
					var data = new log4net.Core.LoggingEventData ();
					switch (severity) {
					case MSCLogSeverity.MSCLS_Debug:
						data.Level = log4net.Core.Level.Debug;
						break;
					case MSCLogSeverity.MSCLS_Info:
						data.Level = log4net.Core.Level.Info;
						break;
					case MSCLogSeverity.MSCLS_Warn:
						data.Level = log4net.Core.Level.Warn;
						break;
					case MSCLogSeverity.MSCLS_Error:
						data.Level = log4net.Core.Level.Error;
						break;
					case MSCLogSeverity.MSCLS_Fatal:
						data.Level = log4net.Core.Level.Fatal;
						break;
					default:
						data.Level = log4net.Core.Level.Fatal;
						log.ErrorFormat ("Unknown severity level {0} specified. Defaults to Fatal.", severity);
						break;
					}
					data.LoggerName = source;
					data.Properties = new log4net.Util.PropertiesDictionary ();
					data.Properties ["Node"] = host ?? "Local";
					data.ThreadName = System.Threading.Thread.CurrentThread.Name;
					data.TimeStamp = DateTime.Now;
					data.Message = message;
					data.Properties ["NDC"] = channel ?? "None";
					log.Logger.Log (new log4net.Core.LoggingEvent(data));
				} catch (Exception ex) {
					log.Error ("Error while forwarding log from native core.", ex);
				}
			}
		}

		public sealed class MasterParameters
		{
			public delegate string PackagePathDelegate(string versionName);
			public delegate void DeployPackageDelegate (string versionName);

			public bool DisallowVersionSpecification;
			public string NodeEndpoint;
			public string ClientEndpoint;
			public string SslCertificateFile;
			public string SslPrivateKeyFile;
			public string SslPassword;
			public PackagePathDelegate PackagePathProvider;
		}

		public sealed class Library: IDisposable
		{
			readonly MSCLibrarySafeHandle handle;

			public static Library Instance = new Library();

			Library()
			{
				CheckResult(MSCLibraryCreate(out handle));
			}

			public void Dispose()
			{
				handle.Dispose ();
			}

			public MSCLibrarySafeHandle SafeHandle
			{
				get { return handle; }
			}
		}

		public sealed class Master: IDisposable
		{
			readonly MSCMasterSafeHandle handle;
			readonly MasterParameters param;
			readonly MSCMasterParameters paramMarshaled;

			Master(Library library, MasterParameters param)
			{
				this.param = param;
				paramMarshaled = new MSCMasterParameters() {
					nodeEndpoint = param.NodeEndpoint,
					clientEndpoint = param.ClientEndpoint,
					sslCertificateFile = param.SslCertificateFile,
					sslPrivateKeyFile = param.SslPrivateKeyFile,
					sslPassword = param.SslPassword,
					packagePathCallback = HandleMSCGetVersionPackagePathCallback,
					flags = MSCMasterFlags.MSCMF_None
				};
				if (param.DisallowVersionSpecification) {
					paramMarshaled.flags |= MSCMasterFlags.MSCMF_DisallowVersionSpecification;
				}
				CheckResult(MSCMasterCreate(library.SafeHandle, paramMarshaled, out handle));
			}

			public Master(MasterParameters param):
			this(Library.Instance, param) { }



			uint HandleMSCGetVersionPackagePathCallback (
				[MarshalAs(UnmanagedType.LPStr)]string versionName, 
				byte* outPackagePath, int bufferSize, IntPtr userdata)
			{
				try {
					string path = param.PackagePathProvider(versionName);
					byte[] bytes = System.Text.Encoding.UTF8.GetBytes(path);
					if (checked(bytes.Length + 1) > bufferSize) {
						return 1;
					}
					Marshal.Copy(bytes, 0, new IntPtr(outPackagePath), bytes.Length);
					outPackagePath[bytes.Length] = 0; // null terminated string
					return 0;
				} catch {
					return 1;
				}
			}

			public void AddVersion(string name)
			{
				MSCMasterAddVersion (handle, name);
			}

			public void RemoveVersion(string name)
			{
				MSCMasterRemoveVersion (handle, name);
			}

			public void SetVersionThrottle(string name, double throttle)
			{
				MSCMasterSetVersionThrottle (handle, name, throttle);
			}

			public void SetNodeThrottle(string name, double throttle)
			{
				MSCMasterSetNodeThrottle (handle, name, throttle);
			}

			public sealed class DomainInfo
			{
				public string VersionName;
				public int ClientCount;
				public int RoomCount;
			}

			public sealed class NodeInfo
			{
				public string NodeName;
				public DomainInfo[] Domains;
			}

			readonly List<NodeInfo> nodes = new List<NodeInfo>();
			readonly List<DomainInfo> domains = new List<DomainInfo>();

			public NodeInfo[] EnumerateNodes()
			{
				nodes.Clear ();
				domains.Clear ();
				MSCMasterEnumerateNodes (handle, 
					HandleMSCMasterEnumerateNodesNodeCallback,
					HandleMSCMasterEnumerateNodesDomainCallback, IntPtr.Zero);

				FillDomains ();
				return nodes.ToArray();
			}

			void FillDomains()
			{
				if (nodes.Count > 0) {
					nodes [nodes.Count - 1].Domains = domains.ToArray ();
				}
				domains.Clear ();
			}

			void HandleMSCMasterEnumerateNodesDomainCallback (string nodeName, string versionName, int numClients, int numRooms, IntPtr userdata)
			{
				domains.Add (new DomainInfo () {
					VersionName = versionName,
					ClientCount = numClients,
					RoomCount = numRooms
				});
			}

			void HandleMSCMasterEnumerateNodesNodeCallback (string nodeName, IntPtr userdata)
			{
				FillDomains ();

				nodes.Add (new NodeInfo () {
					NodeName = nodeName
				});
			}

			public void Dispose()
			{
				handle.Dispose ();
			}

			public MSCMasterSafeHandle SafeHandle
			{
				get { return handle; }
			}
		}

		public sealed class ClientSetup
		{
			public int WriteFileDescriptor;
			public int ReadFileDescriptor;
		}

		public sealed class NodeParameters
		{
			public delegate string PackagePathDelegate(string versionName);
			public delegate void DeployPackageDelegate (string versionName);
			public delegate void LoadVersionDelegate(string versionName);
			public delegate void AcceptClientDelegate(ulong clientId, string version, byte[] room);
			public delegate void SetupClientDelegate(ulong clientId, ClientSetup setup);
			public delegate void DiscardClientDelegate(ulong clientId);

			public string NodeName;
			public string MasterEndpoint;
			public PackagePathDelegate PackagePathProvider;
			public PackagePathDelegate PackageDownloadPathProvider;
			public DeployPackageDelegate DeployPackageMethod;
			public LoadVersionDelegate VersionLoader;
			public LoadVersionDelegate VersionUnloader;
			public AcceptClientDelegate AcceptClientMethod;
			public SetupClientDelegate SetupClientMethod;
			public DiscardClientDelegate DiscardClientMethod;
		}
		public sealed class Node: IDisposable
		{
			readonly MSCNodeSafeHandle handle;
			readonly NodeParameters param;
			readonly MSCNodeParameters paramMarshaled;

			Node(Library library, NodeParameters param)
			{
				this.param = param;
				paramMarshaled = new MSCNodeParameters() {
					nodeName = param.NodeName,
					masterEndpoint = param.MasterEndpoint,
					packagePathCallback = HandleMSCGetVersionPackagePathCallback,
					packageDownloadPathCallback = HandleMSCGetVersionPackageDownloadPathCallback,
					deployPackageCallback = HandleMSCDeployPackageCallback,
					loadVersionCallback = HandleMSCLoadVersionCallback,
					unloadVersionCallback = HandleMSCUnloadVersionCallback,
					acceptClientCallback = HandleMSCAcceptClientCallback,
					discardClientCallback = HandleMSCDiscardClientCallback,
					setupClientCallback = HandleMSCSetupClientCallback
				};
				CheckResult(MSCNodeCreate(library.SafeHandle, paramMarshaled, out handle));
			}

			uint HandleMSCSetupClientCallback (ulong clientId, [MarshalAs(UnmanagedType.LPStruct)] ref MSCClientSetup setup, IntPtr userdata)
			{
				try {
					param.SetupClientMethod(clientId, new ClientSetup() {
						WriteFileDescriptor = setup.writeStream,
						ReadFileDescriptor = setup.readStream
					});
					return 0;
				} catch {
					return 1;
				}
			}

			uint HandleMSCDiscardClientCallback (ulong clientId, IntPtr userdata)
			{
				try {
					param.DiscardClientMethod(clientId);
					return 0;
				} catch {
					return 1;
				}
			}

			uint HandleMSCAcceptClientCallback (ulong clientId, string version, IntPtr room, int roomNameLen, IntPtr userdata)
			{
				try {
					byte[] roomName = new byte[roomNameLen];
					Marshal.Copy(room, roomName, 0, roomNameLen);
					param.AcceptClientMethod(clientId, version, roomName);
					return 0;
				} catch {
					return 1;
				}
			}

			uint HandleMSCUnloadVersionCallback (string versionName, IntPtr userdata)
			{
				try {
					param.VersionLoader(versionName);
					return 0;
				} catch {
					return 1;
				}
			}

			uint HandleMSCLoadVersionCallback (string versionName, IntPtr userdata)
			{
				try {
					param.VersionLoader(versionName);
					return 0;
				} catch {
					return 1;
				}
			}

			public Node(NodeParameters param):
			this(Library.Instance, param) { }

			uint HandleMSCDeployPackageCallback (
				[MarshalAs(UnmanagedType.LPStr)]string versionName, IntPtr userdata)
			{
				try {
					param.DeployPackageMethod(versionName);
					return 0;
				} catch {
					return 1;
				}
			}

			uint HandleMSCGetVersionPackageDownloadPathCallback (
				[MarshalAs(UnmanagedType.LPStr)]string versionName, 
				byte* outPackagePath, int bufferSize, IntPtr userdata)
			{
				try {
					string path = param.PackageDownloadPathProvider(versionName);
					byte[] bytes = System.Text.Encoding.UTF8.GetBytes(path);
					if (checked(bytes.Length + 1) > bufferSize) {
						return 1;
					}
					Marshal.Copy(bytes, 0, new IntPtr(outPackagePath), bytes.Length);
					outPackagePath[bytes.Length] = 0; // null terminated string
					return 0;
				} catch {
					return 1;
				}
			}

			uint HandleMSCGetVersionPackagePathCallback (
				[MarshalAs(UnmanagedType.LPStr)]string versionName, 
				byte* outPackagePath, int bufferSize, IntPtr userdata)
			{
				try {
					string path = param.PackagePathProvider(versionName);
					byte[] bytes = System.Text.Encoding.UTF8.GetBytes(path);
					if (checked(bytes.Length + 1) > bufferSize) {
						return 1;
					}
					Marshal.Copy(bytes, 0, new IntPtr(outPackagePath), bytes.Length);
					outPackagePath[bytes.Length] = 0; // null terminated string
					return 0;
				} catch {
					return 1;
				}
			}

			public void VersionLoaded(string ver)
			{
				MSCNodeVersionLoaded (handle, ver);
			}

			public void VersionUnloaded(string ver)
			{
				MSCNodeVersionUnloaded (handle, ver);
			}

			public unsafe void BindRoom(byte[] room, string version)
			{
				fixed (byte *roomPtr = room) {
					MSCNodeBindRoom (handle, new IntPtr (roomPtr), room.Length, version);
				}
			}

			public unsafe void UnbindRoom(byte[] room)
			{
				fixed (byte *roomPtr = room) {
					MSCNodeUnbindRoom (handle, new IntPtr (roomPtr), room.Length);
				}
			}

			public void Dispose()
			{
				handle.Dispose ();
			}

			public MSCNodeSafeHandle SafeHandle
			{
				get { return handle; }
			}
		}

	}
}

