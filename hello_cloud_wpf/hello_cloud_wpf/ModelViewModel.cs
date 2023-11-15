using System;
using System.Collections.ObjectModel;
using System.Diagnostics;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Windows;
using System.Windows.Input;
using static HelloCloudWpf.NearbyConnections;

namespace HelloCloudWpf {
    public class RelayCommand : ICommand {
        readonly Action<object?> _execute;
        readonly Predicate<object?> _canExecute;

        public RelayCommand(Action<object?> execute, Predicate<object?> canExecute) {
            _execute = execute ?? throw new ArgumentNullException("execute");
            _canExecute = canExecute;
        }

        public bool CanExecute(object? parameter) {
            return _canExecute == null ? true : _canExecute(parameter);
        }

        public event EventHandler? CanExecuteChanged {
            add { CommandManager.RequerySuggested += value; }
            remove { CommandManager.RequerySuggested -= value; }
        }

        public void Execute(object? parameter) { _execute(parameter); }
    }

    public class EndpointModel {
        public string id;
        public string name;

        public EndpointModel(string id, string name) {
            this.id = id;
            this.name = name;
        }
    }

    public class EndpointViewModel {
        public enum EndpointState {
            Discovered,  // Discovered, ready to connect
            Requested, // Pending connection request
            Accepted, // Connection request accepted
            Connected, // Connection established, ready to send/receive payloads
            Sending, // Sending a payload
            Receiving, // Receiving a payload
        }

        public EndpointModel model;

        public string Id { get { return model.id; } }
        public string Name { get { return model.name; } }
        public EndpointState State { get; set; }

        private readonly ICommand connectCommand;
        public ICommand ConnectCommand { get { return connectCommand; } }

        private readonly ICommand sendCommand;
        public ICommand SendCommand { get { return sendCommand; } }

        private readonly ICommand acceptCommand;
        public ICommand AcceptCommand { get { return acceptCommand; } }

        private readonly MainWindowViewModel mainWindowViewModel;

        public EndpointViewModel(MainWindowViewModel mainWindowViewModel, string id, string name) {
            this.mainWindowViewModel = mainWindowViewModel;
            State = EndpointState.Discovered;

            model = new EndpointModel(id, name);
            connectCommand = new RelayCommand(
                _ => mainWindowViewModel.RequestConnection(Id), 
                _ => State == EndpointState.Discovered);
            sendCommand = new RelayCommand(
                _ => mainWindowViewModel.SendBytes(Id), 
                _ => State == EndpointState.Connected);
            acceptCommand = new RelayCommand(
                _ => mainWindowViewModel.Accept(Id), 
                _ => false);
        }
    }

    public class MainWindowViewModel {
        static readonly string serviceId = "com.google.location.nearby.apps.helloconnections";

        private readonly IntPtr router = IntPtr.Zero;
        private readonly IntPtr core = IntPtr.Zero;
        private readonly ObservableCollection<EndpointViewModel> endpoints = new();

        public ObservableCollection<EndpointViewModel> Endpoints { get { return endpoints; } }
        public EndpointViewModel ActiveEndpoint { get; set; }

        // Local endpoint info. In our case, it's a UTF8 encoded name.
        byte[]? localEndpointInfo;
        string localEndpointName;
        public string LocalEndpointName {
            get {
                return localEndpointName;
            }
            set {
                localEndpointName = value;

                int len = Encoding.UTF8.GetByteCount(localEndpointName);
                localEndpointInfo = new byte[len + 1];
                Encoding.UTF8.GetBytes(localEndpointName, 0, localEndpointName.Length, localEndpointInfo, 0);
                localEndpointInfo[len] = 0;
            }
        }

        EndpointFoundCallback endpointFoundCallback;
        EndpointLostCallback endpointLostCallback;
        EndpointDistanceChangedCallback endpointDistanceChangedCallback;

        ConnectionInitiatedCallback initiatedCallback;
        ConnectionAcceptedCallback acceptedCallback;
        ConnectionRejectedCallback rejectedCallback;
        ConnectionDisconnectedCallback disconnectedCallback;
        BandwidthUpgradedCallback bandwidthUpgradedCallback;

        PayloadInitiatedCallback payloadInitiatedCallback;
        PayloadProgressCallback payloadProgressCallback;

        // Medium selection for advertising and connection
        BooleanMediumSelector advertisingMediumSelector = new() {
            ble = true,
            bluetooth = true,
            webRtc = true,
            wifiDirect = true,
            wifiHotspot = true,
            wifiLan = true,
        };

        BooleanMediumSelector discoveryMediumSelector = new() {
            ble = true,
            bluetooth = true,
            webRtc = true,
            wifiDirect = true,
            wifiHotspot = true,
            wifiLan = true,
        };

        BooleanMediumSelector connectionMediumSelector = new() {
            ble = true,
            bluetooth = true,
            webRtc = true,
            wifiDirect = true,
            wifiHotspot = true,
            wifiLan = true,
        };

        public MainWindowViewModel() {
            LocalEndpointName = Environment.GetEnvironmentVariable("COMPUTERNAME") ?? "Windows";

            Endpoints.Add(new EndpointViewModel(this, id: "ABCD", name: "Example endpoint 1"));
            Endpoints.Add(new EndpointViewModel(this, id: "1234", name: "Example endpoint 2"));
            ActiveEndpoint = Endpoints[0];

            router = InitServiceControllerRouter();
            if (router == IntPtr.Zero) {
                throw new Exception("Failed: InitServiceControllerRouter");
            }

            core = InitCore(router);
            if (core == IntPtr.Zero) {
                throw new Exception("Failed: InitCore");
            }

            // Allocate memory these delegates so that they don't get GCed.
            // TODO: free up the memory in the destructor.

            endpointFoundCallback = OnEndpointFound;
            endpointLostCallback = OnEndpointLost;
            endpointDistanceChangedCallback = OnEndpointDistanceChanged;

            GCHandle.Alloc(endpointFoundCallback);
            GCHandle.Alloc(endpointLostCallback);
            GCHandle.Alloc(endpointDistanceChangedCallback);

            initiatedCallback = OnConnectionInitiated;
            acceptedCallback = OnConnectionAccepted;
            rejectedCallback = OnConnectionRejected;
            disconnectedCallback = OnConnectionDisconnected;
            bandwidthUpgradedCallback = OnBandwidthUpgraded;

            GCHandle.Alloc(initiatedCallback);
            GCHandle.Alloc(acceptedCallback);
            GCHandle.Alloc(rejectedCallback);
            GCHandle.Alloc(disconnectedCallback);
            GCHandle.Alloc(bandwidthUpgradedCallback);

            payloadInitiatedCallback = OnPayloadInitiated;
            payloadProgressCallback = OnPayloadProgress;

            GCHandle.Alloc(payloadInitiatedCallback);
            GCHandle.Alloc(payloadProgressCallback);
        }

        EndpointViewModel? GetEndpointById(string endpointId) {
            foreach (EndpointViewModel endpoint in Endpoints) {
                if (endpoint.Id == endpointId) {
                    return endpoint;
                }
            }
            return null;
        }

        void UpdateEndpointState(string endpointId, EndpointViewModel.EndpointState state) {
            EndpointViewModel? endpoint = GetEndpointById(endpointId);
            if (endpoint == null) {
                Console.WriteLine("End point {0} not found. Probably already lost.", endpointId);
                return;
            }

            Console.WriteLine("Updating endpoint {0}'s state to {1}", endpointId, state);
            endpoint.State = state;
        }

        void AddEndpointOnUIThread(string id, string name) {
            Action<EndpointViewModel> addMethod = Endpoints.Add;
            Application.Current.Dispatcher.BeginInvoke(
                (string id, string name) => Endpoints.Add(new EndpointViewModel(this, id, name)), id, name);
        }

        void RemoveEndpointOnUIThread(string id) {
            Application.Current.Dispatcher.BeginInvoke(
                (string id) => {
                    EndpointViewModel? endpoint =
                    Endpoints.FirstOrDefault(endpoint => endpoint.Id == id);
                    if (endpoint != null) {
                        Endpoints.Remove(endpoint);
                    }
                },
                id);
        }

        public void StartAdvertising() {
            Console.WriteLine("Starting advertising...");
            AdvertisingOptions advertisingOptions = new() {
                strategy = NearbyConnections.P2pCluster,
                allowed = advertisingMediumSelector,
                autoUpgradeBandwidth = true,
                enforceTopologyConstraints = true,
                enableBluetoothListening = true,
                enableWebrtcListening = true,
                isOutOfBandConnection = false,
                lowPower = true,
                deviceInfo = IntPtr.Zero,
            };
            Debug.Assert(localEndpointInfo != null);
            NearbyConnections.StartAdvertising(
                core,
                serviceId,
                advertisingOptions,
                localEndpointInfo,
                initiatedCallback,
                acceptedCallback,
                rejectedCallback,
                disconnectedCallback,
                bandwidthUpgradedCallback);
        }

        public void StopAdvertising() {
            Console.WriteLine("Stopping advertising...");
            NearbyConnections.StopAdvertising(core);
        }

        public void StartDiscovering() {
            Console.WriteLine("Starting discovering...");
            DiscoveryOptions discoveryOptions = new() {
                strategy = NearbyConnections.P2pCluster,
                allowed = discoveryMediumSelector,
                autoUpgradeBandwidth = true,
                enforceTopologyConstraints = true,
                isOutOfBandConnection = false,
                lowPower = true,
            };
            NearbyConnections.StartDiscovering(
                core,
                serviceId,
                discoveryOptions,
                endpointFoundCallback,
                endpointLostCallback,
                endpointDistanceChangedCallback);
        }

        public void StopDiscovering() {
            Console.WriteLine("Stopping discovering...");
            NearbyConnections.StopDiscovering(core);
            Endpoints.Clear();
        }

        public void Accept(string endpoindId) {
            Console.WriteLine("Accepting...");
            OperationResult status = NearbyConnections.AcceptConnection(core, endpoindId, payloadInitiatedCallback, payloadProgressCallback);
            Console.WriteLine("  status: " + status.ToString());
        }

        public void SendBytes(string endpointId) {
            Console.WriteLine("Sending bytes...");
            Console.WriteLine("  endpoint_id: " + endpointId);

            UpdateEndpointState(endpointId, EndpointViewModel.EndpointState.Sending);

            byte[] payload = new byte[10];
            for (int i = 0; i < payload.Length; i++) {
                payload[i] = (byte)i;
            }
            NearbyConnections.SendPayloadBytes(core, endpointId, payload.Length, payload);
        }

        void OnEndpointFound(string endpointId, byte[] endpointInfo, int size, string serviceId) {
            var _ = size;
            string endpointName = Encoding.UTF8.GetString(endpointInfo);
            Console.WriteLine("OnEndPointFound:");
            Console.WriteLine("  endpoint_id: " + endpointId);
            Console.WriteLine("  endpoint_info: " + endpointName);
            Console.WriteLine("  service_id: " + serviceId);

            AddEndpointOnUIThread(endpointId, endpointName);
        }

        void OnEndpointLost(string endpointId) {
            Console.WriteLine("OnEndpointLost: " + endpointId);
            RemoveEndpointOnUIThread(endpointId);
        }

        void OnEndpointDistanceChanged(string endpointId, DistanceInfo distanceInfo) {
            Console.WriteLine("OnEndpointDistanceChanged: ");
            Console.WriteLine("  endpoint_id: " + endpointId);
            Console.WriteLine("  distance_info: " + distanceInfo);
        }

        void OnConnectionInitiated(string endpointId, byte[] endpointInfo, int size) {
            Console.WriteLine("OnConnectionInitiated:");
            Console.WriteLine("  endpoint_id: " + endpointId);

            Application.Current.Dispatcher.BeginInvoke(
                (string endpointId) => Accept(endpointId), endpointId);

            UpdateEndpointState(endpointId, EndpointViewModel.EndpointState.Requested);
        }

        void OnConnectionAccepted(string endpointId) {
            Console.WriteLine("OnConnectionAccepted:");
            Console.WriteLine("  endpoint_id: " + endpointId);

            UpdateEndpointState(endpointId, EndpointViewModel.EndpointState.Connected);
        }

        void OnConnectionRejected(string endpointId, OperationResult status) {
            Console.WriteLine("OnConnectionRejected:");
            Console.WriteLine("  endpoint_id: " + endpointId);
            Console.WriteLine("  status: " + status.ToString());

            UpdateEndpointState(endpointId, EndpointViewModel.EndpointState.Discovered);
        }

        void OnConnectionDisconnected(string endpointId) {
            Console.WriteLine("OnConnectionDisconnected:");
            Console.WriteLine("  endpoint_id: " + endpointId);

            UpdateEndpointState(endpointId, EndpointViewModel.EndpointState.Discovered);
        }

        void OnBandwidthUpgraded(string endpointId, Medium medium) {
            Console.WriteLine("OnBandwidthUpgraded:");
            Console.WriteLine("  endpoint_id: " + endpointId);
            Console.WriteLine("  medium: " + medium.ToString());
        }

        void OnPayloadInitiated(string endpointId, long payloadId, long payloadSize, byte[] payloadContent) {
            Console.WriteLine("OnPayloadInitiated:");
            Console.WriteLine("  endpoint_id: " + endpointId);
            Console.WriteLine("  payload_id: " + payloadId);
            for (int i = 0; i < Math.Min(16, payloadSize); i++) {
                Console.Write("{0:X} ", payloadContent[i]);
            }
            Console.WriteLine();

            EndpointViewModel? endpoint = GetEndpointById(endpointId);
            if (endpoint == null) {
                Console.WriteLine("  Endpoint not found, probably already lost.");
                return;
            }

            if (endpoint.State != EndpointViewModel.EndpointState.Sending) {
                endpoint.State = EndpointViewModel.EndpointState.Receiving;
            }
        }

        void OnPayloadProgress(string endpointId, long payloadId, PayloadStatus status,
            long bytesTotal, long bytesTransferred) {
            Console.WriteLine("OnPayloadProgress:");
            Console.WriteLine("  endpoint_id: " + endpointId);
            Console.WriteLine("  paylod_id: " + payloadId.ToString());
            Console.WriteLine("  status: " + status.ToString());

            switch (status) {
            case PayloadStatus.kSuccess:
                UpdateEndpointState(endpointId, EndpointViewModel.EndpointState.Connected);
                break;
            case PayloadStatus.kFailure:
                UpdateEndpointState(endpointId, EndpointViewModel.EndpointState.Connected);
                break;
            case PayloadStatus.kInProgress:
                break;
            case PayloadStatus.kCanceled:
                UpdateEndpointState(endpointId, EndpointViewModel.EndpointState.Connected);
                break;
            default:
                break;
            }
        }

        public void RequestConnection(string remoteEndpointId) {
            Console.WriteLine("Requesting connection to {0} ...", remoteEndpointId);
            Debug.Assert(localEndpointInfo != null);
            ConnectionOptions connectionOptions = new() {
                strategy = NearbyConnections.P2pCluster,
                allowed = connectionMediumSelector,
                autoUpgradeBandwidth = true,
                enforceTopologyConstraints = true,
                enableBluetoothListening = true,
                enableWebrtcListening = true,
                isOutOfBandConnection = false,
                lowPower = true,
                remoteBluetoothMacAddress = new byte[32],
            };
            OperationResult result = NearbyConnections.RequestConnection(
                core,
                remoteEndpointId,
                connectionOptions,
                localEndpointInfo,
                initiatedCallback,
                acceptedCallback,
                rejectedCallback,
                disconnectedCallback,
                bandwidthUpgradedCallback);
            Console.WriteLine("Request to connect to {0} completed. Result: " + result.ToString());
        }
    }
}
