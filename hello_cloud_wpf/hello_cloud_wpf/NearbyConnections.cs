using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace hello_cloud_wpf
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
