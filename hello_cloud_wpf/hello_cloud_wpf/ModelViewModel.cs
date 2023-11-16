using System;
using System.Collections.ObjectModel;
using System.ComponentModel;
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

    public class EndpointViewModel : INotifyPropertyChanged {
        public enum EndpointState {
            Discovered,  // Discovered, ready to connect
            Pending, // Pending connection
            // Accepted, // Connection request accepted by one party
            Connected, // Connection request accepted by both parties. Ready to send/receive payloads
            Sending, // Sending a payload
            Receiving, // Receiving a payload
        }

        public Visibility SpinnerVisibility {
            get {
                return state == EndpointState.Pending
                    || state == EndpointState.Receiving
                    || state == EndpointState.Sending
                    ? Visibility.Visible
                    : Visibility.Hidden;
            }
        }

        public Visibility CheckMarkVisibility {
            get {
                return state == EndpointState.Connected
                    ? Visibility.Visible : Visibility.Hidden;
            }
        }

        public Visibility GrayDotVisibility {
            get {
                return state == EndpointState.Discovered
                    ? Visibility.Visible : Visibility.Hidden;
            }
        }

        public Medium Medium {
            get {
                return medium;
            }
            set {
                medium = value;
                PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(Medium)));
            }
        }

        public string Id { get { return model.id; } }

        public string Name { get { return model.name; } }

        public EndpointState State {
            get { return state; }
            set {
                state = value;
                PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(State)));
                PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(SpinnerVisibility)));
                PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(CheckMarkVisibility)));
                PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(GrayDotVisibility)));
            }
        }

        public event PropertyChangedEventHandler? PropertyChanged;

        public ICommand ConnectCommand { get { return connectCommand; } }
        public ICommand SendCommand { get { return sendCommand; } }
        public ICommand DisconnectCommand { get { return disconnectCommand; } }

        private readonly EndpointModel model;
        private readonly MainWindowViewModel mainWindowViewModel;

        private EndpointState state;
        private Medium medium;

        private readonly ICommand connectCommand;
        private readonly ICommand sendCommand;
        private readonly ICommand disconnectCommand;

        public EndpointViewModel(MainWindowViewModel mainWindowViewModel, string id, string name) {
            this.mainWindowViewModel = mainWindowViewModel;
            this.model = new EndpointModel(id, name);
            this.medium = Medium.kUnknown;
            State = EndpointState.Discovered;

            connectCommand = new RelayCommand(
                _ => this.mainWindowViewModel.RequestConnection(Id),
                _ => State == EndpointState.Discovered);
            sendCommand = new RelayCommand(
                _ => this.mainWindowViewModel.SendBytes(Id),
                _ => State == EndpointState.Connected);
            disconnectCommand = new RelayCommand(
                _ => this.mainWindowViewModel.Disconnect(Id),
                _ => State == EndpointState.Connected);
        }
    }

    public class MainWindowViewModel : INotifyPropertyChanged {
        static readonly string serviceId = "com.google.location.nearby.apps.helloconnections";

        private readonly IntPtr router = IntPtr.Zero;
        private readonly IntPtr core = IntPtr.Zero;
        private readonly ObservableCollection<EndpointViewModel> endpoints = new();

        public ObservableCollection<EndpointViewModel> Endpoints { get { return endpoints; } }

        private EndpointViewModel? selectedEndpoint;
        public EndpointViewModel? SelectedEndpoint {
            get { return selectedEndpoint; }
            set {
                selectedEndpoint = value;
                PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(SelectedEndpoint)));
            }
        }

        public Cursor Cursor {
            get {
                return busy ? Cursors.Wait : Cursors.Arrow;
            }
        }

        // Whether we are in the middle of an operation. Used for showing the hourglass cursor.
        private bool busy;

        private void SetBusy(bool value) {
            busy = value;
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(Cursor)));
        }

        // Local endpoint info. In our case, it's a UTF8 encoded name.
        byte[] localEndpointInfo;
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

        public ObservableCollection<string> LogEntries { get; set; }

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

        public event PropertyChangedEventHandler? PropertyChanged;

#pragma warning disable CS8618
        public MainWindowViewModel() {
            LogEntries = new ObservableCollection<string>();
            LocalEndpointName = Environment.GetEnvironmentVariable("COMPUTERNAME") ?? "Windows";

            Endpoints.Add(new EndpointViewModel(this, id: "ABCD", name: "Example endpoint 1"));
            Endpoints.Add(new EndpointViewModel(this, id: "1234", name: "Example endpoint 2"));
            SelectedEndpoint = Endpoints[0];

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
#pragma warning restore CS8618

        EndpointViewModel? GetEndpointById(string endpointId) {
            foreach (EndpointViewModel endpoint in Endpoints) {
                if (endpoint.Id == endpointId) {
                    return endpoint;
                }
            }
            return null;
        }

        EndpointViewModel.EndpointState? GetEndpointState(string endpointId) {
            EndpointViewModel? endpoint = GetEndpointById(endpointId);
            if (endpoint == null) {
                Log(String.Format("End point {0} not found. Probably already lost.", endpointId));
                return null;
            }
            return endpoint.State;
        }

        // TODO: does this also need to happen on the UI thread?
        void UpdateEndpointState(string endpointId, EndpointViewModel.EndpointState state) {
            EndpointViewModel? endpoint = GetEndpointById(endpointId);
            if (endpoint == null) {
                Log(String.Format("End point {0} not found. Probably already lost.", endpointId));
                return;
            }

            Log(String.Format("Updating endpoint {0}'s state to {1}", endpointId, state));
            endpoint.State = state;
        }

        #region Operations on the UI thread
        void SelectEndpointOnUIThread(string endpointId) {
            Application.Current.Dispatcher.BeginInvoke(
                (string endpointId) => {
                    EndpointViewModel? endpoint = GetEndpointById(endpointId);
                    Debug.Assert(endpoint != null);
                    SelectedEndpoint = endpoint;
                }, endpointId);
        }

        void AddEndpointOnUIThread(EndpointViewModel endpoint) {
            Application.Current.Dispatcher.BeginInvoke(
                (EndpointViewModel endpoint) => {
                    Endpoints.Add(endpoint);
                    SelectedEndpoint = endpoint;
                }, endpoint);
        }

        void AddEndpointOnUIThread(string id, string name) {
            AddEndpointOnUIThread(new EndpointViewModel(this, id, name));
        }

        void RemoveEndpointOnUIThread(string id) {
            Application.Current.Dispatcher.BeginInvoke(
                (string id) => {
                    EndpointViewModel? endpoint = Endpoints.FirstOrDefault(endpoint => endpoint.Id == id);
                    if (endpoint != null) {
                        Endpoints.Remove(endpoint);
                        // If we have just removed the previously selected item, set selection to the first entry
                        if (SelectedEndpoint == endpoint && Endpoints.Count > 0) {
                            SelectedEndpoint = Endpoints[0];
                        }
                    }
                },
                id);
        }

        public void Accept(string endpointId) {
            Application.Current.Dispatcher.BeginInvoke(
                (string endpointId) => {
                    Log("Accepting...");
                    Log("  endpoint_id: " + endpointId);
                    OperationResult status = NearbyConnections.AcceptConnection(core, endpointId, payloadInitiatedCallback, payloadProgressCallback);
                    Log("  status: " + status.ToString());
                }, endpointId);
        }

        void Log(string message) {
            Application.Current.Dispatcher.BeginInvoke(
                (string message) => { LogEntries.Add(message); },
                message);
        }
        #endregion

        #region NearbyConnections operations.
        public void StartAdvertising() {
            Log("Starting advertising...");
            // StartAdvertising is a sync call. Show hourglass icon before it's done.
            SetBusy(true);
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
            OperationResult result = NearbyConnections.StartAdvertising(
                core,
                serviceId,
                advertisingOptions,
                localEndpointInfo,
                initiatedCallback,
                acceptedCallback,
                rejectedCallback,
                disconnectedCallback,
                bandwidthUpgradedCallback);
            Log("StartAdvertising finished. Result: " + result);
            SetBusy(false);
        }

        public void StopAdvertising() {
            Log("Stopping advertising...");
            SetBusy(true);
            OperationResult result = NearbyConnections.StopAdvertising(core);
            Log("StopAdvertising finished. Result: " + result);
            SetBusy(false);
        }

        public void StartDiscovering() {
            Log("Starting discovering...");
            SetBusy(true);
            DiscoveryOptions discoveryOptions = new() {
                strategy = NearbyConnections.P2pCluster,
                allowed = discoveryMediumSelector,
                autoUpgradeBandwidth = true,
                enforceTopologyConstraints = true,
                isOutOfBandConnection = false,
                lowPower = true,
            };
            OperationResult result = NearbyConnections.StartDiscovering(
                core,
                serviceId,
                discoveryOptions,
                endpointFoundCallback,
                endpointLostCallback,
                endpointDistanceChangedCallback);
            Log("StartDiscovering finished. Result: " + result);
            SetBusy(false);
        }

        public void StopDiscovering() {
            Log("Stopping discovering...");
            NearbyConnections.StopDiscovering(core);
            Endpoints.Clear();
        }

        public void RequestConnection(string remoteEndpointId) {
            Log(String.Format("Requesting connection to {0} ...", remoteEndpointId));
            Debug.Assert(localEndpointInfo != null);
            SetBusy(true);
            UpdateEndpointState(remoteEndpointId, EndpointViewModel.EndpointState.Pending);

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
            Log(String.Format("Request to connect to {0} completed. Result: {1}", remoteEndpointId, result));
            if (result != OperationResult.kSuccess) {
                // The request has not been sent by NC. We go back to Discovered state.
                UpdateEndpointState(remoteEndpointId, EndpointViewModel.EndpointState.Discovered);
            }
            SetBusy(false);
        }

        public void SendBytes(string endpointId) {
            Log("Sending bytes...");
            Log("  endpoint_id: " + endpointId);
            SetBusy(true);
            UpdateEndpointState(endpointId, EndpointViewModel.EndpointState.Sending);

            byte[] payload = new byte[10];
            for (int i = 0; i < payload.Length; i++) {
                payload[i] = (byte)i;
            }
            OperationResult result = NearbyConnections.SendPayloadBytes(core, endpointId, payload.Length, payload);
            Log("SendBytes finished. Result: " + result);
            SetBusy(false);

        }

        public void Disconnect(string endpointId) {
            Log("Disconnecting...");
            Log("  endpoint_id: " + endpointId);
            SetBusy(true);
            OperationResult result = NearbyConnections.Disconnect(core, endpointId);
            Log("Disconnect finished. Result: " + result);
            SetBusy(false);

        }
        #endregion

        #region Callbacks
        void OnEndpointFound(string endpointId, byte[] endpointInfo, int size, string serviceId) {
            var _ = size;
            string endpointName = Encoding.UTF8.GetString(endpointInfo);
            Log("OnEndPointFound:");
            Log("  endpoint_id: " + endpointId);
            Log("  endpoint_info: " + endpointName);
            Log("  service_id: " + serviceId);

            AddEndpointOnUIThread(endpointId, endpointName);
        }

        void OnEndpointLost(string endpointId) {
            Log("OnEndpointLost: " + endpointId);
            RemoveEndpointOnUIThread(endpointId);
        }

        void OnEndpointDistanceChanged(string endpointId, DistanceInfo distanceInfo) {
            Log("OnEndpointDistanceChanged: ");
            Log("  endpoint_id: " + endpointId);
            Log("  distance_info: " + distanceInfo);
        }

        void OnConnectionInitiated(string endpointId, byte[] endpointInfo, int size) {
            Log("OnConnectionInitiated:");
            Log("  endpoint_id: " + endpointId);

            Application.Current.Dispatcher.BeginInvoke(
                (string endpointId) => Accept(endpointId), endpointId);

            if (GetEndpointById(endpointId) == null) {
                string name = Encoding.UTF8.GetString(endpointInfo);
                if (String.IsNullOrEmpty(name)) {
                    Log(String.Format("Endpoint {0} has an invalid name, discarding...", endpointId));
                    return;
                }

                EndpointViewModel endpoint = new(this, endpointId, name) {
                    State = EndpointViewModel.EndpointState.Pending
                };
                AddEndpointOnUIThread(endpoint);
            } else {
                UpdateEndpointState(endpointId, EndpointViewModel.EndpointState.Pending);
            }

            SelectEndpointOnUIThread(endpointId);
        }

        void OnConnectionAccepted(string endpointId) {
            Log("OnConnectionAccepted:");
            Log("  endpoint_id: " + endpointId);

            UpdateEndpointState(endpointId, EndpointViewModel.EndpointState.Connected);
        }

        void OnConnectionRejected(string endpointId, OperationResult result) {
            Log("OnConnectionRejected:");
            Log("  endpoint_id: " + endpointId);
            Log("  result: " + result.ToString());

            UpdateEndpointState(endpointId, EndpointViewModel.EndpointState.Discovered);
        }

        void OnConnectionDisconnected(string endpointId) {
            Log("OnConnectionDisconnected:");
            Log("  endpoint_id: " + endpointId);

            UpdateEndpointState(endpointId, EndpointViewModel.EndpointState.Discovered);
        }

        void OnBandwidthUpgraded(string endpointId, Medium medium) {
            Log("OnBandwidthUpgraded:");
            Log("  endpoint_id: " + endpointId);
            Log("  medium: " + medium.ToString());

            var endpoint = GetEndpointById(endpointId);
            if (endpoint != null) {
                endpoint.Medium = medium;
            }
        }

        void OnPayloadInitiated(string endpointId, long payloadId, long payloadSize, byte[] payloadContent) {
            Log("OnPayloadInitiated:");
            Log("  endpoint_id: " + endpointId);
            Log("  payload_id: " + payloadId);
            StringBuilder stringBuilder = new();
            for (int i = 0; i < Math.Min(16, payloadSize); i++) {
                stringBuilder.AppendFormat("{0:X} ", payloadContent[i]);
            }
            Log(stringBuilder.ToString());

            if (GetEndpointState(endpointId) != EndpointViewModel.EndpointState.Sending) {
                UpdateEndpointState(endpointId, EndpointViewModel.EndpointState.Receiving);
            }
        }

        void OnPayloadProgress(string endpointId, long payloadId, PayloadStatus status,
            long bytesTotal, long bytesTransferred) {
            Log("OnPayloadProgress:");
            Log("  endpoint_id: " + endpointId);
            Log("  paylod_id: " + payloadId.ToString());
            Log("  status: " + status.ToString());
            Log(String.Format("  bytes transferred: {0}/{1}", bytesTransferred, bytesTotal));

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
        #endregion
    }
}
