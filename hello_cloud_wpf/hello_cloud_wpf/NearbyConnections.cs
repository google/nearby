using System;
using System.Runtime.InteropServices;

namespace HelloCloudWpf {
    public static class NearbyConnections {
        const string NearbyConnectionsDll = "nearby_connections_dart.dll";

        // TODO: Use protobuf for all these structs on both sides
        // TODO: Use pure C types for all other fields on the SDK side
        // I.e. remove std::string and use char* instead.
        [StructLayout(LayoutKind.Sequential)]
        public struct Strategy {
            public enum ConnectionType {
                kNone = 0,
                kPointToPoint = 1,
            };

            public enum TopologyType {
                kUnknown = 0,
                kOneToOne = 1,
                kOneToMany = 2,
                kManyToMany = 3,
            };

            public ConnectionType connectionType;
            public TopologyType topologyType;
        };

        public static Strategy P2pCluster = new() {
            connectionType = Strategy.ConnectionType.kPointToPoint,
            topologyType = Strategy.TopologyType.kManyToMany
        };

        [StructLayout(LayoutKind.Sequential)]
        public struct BooleanMediumSelector {
            [MarshalAs(UnmanagedType.U1)] public bool bluetooth;
            [MarshalAs(UnmanagedType.U1)] public bool ble;
            [MarshalAs(UnmanagedType.U1)] public bool webRtc;
            [MarshalAs(UnmanagedType.U1)] public bool wifiLan;
            [MarshalAs(UnmanagedType.U1)] public bool wifiHotspot;
            [MarshalAs(UnmanagedType.U1)] public bool wifiDirect;
        };

        [StructLayout(LayoutKind.Explicit)]
        public struct DiscoveryOptions {
            [FieldOffset(0)] public Strategy strategy;
            [FieldOffset(8)] public BooleanMediumSelector allowed;
            [FieldOffset(16)][MarshalAs(UnmanagedType.U1)] public bool autoUpgradeBandwidth;
            [FieldOffset(17)][MarshalAs(UnmanagedType.U1)] public bool enforceTopologyConstraints;
            [FieldOffset(18)][MarshalAs(UnmanagedType.U1)] public bool isOutOfBandConnection;
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 32)]
            [FieldOffset(24)] public char[] unused; // std::string fast_advertisement_service_uuid;
            [FieldOffset(56)][MarshalAs(UnmanagedType.U1)] public bool lowPower;
        };

        public enum Status {
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

        public enum DistanceInfo {
            kUnknown = 1,
            kVeryClose = 2,
            kClose = 3,
            kFar = 4,
        };

        public enum Medium {
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

        [StructLayout(LayoutKind.Sequential)]
        public struct PayloadProgress {
            [MarshalAs(UnmanagedType.I8)] public Int64 payloadId;
            public enum Status {
                kSuccess,
                kFailure,
                kInProgress,
                kCanceled,
            };
            [MarshalAs(UnmanagedType.I8)] public Status status;
            [MarshalAs(UnmanagedType.I8)] public Int64 bytesTotal;
            [MarshalAs(UnmanagedType.I8)] public Int64 bytesTransferred;
        };

        [StructLayout(LayoutKind.Explicit)]
        public struct AdvertisingOptions {
            [FieldOffset(0)] public Strategy strategy;
            [FieldOffset(8)] public BooleanMediumSelector allowed;
            [FieldOffset(16)][MarshalAs(UnmanagedType.U1)] public bool autoUpgradeBandwidth;
            [FieldOffset(17)][MarshalAs(UnmanagedType.U1)] public bool enforceTopologyConstraints;
            [FieldOffset(18)][MarshalAs(UnmanagedType.U1)] public bool lowPower;
            [FieldOffset(19)][MarshalAs(UnmanagedType.U1)] public bool enableBluetoothListening;
            [FieldOffset(20)][MarshalAs(UnmanagedType.U1)] public bool enableWebrtcListening;
            [FieldOffset(21)][MarshalAs(UnmanagedType.U1)] public bool isOutOfBandConnection;
            [FieldOffset(24)][MarshalAs(UnmanagedType.ByValArray, SizeConst = 32)] public char[] unused; // std::string fast_advertisement_service_uuid;
            [FieldOffset(56)] public IntPtr deviceInfo;
        };

        [StructLayout(LayoutKind.Explicit)]
        public struct ConnectionOptions {
            [FieldOffset(0)] public Strategy strategy;
            [FieldOffset(8)] public BooleanMediumSelector allowed;
            [FieldOffset(16)][MarshalAs(UnmanagedType.U1)] public bool autoUpgradeBandwidth;
            [FieldOffset(17)][MarshalAs(UnmanagedType.U1)] public bool enforceTopologyConstraints;
            [FieldOffset(18)][MarshalAs(UnmanagedType.U1)] public bool lowPower;
            [FieldOffset(19)][MarshalAs(UnmanagedType.U1)] public bool enableBluetoothListening;
            [FieldOffset(20)][MarshalAs(UnmanagedType.U1)] public bool enableWebrtcListening;
            [FieldOffset(21)][MarshalAs(UnmanagedType.U1)] public bool isOutOfBandConnection;
            [FieldOffset(24)][MarshalAs(UnmanagedType.ByValArray, SizeConst = 32)] public byte[] remoteBluetoothMacAddress;
            [FieldOffset(56)][MarshalAs(UnmanagedType.ByValArray, SizeConst = 32)] public char[] unused; // std::string fast_advertisement_service_uuid;
            [FieldOffset(88)] public int keepAliveIntervalMillis;
            [FieldOffset(92)] public int keepAliveTimeoutMillis;
            [FieldOffset(96)][MarshalAs(UnmanagedType.ByValArray, SizeConst = 172)] public byte[] endpointInfo;
        };

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        public delegate void OperationResultCallback(Status status);

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        public delegate void EndpointFoundCallback(string endpointId,
            [MarshalAs(UnmanagedType.LPArray, SizeParamIndex = 2)] byte[] endpointInfo,
            int size,
            string serviceId);

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        public delegate void EndpointLostCallback(string endpointId);

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        public delegate void EndpointDistanceChangedCallback(string endpointId, DistanceInfo distanceInfo);

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        public delegate void ConnectionInitiatedCallback(
            string endpointId,
            [MarshalAs(UnmanagedType.LPArray, SizeParamIndex = 2)] byte[] endpointInfo,
            int size);

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        public delegate void ConnectionAcceptedCallback(string endpointId);

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        public delegate void ConnectionRejectedCallback(string endpointId, Status status);

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        public delegate void ConnectionDisconnectedCallback(string endpointId);

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        public delegate void BandwidthUpgradedCallback(string endpointId, Medium medium);

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        public delegate void PayloadInitiatedCallback(string endpointId, int payloadId, int payloadSize,
            [MarshalAs(UnmanagedType.LPArray, SizeParamIndex = 2)] byte[] payloadContent);

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        public delegate void PayloadProgressCallback(string endpointId, PayloadProgress payloadProgress);

        [DllImport(NearbyConnectionsDll)]
        public static extern IntPtr InitServiceControllerRouter();

        [DllImport(NearbyConnectionsDll)]
        public static extern IntPtr CloseServiceControllerRouter(IntPtr router);

        [DllImport(NearbyConnectionsDll)]
        public static extern IntPtr InitCore(IntPtr serviceControllerRouter);

        [DllImport(NearbyConnectionsDll)]
        public static extern void CloseCore(IntPtr core);

        [DllImport(NearbyConnectionsDll, EntryPoint = "StartDiscoveringSharp")]
        public static extern Status StartDiscovering(
            IntPtr core,
            [MarshalAs(UnmanagedType.LPStr)] string serviceId,
            DiscoveryOptions discoveryOptions,
            EndpointFoundCallback endpointFoundCallback,
            EndpointLostCallback endpointLostCallback,
            EndpointDistanceChangedCallback endpointDistanceChangedCallback);

        [DllImport(NearbyConnectionsDll, EntryPoint = "StopDiscoveringSharp")]
        public static extern Status StopDiscovering(IntPtr core);

        [DllImport(NearbyConnectionsDll, EntryPoint = "StartAdvertisingSharp")]
        public static extern Status StartAdvertising(
            IntPtr pCore,
            [MarshalAs(UnmanagedType.LPStr)] string serviceId,
            AdvertisingOptions advertisingOptions,
            byte[] endpointInfo,
            ConnectionInitiatedCallback initiatedCallback,
            ConnectionAcceptedCallback acceptedCallback,
            ConnectionRejectedCallback rejectedCallback,
            ConnectionDisconnectedCallback disconnectedCallback,
            BandwidthUpgradedCallback bandwidthUpgradedCallback);

        [DllImport(NearbyConnectionsDll, EntryPoint = "StopAdvertisingSharp")]
        public static extern Status StopAdvertising(IntPtr core);

        [DllImport(NearbyConnectionsDll, EntryPoint = "RequestConnectionSharp")]
        public static extern Status RequestConnection(
            IntPtr pCore,
            [MarshalAs(UnmanagedType.LPStr)] string endpointId,
            ConnectionOptions connectionOptions,
            byte[] endpointInfo,
            ConnectionInitiatedCallback initiatedCallback,
            ConnectionAcceptedCallback acceptedCallback,
            ConnectionRejectedCallback rejectedCallback,
            ConnectionDisconnectedCallback disconnectedCallback,
            BandwidthUpgradedCallback bandwidthUpgradedCallback);

        [DllImport(NearbyConnectionsDll, EntryPoint = "AcceptConnectionSharp")]
        public static extern Status AcceptConnection(
            IntPtr pCore,
            [MarshalAs(UnmanagedType.LPStr)] string endpointId,
            PayloadInitiatedCallback payloadInitiatedCallback,
            PayloadProgressCallback payloadProgressCallback);

        [DllImport(NearbyConnectionsDll, EntryPoint = "SendPayloadBytesSharp")]
        public static extern Status SendPayloadBytes(
            IntPtr pCore,
            [MarshalAs(UnmanagedType.LPStr)] string endpointId,
            int payloadSize,
            [MarshalAs(UnmanagedType.LPArray)] byte[] payloadContent);
    }
}
