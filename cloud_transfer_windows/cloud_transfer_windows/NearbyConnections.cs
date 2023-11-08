using System;
using System.Runtime.InteropServices;

namespace cloud_transfer_windows
{
    internal static class NearbyConnections
    {
        //DLL_API ServiceControllerRouter* __stdcall InitServiceControllerRouter();
        [DllImport("nearby_connections_dart.dll")]
        public static extern IntPtr InitServiceControllerRouter();

        //DLL_API void __stdcall CloseServiceControllerRouter(ServiceControllerRouter*);
        [DllImport("nearby_connections_dart.dll")]
        public static extern IntPtr CloseServiceControllerRouter(IntPtr router);

        //DLL_API Core* __stdcall InitCore(ServiceControllerRouter*);
        [DllImport("nearby_connections_dart.dll")]
        public static extern IntPtr InitCore(IntPtr serviceControllerRouter);

        // Closes the core with stopping all endpoints, then free the memory.
        //DLL_API void __stdcall CloseCore(Core*);
        [DllImport("nearby_connections_dart.dll")]
        public static extern void CloseCore(IntPtr core);
    }
}
