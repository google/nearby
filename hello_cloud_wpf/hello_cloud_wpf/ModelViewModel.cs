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

        public RelayCommand(Action<object?> execute) : this(execute, canExecute: null) { }
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
        public EndpointModel model;

        public string Id { get { return model.id; } }
        public string Name { get { return model.name; } }

        private readonly ICommand connectCommand;
        public ICommand ConnectCommand { get { return connectCommand; } }

        private MainWindowViewModel mainWindowViewModel;

        public EndpointViewModel(MainWindowViewModel mainWindowViewModel, string id, string name) {
            this.mainWindowViewModel = mainWindowViewModel;
            model = new EndpointModel(id, name);
            connectCommand = new RelayCommand(param => this.Connect(param), param => this.CanConnect(param));
        }

        public void Connect(object? param) {
            mainWindowViewModel.RequestConnection(Id);
        }

        public bool CanConnect(object? param) {
            return true;
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
        OperationResultCallback discoveryStartedCallback;

        ConnectionInitiatedCallback initiatedCallback;
        ConnectionAcceptedCallback acceptedCallback;
        ConnectionRejectedCallback rejectedCallback;
        ConnectionDisconnectedCallback disconnectedCallback;
        BandwidthUpgradedCallback bandwidthUpgradedCallback;
        OperationResultCallback advertisingStartedCallback;

        OperationResultCallback connectionStartedCallback;

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
            discoveryStartedCallback = OnDiscoveryStarted;

            GCHandle.Alloc(endpointFoundCallback);
            GCHandle.Alloc(endpointLostCallback);
            GCHandle.Alloc(endpointDistanceChangedCallback);
            GCHandle.Alloc(discoveryStartedCallback);

            initiatedCallback = OnConnectionInitiated;
            acceptedCallback = OnConnectionAccepted;
            rejectedCallback = OnConnectionRejected;
            disconnectedCallback = OnConnectionDisconnected;
            bandwidthUpgradedCallback = OnBandwidthUpgraded;
            advertisingStartedCallback = OnAdvertisingStarted;

            GCHandle.Alloc(initiatedCallback);
            GCHandle.Alloc(acceptedCallback);
            GCHandle.Alloc(rejectedCallback);
            GCHandle.Alloc(disconnectedCallback);
            GCHandle.Alloc(bandwidthUpgradedCallback);
            GCHandle.Alloc(advertisingStartedCallback);

            connectionStartedCallback = OnConnectionStarted;
            GCHandle.Alloc(connectionStartedCallback);

            Console.WriteLine("Starting discovery...");
            DiscoveryOptions discoveryOptions = new() {
                strategy = NearbyConnections.P2pCluster,
                allowed = discoveryMediumSelector,
                autoUpgradeBandwidth = true,
                enforceTopologyConstraints = true,
                isOutOfBandConnection = false,
                lowPower = true,
            };
            StartDiscovery(
                core,
                serviceId,
                discoveryOptions,
                endpointFoundCallback,
                endpointLostCallback,
                endpointDistanceChangedCallback,
                discoveryStartedCallback);
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

        void OnDiscoveryStarted(Status status) {
            Console.WriteLine("OnDiscoveryStarted: ");
            Console.WriteLine("  status: " + status.ToString());
        }

        void OnEndpointFound(string endpointId, byte[] endpointInfo, int size, string serviceId) {
            var _ = size;
            string endpointName = Encoding.UTF8.GetString(endpointInfo);
            Console.WriteLine("OnEndPointFound: ");
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

        void OnAdvertisingStarted(Status status) {
            Console.WriteLine("OnStartAdvertisingStarted:");
            Console.WriteLine("  status: " + status.ToString());
        }

        void OnConnectionInitiated(string endpointId, byte[] endpointInfo, int size) {
            Console.WriteLine("OnInitiated:");
            Console.WriteLine("  endpoint_id: " + endpointId);
        }

        void OnConnectionAccepted(string endpointId) {
            Console.WriteLine("OnAccepted:");
            Console.WriteLine("  endpoint_id: " + endpointId);
        }

        void OnConnectionRejected(string endpointId, Status status) {
            Console.WriteLine("OnRejected:");
            Console.WriteLine("  endpoint_id: " + endpointId);
            Console.WriteLine("  status: " + status.ToString());
        }

        void OnConnectionDisconnected(string endpointId) {
            Console.WriteLine("OnDisconnected:");
            Console.WriteLine("  endpoint_id: " + endpointId);
        }

        void OnBandwidthUpgraded(string endpointId, Medium medium) {
            Console.WriteLine("OnBandwidthUpgraded:");
            Console.WriteLine("  endpoint_id: " + endpointId);
            Console.WriteLine("  medium: " + medium.ToString());
        }

        void OnConnectionStarted(Status status) {
            Console.WriteLine("OnConnectionStarted:");
            Console.WriteLine("  status: " + status.ToString());
        }

        public void RequestConnection(string remoteEndpointId) {
            Console.WriteLine("Requesting connection...");
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
            NearbyConnections.RequestConnection(
                core,
                remoteEndpointId,
                connectionOptions,
                localEndpointInfo,
                initiatedCallback,
                acceptedCallback,
                rejectedCallback,
                disconnectedCallback,
                bandwidthUpgradedCallback,
                connectionStartedCallback);
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

            int len = Encoding.UTF8.GetByteCount(LocalEndpointName);
            localEndpointInfo = new byte[len + 1];
            Encoding.UTF8.GetBytes(LocalEndpointName, 0, LocalEndpointName.Length, localEndpointInfo, 0);
            localEndpointInfo[len] = 0;

            NearbyConnections.StartAdvertising(
                core,
                serviceId,
                advertisingOptions,
                localEndpointInfo,
                initiatedCallback,
                acceptedCallback,
                rejectedCallback,
                disconnectedCallback,
                bandwidthUpgradedCallback,
                advertisingStartedCallback);
        }
    }
}
