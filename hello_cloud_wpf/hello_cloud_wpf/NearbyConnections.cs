using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace hello_cloud_wpf
{
    public static class NearbyConnections
    {
        const string nearby_connections_dll = "nearby_connections_dart.dll";

        [StructLayout(LayoutKind.Sequential)]
        public struct Strategy
        {
            public enum ConnectionType
            {
                kNone = 0,
                kPointToPoint = 1,
            };

            public enum TopologyType
            {
                kUnknown = 0,
                kOneToOne = 1,
                kOneToMany = 2,
                kManyToMany = 3,
            };

            public ConnectionType connection_type;
            public TopologyType topology_type;
        };

        public static Strategy kP2pCluster = new()
        {
            connection_type = Strategy.ConnectionType.kPointToPoint,
            topology_type = Strategy.TopologyType.kManyToMany
        };

        [StructLayout(LayoutKind.Sequential)]
        public struct BooleanMediumSelector
        {
            [MarshalAs(UnmanagedType.U1)] public bool bluetooth;
            [MarshalAs(UnmanagedType.U1)] public bool ble;
            [MarshalAs(UnmanagedType.U1)] public bool web_rtc;
            [MarshalAs(UnmanagedType.U1)] public bool wifi_lan;
            [MarshalAs(UnmanagedType.U1)] public bool wifi_hotspot;
            [MarshalAs(UnmanagedType.U1)] public bool wifi_direct;
        };

        [StructLayout(LayoutKind.Explicit)]
        public struct DiscoveryOptions
        {
            [FieldOffset(0)] public Strategy strategy;
            [FieldOffset(8)] public BooleanMediumSelector allowed;
            [FieldOffset(16)][MarshalAs(UnmanagedType.U1)] public bool auto_upgrade_bandwidth;
            [FieldOffset(17)][MarshalAs(UnmanagedType.U1)] public bool enforce_topology_constraints;
            [FieldOffset(18)][MarshalAs(UnmanagedType.U1)] public bool is_out_of_band_connection;
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 32)]
            [FieldOffset(24)] public char[] unused; // std::string fast_advertisement_service_uuid;
            [FieldOffset(56)][MarshalAs(UnmanagedType.U1)] public bool low_power;
        };

        public enum Status
        {
            kSuccess,
            kError,
            kOutOfOrderApiCall,
            kAlreadyHaveActiveStrategy,
            kAlreadyAdvertising,
            kAlreadyDiscovering,
            kAlreadyListening,
            kEndpointIoError,
            kEndpointUnknown,
            kConnectionRejected,
            kAlreadyConnectedToEndpoint,
            kNotConnectedToEndpoint,
            kBluetoothError,
            kBleError,
            kWifiLanError,
            kPayloadUnknown,
            kReset,
            kTimeout,
            kUnknown,
            kNextValue,
        };

        public enum DistanceInfo
        {
            kUnknown = 1,
            kVeryClose = 2,
            kClose = 3,
            kFar = 4,
        };

        public enum Medium
        {
            kUnknown = 0,
            kMdns = 1,
            kBluetooth = 2,
            kWiFiHotSpot = 3,
            kBle = 4,
            kWiFiLan = 5,
            kWifiAware = 6,
            kNfc = 7,
            kWiFiDirect = 8,
            kWebRtc = 9,
            kBleL2Cap = 10,
            kUsb = 11,
        };

        [StructLayout(LayoutKind.Explicit)]
        public struct AdvertisingOptions
        {
            [FieldOffset(0)] public Strategy strategy;
            [FieldOffset(8)] public BooleanMediumSelector allowed;
            [FieldOffset(16)][MarshalAs(UnmanagedType.U1)] public bool auto_upgrade_bandwidth;
            [FieldOffset(17)][MarshalAs(UnmanagedType.U1)] public bool enforce_topology_constraints;
            [FieldOffset(18)][MarshalAs(UnmanagedType.U1)] public bool low_power;
            [FieldOffset(19)][MarshalAs(UnmanagedType.U1)] public bool enable_bluetooth_listening;
            [FieldOffset(20)][MarshalAs(UnmanagedType.U1)] public bool enable_webrtc_listening;
            [FieldOffset(21)][MarshalAs(UnmanagedType.U1)] public bool is_out_of_band_connection;
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 32)]
            [FieldOffset(24)] public char[] unused; // std::string fast_advertisement_service_uuid;
            [FieldOffset(56)] string device_info;
        };

        // TODO: use proto
        public struct ConnectionResponseInfo
        {
            public byte[] endpointInfo;
            public string authenticationToken; // always 4 characters
            public string rawAuthenticationToken;
            public bool isIncomingConnection;
            public bool isConnectionVerified;
        };

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        public delegate void ResultCallback(Status status);

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        public delegate void EndpointFoundCallback(string endpointId,
            [MarshalAs(UnmanagedType.LPArray, SizeParamIndex = 2)] byte[] endpointInfo, int size, string serviceId);

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        public delegate void EndpointLostCallback(string endpointId);

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        public delegate void EndpointDistanceChangedCallback(string endpointId, DistanceInfo distanceInfo);

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        public delegate void InitiatedCallback(string endpointId, ConnectionResponseInfo connectionResponseInfo);

        //DLL_API ServiceControllerRouter* __stdcall InitServiceControllerRouter();
        [DllImport(nearby_connections_dll)]
        public static extern IntPtr InitServiceControllerRouter();

        //DLL_API void __stdcall CloseServiceControllerRouter(ServiceControllerRouter*);
        [DllImport(nearby_connections_dll)]
        public static extern IntPtr CloseServiceControllerRouter(IntPtr router);

        //DLL_API Core* __stdcall InitCore(ServiceControllerRouter*);
        [DllImport(nearby_connections_dll)]
        public static extern IntPtr InitCore(IntPtr serviceControllerRouter);

        //DLL_API void __stdcall CloseCore(Core*);
        [DllImport(nearby_connections_dll)]
        public static extern void CloseCore(IntPtr core);

        [DllImport(nearby_connections_dll, EntryPoint = "StartDiscoverySharp")]
        public static extern void StartDiscovery(
            IntPtr core,
            [MarshalAs(UnmanagedType.LPStr)] string service_id,
            DiscoveryOptions discovery_options,
            EndpointFoundCallback endpoint_found_callback,
            EndpointLostCallback endpoint_lost_callback,
            EndpointDistanceChangedCallback endpoint_distance_changed_callback,
            ResultCallback start_discovery_callback);

        [DllImport(nearby_connections_dll, CharSet = CharSet.Unicode, EntryPoint = "StartAdvertisingSharp")]
        public static extern void StartAdvertising(
            IntPtr pCore,
            [MarshalAs(UnmanagedType.LPStr)] string service_id,
            AdvertisingOptions advertising_options,
            byte[] endpoint_info,
            InitiatedCallback initiated_callback,
            ResultCallback start_advertising_callback);
    }
}
