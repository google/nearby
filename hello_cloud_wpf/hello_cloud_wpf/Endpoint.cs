using Google.Cloud.Storage.V1;
using Microsoft.Win32;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Input;
using static HelloCloudWpf.NearbyConnections;

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

    public class EndpointModel {
        public enum State {
            Discovered,  // Discovered, ready to connect
            Pending, // Pending connection
            // Accepted, // Connection request accepted by one party
            Connected, // Connection request accepted by both parties. Ready to send/receive payloads
            Sending, // Sending a payload
            Receiving, // Receiving a payload
        }

        public string id = string.Empty;
        public string name = string.Empty;
        public Medium? medium = null;
        public State? state = null;
        // An incoming endpoint is one that we didn't discover,
        // but one that has discovered us and connected to us.
        public bool isIncoming;

        public Collection<OutgoingFileModel> outgoingFiles = new();
        public Collection<IncomingFileModel> incomingFiles = new();
        public Collection<TransferModel> transfers = new();

        public EndpointModel(string id, string name, State? state = null, bool isIncoming = false) {
            this.id = id;
            this.name = name;
            this.state = state;
            this.isIncoming = isIncoming;
        }
    }

    public class EndpointViewModel : INotifyPropertyChanged, IViewModel<EndpointModel> {
        public Visibility SpinnerIconVisibility => Model!.state is EndpointModel.State.Pending
                or EndpointModel.State.Receiving
                or EndpointModel.State.Sending
                ? Visibility.Visible : Visibility.Hidden;

        public Visibility GreenIconVisibility => Model!.state == EndpointModel.State.Connected
                ? Visibility.Visible : Visibility.Hidden;

        public Visibility GrayIconVisibility => Model!.state == EndpointModel.State.Discovered ?
                Visibility.Visible : Visibility.Hidden;

        public string MediumReadableName { get => Model!.medium?.ReadableName() ?? ""; }

        public Medium? Medium {
            get => Model!.medium;
            set {
                Model!.medium = value;
                PropertyChanged?.Invoke(this, new(nameof(MediumReadableName)));
            }
        }

        public string Id { get => Model!.id; }

        public string Name { get => Model!.name; }

        public bool IsIncoming { get => Model!.isIncoming; }

        public EndpointModel.State? State {
            get => Model!.state;
            set {
                Model!.state = value;
                PropertyChanged?.Invoke(this, new(nameof(State)));
                PropertyChanged?.Invoke(this, new(nameof(SpinnerIconVisibility)));
                PropertyChanged?.Invoke(this, new(nameof(GreenIconVisibility)));
                PropertyChanged?.Invoke(this, new(nameof(GrayIconVisibility)));

                // Refresh the states of the buttons if the endpoint is currently selected.
                if (this == MainViewModel.Instance.SelectedEndpoint) {
                    UpdateCanExecute();
                }
            }
        }

        public ObservableCollection<OutgoingFileViewModel> OutgoingFiles => outgoingFiles;

        public ObservableCollection<IncomingFileViewModel> IncomingFiles => incomingFiles;

        public ObservableCollection<TransferViewModel> Transfers => transfers;

        public event PropertyChangedEventHandler? PropertyChanged;

        public ICommand ConnectCommand => connectCommand;
        public ICommand SendCommand => sendCommand;
        public ICommand DisconnectCommand => disconnectCommand;
        public ICommand PickFilesCommand => pickFilesCommand;
        public ICommand BeginUploadCommand => beginUploadCommand;
        public ICommand BeginDownloadCommand => beginDownloadCommand;

        public EndpointModel? Model {
            get => model;
            set {
                model = value;
                outgoingFiles = new(model!.outgoingFiles);
                incomingFiles = new(model!.incomingFiles);
                transfers = new(model!.transfers);
            }
        }

        private ViewModelCollection<OutgoingFileViewModel, OutgoingFileModel> outgoingFiles;
        private ViewModelCollection<IncomingFileViewModel, IncomingFileModel> incomingFiles;
        private ViewModelCollection<TransferViewModel, TransferModel> transfers;

        private readonly ICommand connectCommand;
        private readonly ICommand sendCommand;
        private readonly ICommand disconnectCommand;
        private readonly ICommand pickFilesCommand;
        private readonly ICommand beginUploadCommand;
        private readonly ICommand beginDownloadCommand;

        private EndpointModel? model;

        public EndpointViewModel() {
            connectCommand = new RelayCommand(
                _ => MainViewModel.Instance.RequestConnection(Id),
                _ => State == EndpointModel.State.Discovered);
            sendCommand = new RelayCommand(
                _ => MainViewModel.Instance.SendFiles(
                    Id, OutgoingFiles.Select(file => (file.LocalPath, file.RemotePath!))),
                _ => State == EndpointModel.State.Connected
                    && OutgoingFiles.Any()
                    && OutgoingFiles.All(file => file.IsUploaded));
            disconnectCommand = new RelayCommand(
                _ => MainViewModel.Instance.Disconnect(Id),
                _ => State == EndpointModel.State.Connected);
            pickFilesCommand = new RelayCommand(
                _ => PickFiles(),
                _ => OutgoingFiles.All(file => !file.IsUploading)
                    && State == EndpointModel.State.Connected);
            beginUploadCommand = new RelayCommand(
                _ => BeginUpload(),
                _ => OutgoingFiles.All(file => !file.IsUploading)
                    && OutgoingFiles.Any(file => !file.IsUploaded));
            beginDownloadCommand = new RelayCommand(
                _ => BeginDownload(),
                _ => IncomingFiles.All(file => !file.IsDownloading)
                    && IncomingFiles.Any(file => !file.IsDownloaded));
        }

        public void AddIncomingFile(IncomingFileModel file) {
            Application.Current.Dispatcher.BeginInvoke(
                IncomingFiles.Add,
                new IncomingFileViewModel() { Model = file });
        }

        public void AddTransfer(TransferModel transfer) {
            Application.Current.Dispatcher.BeginInvoke(
                Transfers.Add,
                new TransferViewModel() { Model = transfer });
        }

        public void ClearOutgoingFiles() {
            Application.Current.Dispatcher.BeginInvoke(OutgoingFiles.Clear);
        }

        public void ClearIncomingFiles() {
            Application.Current.Dispatcher.BeginInvoke(IncomingFiles.Clear);
        }

        public void ClearTransfers() {
            Application.Current.Dispatcher.BeginInvoke(Transfers.Clear);
        }

        public static void UpdateCanExecute() {
            Application.Current.Dispatcher.BeginInvoke(CommandManager.InvalidateRequerySuggested);
        }

        private void PickFiles() {
            OpenFileDialog openFileDialog = new() { Multiselect = true };
            bool? result = openFileDialog.ShowDialog();
            if (result == true) {
                OutgoingFiles.Clear();
                foreach (string fileName in openFileDialog.FileNames) {
                    OutgoingFiles.Add(
                        new OutgoingFileViewModel() {
                            Model = new OutgoingFileModel(fileName)
                        });
                }
            }
        }

        private void BeginUpload() {
            MainViewModel.Instance.SetBusy(true);
            Thread thread = new(UploadWorker);
            thread.Start();
            MainViewModel.Instance.SetBusy(false);
        }

        private void UploadWorker() {
            // See https://cloud.google.com/docs/authentication/provide-credentials-adc#local-dev
            // for setting up adc
            StorageClient client = StorageClient.Create();

            List<Task<string?>> uploadingTasks = new();
            foreach (OutgoingFileViewModel file in OutgoingFiles) {
                uploadingTasks.Add(file.Upload(client));
            }
            Task.WaitAll(uploadingTasks.ToArray());

            foreach (var (task, file) in uploadingTasks.Zip(OutgoingFiles)) {
                MainViewModel.Instance.Log("Uploading completed.");
                MainViewModel.Instance.Log("  local path: " + file.LocalPath);
                MainViewModel.Instance.Log("  remote path: " + task.Result);
            }
            UpdateCanExecute();

            foreach (OutgoingFileViewModel file in OutgoingFiles) {
                TransferModel transfer = new(
                    direction: TransferModel.Direction.Upload,
                    fileName: file.LocalPath,
                    url: file.RemotePath!,
                    result: TransferModel.Result.Success);
                AddTransfer(transfer);
            }
        }

        private void BeginDownload() {
            Thread thread = new(DownloadWorker);
            thread.Start();
        }

        private void DownloadWorker() {
            // See https://cloud.google.com/docs/authentication/provide-credentials-adc#local-dev
            // for setting up adc
            StorageClient client = StorageClient.Create();

            List<IncomingFileViewModel> filesToDownload =
                IncomingFiles.Where(file => !file.IsDownloaded && !file.IsDownloading).ToList();

            List<Task<string?>> downloadingTasks = new();
            foreach (IncomingFileViewModel file in filesToDownload) {
                downloadingTasks.Add(file.Download(client));
            }
            Task.WaitAll(downloadingTasks.ToArray());

            foreach (var (task, file) in downloadingTasks.Zip(filesToDownload)) {
                MainViewModel.Instance.Log("Downloading completed.");
                MainViewModel.Instance.Log("  remote path: " + file.RemotePath);
                MainViewModel.Instance.Log("  local path: " + task.Result);
            }

            foreach (IncomingFileViewModel file in filesToDownload) {
                TransferModel transfer = new(
                    direction: TransferModel.Direction.Download,
                    fileName: file.Path,
                    url: file.RemotePath,
                    result: TransferModel.Result.Success);
                AddTransfer(transfer);
            }
        }
    }
}
