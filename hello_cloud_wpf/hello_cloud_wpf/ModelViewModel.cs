using Microsoft.Win32;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net;
using System.Numerics;
using System.Runtime.InteropServices;
using System.Runtime.Serialization.Formatters.Binary;
using System.Security.Policy;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Input;
using static HelloCloudWpf.NearbyConnections;
using static System.Net.WebRequestMethods;

namespace HelloCloudWpf {
    // Normally, we should use an enum class so that we can bake behaviours into the enum.
    // But we will eventually use protobuf to generate Medium enum. So we use extension
    // methods to add behaviours to this enum.
    public static class MediumToString {
        static readonly IReadOnlyDictionary<Medium, string> mediumNames =
            new Dictionary<Medium, string>() {
                {Medium.kUnknown, "Unknown"},
                {Medium.kMdns, "mDNS"},
                {Medium.kBluetooth, "Bluetooth Classic"},
                {Medium.kWiFiHotSpot, "Wi-Fi hotspot"},
                {Medium.kBle, "Bluetooth Low Energy"},
                {Medium.kWiFiLan, "Wi-Fi LAN"},
                {Medium.kWifiAware, "Wi-Fi Aware"},
                {Medium.kNfc, "NFC"},
                {Medium.kWiFiDirect, "Wi-Fi Direct"},
                {Medium.kWebRtc, "Web RTC"},
                {Medium.kBleL2Cap, "BLE L2CAP"},
                {Medium.kUsb, "USB"},
            };

        public static string ReadableName(this Medium medium) => mediumNames[medium];
    }

    public class RelayCommand : ICommand {
        readonly Action<object?> execute;
        readonly Predicate<object?> canExecute;

        public RelayCommand(Action<object?> execute, Predicate<object?> canExecute) {
            this.execute = execute ?? throw new ArgumentNullException(nameof(execute));
            this.canExecute = canExecute;
        }

        public bool CanExecute(object? parameter) {
            return canExecute == null ? true : canExecute(parameter);
        }

        public event EventHandler? CanExecuteChanged {
            add { CommandManager.RequerySuggested += value; }
            remove { CommandManager.RequerySuggested -= value; }
        }

        public void Execute(object? parameter) { execute(parameter); }
    }

    public struct EndpointModel {
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

        public Visibility SpinnerIconVisibility {
            get => state == EndpointState.Pending
                || state == EndpointState.Receiving
                || state == EndpointState.Sending
                ? Visibility.Visible : Visibility.Hidden;
        }

        public Visibility GreenIconVisibility {
            get => state == EndpointState.Connected
                ? Visibility.Visible : Visibility.Hidden;
        }

        public Visibility GrayIconVisibility {
            get => state == EndpointState.Discovered ?
                Visibility.Visible : Visibility.Hidden;
        }

        public string MediumReadableName { get => medium?.ReadableName() ?? "Unknown"; }

        // TODO: does this belong to the model or the viewmodel?
        public Medium? Medium {
            get => medium;
            set {
                medium = value;
                PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(MediumReadableName)));
            }
        }

        public string Id { get => model.id; }

        // TODO: does this belong to the model or the viewmodel?
        public string Name { get => model.name; }

        // An incoming endpoint is one that we didn't discover, but one that has discovered us and connected to us.
        public bool IsIncoming { get; set; }

        public EndpointState? State {
            get => state;
            set {
                state = value;
                PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(State)));
                PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(SpinnerIconVisibility)));
                PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(GreenIconVisibility)));
                PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(GrayIconVisibility)));
            }
        }

        public ObservableCollection<FileViewModel> Files { get => files; }

        public ObservableCollection<TransferViewModel> Transfers { get => transfers; }

        public event PropertyChangedEventHandler? PropertyChanged;

        public ICommand ConnectCommand { get => connectCommand; }
        public ICommand SendCommand { get => sendCommand; }
        public ICommand DisconnectCommand { get => disconnectCommand; }
        public ICommand PickFilesCommand { get => pickFilesCommand; }
        public ICommand BeginUploadCommand { get => beginUploadCommand; }

        private readonly EndpointModel model;
        private readonly MainWindowViewModel mainWindowViewModel;

        private readonly ObservableCollection<TransferViewModel> transfers = new();
        private readonly ObservableCollection<FileViewModel> files = new();

        private readonly ICommand connectCommand;
        private readonly ICommand sendCommand;
        private readonly ICommand disconnectCommand;
        private readonly ICommand pickFilesCommand;
        private readonly ICommand beginUploadCommand;

        private EndpointState? state = null;
        private Medium? medium;
        private bool isUploading;

        public EndpointViewModel(
            MainWindowViewModel mainWindowViewModel,
            string id, string name, EndpointState? state = null) {
            this.mainWindowViewModel = mainWindowViewModel;
            this.model = new EndpointModel(id, name);
            this.medium = null;
            this.state = state;

            connectCommand = new RelayCommand(
                _ => this.mainWindowViewModel.RequestConnection(Id),
                _ => State == EndpointState.Discovered);
            sendCommand = new RelayCommand(
                _ => this.mainWindowViewModel.SendFiles(Id, files.Select(file => (file.FileName, file.Url!))),
                _ => State == EndpointState.Connected && files.Any() && files.All(file => file.IsUploaded));
            disconnectCommand = new RelayCommand(
                _ => this.mainWindowViewModel.Disconnect(Id),
                _ => State == EndpointState.Connected);
            pickFilesCommand = new RelayCommand(
                _ => PickFiles(),
                _ => !isUploading && State == EndpointState.Connected);
            beginUploadCommand = new RelayCommand(
                _ => BeginUpload(),
                _ => CanUpload());
        }


        public void ClearFiles() {
            Application.Current.Dispatcher.BeginInvoke(Files.Clear);
        }

        public static void UpdateCanExecute() {
            Application.Current.Dispatcher.BeginInvoke(CommandManager.InvalidateRequerySuggested);
        }

        private void PickFiles() {
            OpenFileDialog openFileDialog = new() { Multiselect = true };
            bool? result = openFileDialog.ShowDialog();
            if (result == true) {
                files.Clear();
                foreach (string fileName in openFileDialog.FileNames) {
                    files.Add(new FileViewModel(fileName));
                }
            }
        }

        private bool CanUpload() {
            // not currently uploading, and there's at least one file not uploaded
            return !isUploading && files.Any(file => !file.IsUploaded);
        }

        private void BeginUpload() {
            isUploading = true;
            Thread thread = new(UploadWorker);
            thread.Start();
        }

        private void UploadWorker() {
            // TODO: only upload files that haven't been uploaded successfully
            List<Task<bool>> uploadingTasks = new();
            foreach (FileViewModel file in Files) {
                uploadingTasks.Add(file.Upload());
            }
            Task.WaitAll(uploadingTasks.ToArray());

            foreach (var (task, file) in uploadingTasks.Zip(files)) {
                bool result = task.Result;
                string fileName = file.FileName;
                mainWindowViewModel.Log("Uploading completed.");
                mainWindowViewModel.Log("  file name: " + fileName);
                mainWindowViewModel.Log("  result: " + (result ? "Success" : "Failure"));
            }
            isUploading = false;
            UpdateCanExecute();

            // TODO: only add successful uploads
            foreach (FileViewModel file in Files) {
                TransferViewModel transfer = new(
                direction: TransferViewModel.Direction.Upload,
                fileName: file.FileName, url: file.Url!, result: TransferViewModel.Result.Success);
                this.mainWindowViewModel.AddTransfer(Id, transfer);
            }
        }
    }

    public class TransferViewModel {
        public enum Direction { Send, Receive, Upload, Download }
        public enum Result { Success, Failure, Canceled }

        public Direction direction;
        public Result result;
        public string fileName;
        public string url;

        public TransferViewModel(Direction direction, string fileName, string url, Result result) {
            this.direction = direction;
            this.fileName = fileName;
            this.url = url;
            this.result = result;
        }

        public override string ToString() {
            StringBuilder sb = new();
            sb.Append(DirectionToChar(direction))
                .Append(ResultToChar(result))
                .Append(' ')
                .Append(fileName)
                .Append(" | ")
                .Append(url);
            return sb.ToString();
        }

        private static char DirectionToChar(Direction direction) {
            return direction switch {
                Direction.Send => '↖',
                Direction.Receive => '↘',
                Direction.Upload => '↑',
                Direction.Download => '↓',
                _ => '⚠',
            };
        }

        private static char ResultToChar(Result result) {
            return result switch {
                Result.Success => '✓',
                Result.Failure => '✕',
                Result.Canceled => '⚠',
                _ => ' ',
            };
        }
    }

    public class FileViewModel : INotifyPropertyChanged {
        public event PropertyChangedEventHandler? PropertyChanged;

        public string FileName { get => fileName; }
        public string? Url { get => url; }
        public bool IsUploaded { get => !String.IsNullOrEmpty(Url); }

        public Visibility UploadedIconVisibility { get => IsUploaded ? Visibility.Visible : Visibility.Hidden; }
        public Visibility UploadingIconVisibility { get => isUploading ? Visibility.Visible : Visibility.Hidden; }
        public Visibility NotUploadedIconVisibility { get => IsUploaded || isUploading ? Visibility.Hidden : Visibility.Visible; }

        private readonly string fileName;
        private string? url = null;
        private bool isUploading;

        public FileViewModel(string fileName) {
            this.fileName = fileName;
        }

        public async Task<bool> Upload() {
            isUploading = true;
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(UploadedIconVisibility)));
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(NotUploadedIconVisibility)));
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(UploadingIconVisibility)));

            await Task.Delay(1000);
            url = "http://foo.bar/awesome.jpg";
            isUploading = false;
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(UploadedIconVisibility)));
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(NotUploadedIconVisibility)));
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(UploadingIconVisibility)));

            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(Url)));
            return true;
        }
    }

    public class MainWindowViewModel : INotifyPropertyChanged {
        private static readonly string serviceId = "com.google.location.nearby.apps.helloconnections";

        // Medium selection for advertising and connection
        private static readonly BooleanMediumSelector advertisingMediumSelector = new() {
            ble = true,
            bluetooth = true,
            webRtc = true,
            wifiDirect = true,
            wifiHotspot = true,
            wifiLan = true,
        };

        private static readonly BooleanMediumSelector discoveryMediumSelector = new() {
            ble = true,
            bluetooth = true,
            webRtc = true,
            wifiDirect = true,
            wifiHotspot = true,
            wifiLan = true,
        };

        private static readonly BooleanMediumSelector connectionMediumSelector = new() {
            ble = true,
            bluetooth = true,
            webRtc = true,
            wifiDirect = true,
            wifiHotspot = true,
            wifiLan = true,
        };

        public ObservableCollection<EndpointViewModel> Endpoints => endpoints;

        public EndpointViewModel? SelectedEndpoint {
            get => selectedEndpoint;
            set {
                selectedEndpoint = value;
                PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(SelectedEndpoint)));
            }
        }

        public static EndpointViewModel NullSelectedEndpoint { get { return nullSelectedEndpoint!; } }

        public Cursor Cursor {
            get => busy ? Cursors.Wait : Cursors.Arrow;
        }

        public string LocalEndpointName {
            get => localEndpointName;
            set {
                localEndpointName = value;

                int len = Encoding.UTF8.GetByteCount(localEndpointName);
                localEndpointInfo = new byte[len + 1];
                Encoding.UTF8.GetBytes(localEndpointName, 0, localEndpointName.Length, localEndpointInfo, 0);
                localEndpointInfo[len] = 0;
            }
        }

        public bool IsNotAdvertising {
            get => isNotAdvertising;
            set {
                isNotAdvertising = value;
                PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(IsNotAdvertising)));
            }
        }

        public ObservableCollection<string> LogEntries { get; set; }
        public event PropertyChangedEventHandler? PropertyChanged;
        public ICommand ClearLogCommand { get { return clearLogCommand; } }

        private readonly IntPtr router = IntPtr.Zero;
        private readonly IntPtr core = IntPtr.Zero;

        private static EndpointViewModel? nullSelectedEndpoint = null;

        private readonly ObservableCollection<EndpointViewModel> endpoints = new();
        private EndpointViewModel? selectedEndpoint;
        // Local endpoint info. In our case, it's a UTF8 encoded name.
        private byte[] localEndpointInfo;
        private string localEndpointName;
        private bool isNotAdvertising = true;

        private readonly ICommand clearLogCommand;

        // Whether we are in the middle of an operation. Used for showing the hourglass cursor.
        private bool busy;

        private readonly EndpointFoundCallback endpointFoundCallback;
        private readonly EndpointLostCallback endpointLostCallback;
        private readonly EndpointDistanceChangedCallback endpointDistanceChangedCallback;

        private readonly ConnectionInitiatedCallback initiatedCallback;
        private readonly ConnectionAcceptedCallback acceptedCallback;
        private readonly ConnectionRejectedCallback rejectedCallback;
        private readonly ConnectionDisconnectedCallback disconnectedCallback;
        private readonly BandwidthUpgradedCallback bandwidthUpgradedCallback;

        private readonly PayloadInitiatedCallback payloadInitiatedCallback;
        private readonly PayloadProgressCallback payloadProgressCallback;

#pragma warning disable CS8618
        public MainWindowViewModel() {
            // Note: any operation that changes a property that has a binding from the UI, e.g. the busy state,
            // needs to happen on the UI thread. But since some can only be triggered from the UI, e.g.
            // StartDiscovering(), we're not ensure to make these changes in the UI thread. But we probably should
            // do that rather than rely on that we're already on the UI thread. Right now it's very messy. If there
            // is any weird UI related bugs, they are probably due to this. We'll clean it up later after prototyping.
            LogEntries = new ObservableCollection<string>();

            clearLogCommand = new RelayCommand(
                _ => ClearLog(),
                _ => CanClearLog());

            LocalEndpointName = Environment.GetEnvironmentVariable("COMPUTERNAME") ?? "Windows";

            SelectedEndpoint = null;
            nullSelectedEndpoint = new EndpointViewModel(this, string.Empty, string.Empty);

            //var endpoint = new EndpointViewModel(this, id: "ABCD", name: "Example endpoint 1") {
            //    Medium = Medium.kBluetooth
            //};
            //Endpoints.Add(endpoint);
            //SelectedEndpoint = endpoint;

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

        public void Deinit() {
            if (core != IntPtr.Zero) {
                NearbyConnections.CloseCore(core);
            }
            if (router != IntPtr.Zero) {
                NearbyConnections.CloseServiceControllerRouter(router);
            }
        }

        #region NearbyConnections operations.
        public void StartAdvertising() {
            Log("Starting advertising...");
            // StartAdvertising is a sync call. Show hourglass icon before it's done.
            SetBusy(true);
            IsNotAdvertising = false;
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
            IsNotAdvertising = true;
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
            SetBusy(true);
            NearbyConnections.StopDiscovering(core);
            Endpoints.Clear();
            SetBusy(false);
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

        public void SendFiles(string endpointId, IEnumerable<(string fileName, string url)> files) {
            (int fileCount, byte[] payload) = EncodePayload(files);

            Log($"Sending {endpointId} {fileCount} file(s), {payload.Length} bytes in total ...");
            SetBusy(true);
            UpdateEndpointState(endpointId, EndpointViewModel.EndpointState.Sending);
            OperationResult result = NearbyConnections.SendPayloadBytes(core, endpointId, payload.Length, payload);
            if (result != OperationResult.kSuccess) {
                UpdateEndpointState(endpointId, EndpointViewModel.EndpointState.Discovered);
            }
            Log($"Sending files to {endpointId} finished. Result: {result}");
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

        public void SetBusy(bool value) {
            busy = value;
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(Cursor)));
        }

        public void Log(string message) {
            Application.Current.Dispatcher.BeginInvoke(LogEntries.Add, message);
        }

        public void AddTransfer(string endpointId, TransferViewModel transfer) {
            EndpointViewModel? endpoint = Endpoints.FirstOrDefault(endpoint => endpoint.Id == endpointId);
            if (endpoint == null) {
                Log("Trying to add a transfer from/to an endpoint that has already been removed.");
                Log("  endpoint_id: " + endpointId);
                Log("  transfer: " + transfer);
                return;
            }
            Application.Current.Dispatcher.BeginInvoke(endpoint.Transfers.Add, transfer);
        }

        private EndpointViewModel? GetEndpoint(string endpointId) {
            foreach (EndpointViewModel endpoint in Endpoints) {
                if (endpoint.Id == endpointId) {
                    return endpoint;
                }
            }
            return null;
        }

        private EndpointViewModel.EndpointState? GetEndpointState(string endpointId) {
            EndpointViewModel? endpoint = GetEndpoint(endpointId);
            if (endpoint == null) {
                Log(String.Format("End point {0} not found. Probably already lost.", endpointId));
                return null;
            }
            return endpoint.State;
        }

        // TODO: does this also need to happen on the UI thread?
        private void UpdateEndpointState(string endpointId, EndpointViewModel.EndpointState state) {
            EndpointViewModel? endpoint = GetEndpoint(endpointId);
            if (endpoint == null) {
                Log(String.Format("End point {0} not found. Probably already lost.", endpointId));
                return;
            }

            Log(String.Format("Updating endpoint {0}'s state to {1}", endpointId, state));
            endpoint.State = state;

            // Refresh the states of the buttons if the endpoint is currently selected.
            if (endpointId == SelectedEndpoint?.Id) {
                EndpointViewModel.UpdateCanExecute();
            }
        }

        #region Operations on the UI thread
        private void SelectEndpoint(string endpointId) {
            // Checking for the existence of the endpoint needs to happen on the UI thread
            // instead of the caller thread here, because we could call SelectEndpoint()
            // immediately after AddEndpoint().
            // In this case, adding the endpoint has been scheduled but has not been executed.
            Application.Current.Dispatcher.BeginInvoke(
                (string endpointId) => {
                    EndpointViewModel? endpoint = GetEndpoint(endpointId);

                    if (endpoint == null) {
                        Log("Trying to select an endpoint that has already been removed. Id: " + endpointId);
                        return;
                    }
                    SelectedEndpoint = endpoint;
                },
                endpointId);
        }

        private void AddEndpoint(EndpointViewModel endpoint) {
            Application.Current.Dispatcher.BeginInvoke(
                (EndpointViewModel endpoint) => {
                    Endpoints.Add(endpoint);
                    SelectedEndpoint = endpoint;
                },
                endpoint);
        }

        private void RemoveEndpoint(string id) {
            EndpointViewModel? endpoint = Endpoints.FirstOrDefault(endpoint => endpoint.Id == id);
            if (endpoint == null) {
                Log("Trying to remove an endpoint that has already been removed. Id: " + id);
                return;
            }

            Application.Current.Dispatcher.BeginInvoke(
                (EndpointViewModel endpoint) => {
                    bool removingSelected = SelectedEndpoint == endpoint;
                    Endpoints.Remove(endpoint);
                    // If we have just removed the previously selected item, set selection to the first entry if it exists
                    if (removingSelected) {
                        SelectedEndpoint = Endpoints.FirstOrDefault();
                    }
                },
                endpoint);
        }

        private void Accept(string endpointId) {
            Application.Current.Dispatcher.BeginInvoke(
                (string endpointId) => {
                    Log("Accepting...");
                    Log("  endpoint_id: " + endpointId);
                    OperationResult status = NearbyConnections.AcceptConnection(
                        core, endpointId, payloadInitiatedCallback, payloadProgressCallback);
                    Log("  status: " + status.ToString());
                },
                endpointId);
        }

        private void ClearLog() {
            LogEntries.Clear();
        }

        private bool CanClearLog() {
            return LogEntries.Count > 0;
        }
        #endregion

        #region Callbacks
        private void OnEndpointFound(string endpointId, byte[] endpointInfo, int size, string serviceId) {
            var _ = size;
            string endpointName = Encoding.UTF8.GetString(endpointInfo);
            Log("OnEndPointFound:");
            Log("  endpoint_id: " + endpointId);
            Log("  endpoint_info: " + endpointName);
            Log("  service_id: " + serviceId);

            AddEndpoint(new EndpointViewModel(
                this, endpointId, endpointName, EndpointViewModel.EndpointState.Discovered));
        }

        private void OnEndpointLost(string endpointId) {
            Log("OnEndpointLost: " + endpointId);
            RemoveEndpoint(endpointId);
        }

        private void OnEndpointDistanceChanged(string endpointId, DistanceInfo distanceInfo) {
            Log("OnEndpointDistanceChanged: ");
            Log("  endpoint_id: " + endpointId);
            Log("  distance_info: " + distanceInfo);
        }

        private void OnConnectionInitiated(string endpointId, byte[] endpointInfo, int size) {
            Log("OnConnectionInitiated:");
            Log("  endpoint_id: " + endpointId);

            Application.Current.Dispatcher.BeginInvoke(
                (string endpointId) => Accept(endpointId), endpointId);

            if (GetEndpoint(endpointId) == null) {
                // We didn't discover this endpoint. Add it to the list of remote endpoints.
                string name = Encoding.UTF8.GetString(endpointInfo);
                if (String.IsNullOrEmpty(name)) {
                    Log(String.Format("Endpoint {0} has an invalid name, discarding...", endpointId));
                    return;
                }
                // Since we didn't discover this endpoint, we should remove it after we
                // disconnect from it later. So let's mark it as "incoming".
                EndpointViewModel endpoint = new(this, endpointId, name) {
                    State = EndpointViewModel.EndpointState.Pending,
                    IsIncoming = true,
                };
                AddEndpoint(endpoint);
            } else {
                UpdateEndpointState(endpointId, EndpointViewModel.EndpointState.Pending);
            }

            SelectEndpoint(endpointId);
        }

        private void OnConnectionAccepted(string endpointId) {
            Log("OnConnectionAccepted:");
            Log("  endpoint_id: " + endpointId);

            UpdateEndpointState(endpointId, EndpointViewModel.EndpointState.Connected);
        }

        private void RemoveOrChangeState(string endpointId) {
            // If the endpoint wasn't discovered by us in the first place, remove it.
            // Otherwise, keep it and change its state to Discovered.
            EndpointViewModel? endpoint = GetEndpoint(endpointId);
            if (endpoint?.IsIncoming ?? false) {
                Application.Current.Dispatcher.BeginInvoke(Endpoints.Remove, endpoint);
            } else {
                UpdateEndpointState(endpointId, EndpointViewModel.EndpointState.Discovered);
            }
        }

        private void OnConnectionRejected(string endpointId, OperationResult result) {
            Log("OnConnectionRejected:");
            Log("  endpoint_id: " + endpointId);
            Log("  result: " + result.ToString());
            RemoveOrChangeState(endpointId);
        }

        private void OnConnectionDisconnected(string endpointId) {
            Log("OnConnectionDisconnected:");
            Log("  endpoint_id: " + endpointId);
            RemoveOrChangeState(endpointId);
        }

        private void OnBandwidthUpgraded(string endpointId, Medium medium) {
            Log("OnBandwidthUpgraded:");
            Log("  endpoint_id: " + endpointId);
            Log("  medium: " + medium.ToString());

            var endpoint = GetEndpoint(endpointId);
            if (endpoint != null) {
                endpoint.Medium = medium;
            }
        }

        // It's weird that NC passes the payload content before updating the progress.
        // We are supposed to monitor the progress and wait till it's completed before
        // consuming the payload content. But I'm not sure how it'd work for managed
        // code, since we cannot hold on to the buffer.
        // TODO: investigate how this works for Dart and Java
        private void OnPayloadInitiated(string endpointId, long payloadId, long payloadSize, byte[] payloadContent) {
            Log("OnPayloadInitiated:");
            Log("  endpoint_id: " + endpointId);
            Log("  payload_id: " + payloadId);

            if (GetEndpointState(endpointId) != EndpointViewModel.EndpointState.Sending) {
                UpdateEndpointState(endpointId, EndpointViewModel.EndpointState.Receiving);
                IEnumerable<(string fileName, string url)> files = DecodePayload(payloadContent);
                foreach ((string fileName, string url) in files) {
                    TransferViewModel transfer = 
                        new(TransferViewModel.Direction.Receive, fileName, url, result: TransferViewModel.Result.Success);
                    AddTransfer(endpointId, transfer);
                }
            }
        }

        private void OnPayloadProgress(string endpointId, long payloadId, PayloadStatus status,
            long bytesTotal, long bytesTransferred) {
            Log("OnPayloadProgress:");
            Log("  endpoint_id: " + endpointId);
            Log("  paylod_id: " + payloadId.ToString());
            Log("  status: " + status.ToString());
            Log(String.Format("  bytes transferred: {0}/{1}", bytesTransferred, bytesTotal));

            bool isSending = GetEndpointState(endpointId) == EndpointViewModel.EndpointState.Sending;
            TransferViewModel.Result result;
            EndpointViewModel? endpoint = GetEndpoint(endpointId);

            switch (status) {
            case PayloadStatus.kInProgress:
                return;
            case PayloadStatus.kSuccess:
                result = TransferViewModel.Result.Success;
                UpdateEndpointState(endpointId, EndpointViewModel.EndpointState.Connected);
                if (isSending) {
                    endpoint?.ClearFiles();
                }
                break;
            case PayloadStatus.kFailure:
                result = TransferViewModel.Result.Failure;
                UpdateEndpointState(endpointId, EndpointViewModel.EndpointState.Connected);
                break;
            case PayloadStatus.kCanceled:
                result = TransferViewModel.Result.Canceled;
                UpdateEndpointState(endpointId, EndpointViewModel.EndpointState.Connected);
                break;
            default:
                return;
            }

            if (isSending && endpoint != null) {
                foreach (FileViewModel file in endpoint!.Files) {
                    TransferViewModel transfer = 
                        new(TransferViewModel.Direction.Send, file.FileName, file.Url!, result: result);
                    AddTransfer(endpointId, transfer);
                }
            }
        }

        private static (int, byte[]) EncodePayload(IEnumerable<(string fileName, string url)> files) {
            // Buffer format: 
            // int32: file count
            // Each file:
            //   int32: file name length including the \x0 at the end, not including this int
            //   file name string content, encoded in UTF8
            //   \x0
            //   int32: url length, including the \x0 at the end, not including this int
            //   url content, encoded in UTF8
            //   \x0
            // In other words, file names and URLs are in both Pascal and C styles, to make
            // decoding a bit easier.

            static void WriteStringToStream(MemoryStream stream, string s) {
                int len = s.Length;
                byte[] buffer = BitConverter.GetBytes(len + 1);
                stream.Write(buffer, 0, buffer.Length);

                buffer = Encoding.UTF8.GetBytes(s);
                stream.Write(buffer, 0, buffer.Length);
                stream.WriteByte(0);
            }

            MemoryStream stream = new();
            byte[] buffer;
            int fileCount = 0;

            // File count placeholder, since we haven't counted yet
            buffer = BitConverter.GetBytes(0);
            stream.Write(buffer, 0, buffer.Length);

            foreach ((string fileName, string url) in files) {
                WriteStringToStream(stream, fileName);
                WriteStringToStream(stream, url!);
                fileCount++;
            }

            // Now write the actual file count
            stream.Position = 0;
            buffer = BitConverter.GetBytes(fileCount);
            stream.Write(buffer, 0, buffer.Length);
            return (fileCount, stream.ToArray());
        }

        private static IList<(string fileName, string url)> DecodePayload(byte[] payload) {
            static string ReadString(byte[] payload, ref int offset) {
                int len = BitConverter.ToInt32(payload, offset);
                offset += sizeof(int);
                string s = Encoding.UTF8.GetString(payload, offset, len - 1);
                offset += len;
                return s;
            }

            List<(string fileName, string url)> files = new();

            int offset = 0;
            int fileCount = BitConverter.ToInt32(payload, 0);
            offset += sizeof(int);

            for (int i = 0; i < fileCount; i++) {
                string fileName = ReadString(payload, ref offset);
                string url = ReadString(payload, ref offset);
                files.Add((fileName, url));
            }

            return files;
        }
        #endregion
    }
}
