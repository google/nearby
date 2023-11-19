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

    public struct EndpointModel {
        public enum State {
            Discovered,  // Discovered, ready to connect
            Pending, // Pending connection
            // Accepted, // Connection request accepted by one party
            Connected, // Connection request accepted by both parties. Ready to send/receive payloads
            Sending, // Sending a payload
            Receiving, // Receiving a payload
        }

        public string id;
        public string name;
        public Medium? medium = null;
        public State? state = null;
        // An incoming endpoint is one that we didn't discover, but one that has discovered us and connected to us.
        public bool isIncoming;

        public EndpointModel(string id, string name, State? state = null, bool isIncoming = false) {
            this.id = id;
            this.name = name;
            this.state = state;
            this.isIncoming = isIncoming;
        }
    }

    public class EndpointViewModel : INotifyPropertyChanged {
        public Visibility SpinnerIconVisibility {
            get => model.state == EndpointModel.State.Pending
                || model.state == EndpointModel.State.Receiving
                || model.state == EndpointModel.State.Sending
                ? Visibility.Visible : Visibility.Hidden;
        }

        public Visibility GreenIconVisibility {
            get => model.state == EndpointModel.State.Connected
                ? Visibility.Visible : Visibility.Hidden;
        }

        public Visibility GrayIconVisibility {
            get => model.state == EndpointModel.State.Discovered ?
                Visibility.Visible : Visibility.Hidden;
        }

        public string MediumReadableName { get => model.medium?.ReadableName() ?? ""; }

        public Medium? Medium {
            get => model.medium;
            set {
                model.medium = value;
                PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(MediumReadableName)));
            }
        }

        public string Id { get => model.id; }

        public string Name { get => model.name; }

        public bool IsIncoming { get => model.isIncoming; }

        public EndpointModel.State? State {
            get => model.state;
            set {
                model.state = value;
                PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(State)));
                PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(SpinnerIconVisibility)));
                PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(GreenIconVisibility)));
                PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(GrayIconVisibility)));

                // Refresh the states of the buttons if the endpoint is currently selected.
                if (this == mainWindow.SelectedEndpoint) {
                    UpdateCanExecute();
                }
            }
        }

        public ObservableCollection<OutgoingFileViewModel> OutgoingFiles { get => outgoingFiles; }
        public ObservableCollection<IncomingFileViewModel> IncomingFiles { get => incomingFiles; }

        public ObservableCollection<TransferViewModel> Transfers { get => transfers; }

        public event PropertyChangedEventHandler? PropertyChanged;

        public ICommand ConnectCommand { get => connectCommand; }
        public ICommand SendCommand { get => sendCommand; }
        public ICommand DisconnectCommand { get => disconnectCommand; }
        public ICommand PickFilesCommand { get => pickFilesCommand; }
        public ICommand BeginUploadCommand { get => beginUploadCommand; }
        public ICommand BeginDownloadCommand { get => beginDownloadCommand; }

        private EndpointModel model;
        private readonly MainWindowViewModel mainWindow;

        private readonly ObservableCollection<TransferViewModel> transfers = new();
        private readonly ObservableCollection<OutgoingFileViewModel> outgoingFiles = new();
        private readonly ObservableCollection<IncomingFileViewModel> incomingFiles = new();

        private readonly ICommand connectCommand;
        private readonly ICommand sendCommand;
        private readonly ICommand disconnectCommand;
        private readonly ICommand pickFilesCommand;
        private readonly ICommand beginUploadCommand;
        private readonly ICommand beginDownloadCommand;

        public EndpointViewModel(
            MainWindowViewModel mainWindowViewModel, EndpointModel model) {
            this.mainWindow = mainWindowViewModel;
            this.model = model;

            connectCommand = new RelayCommand(
                _ => mainWindow.RequestConnection(Id),
                _ => State == EndpointModel.State.Discovered);
            sendCommand = new RelayCommand(
                _ => mainWindow.SendFiles(Id, outgoingFiles.Select(file => (file.FileName, file.Url!))),
                _ => State == EndpointModel.State.Connected && outgoingFiles.Any() && outgoingFiles.All(file => file.IsUploaded));
            disconnectCommand = new RelayCommand(
                _ => mainWindow.Disconnect(Id),
                _ => State == EndpointModel.State.Connected);
            pickFilesCommand = new RelayCommand(
                _ => PickFiles(),
                _ => outgoingFiles.All(file => !file.IsUploading) && State == EndpointModel.State.Connected);
            beginUploadCommand = new RelayCommand(
                _ => BeginUpload(),
                _ => outgoingFiles.All(file => !file.IsUploading) && outgoingFiles.Any(file => !file.IsUploaded));
            beginDownloadCommand = new RelayCommand(
                _ => BeginDownload(),
                _ => incomingFiles.All(file => !file.IsDownloading) && incomingFiles.Any(file => !file.IsDownloaded));
        }

        public void AddIncomingFile(IncomingFileViewModel file) {
            Application.Current.Dispatcher.BeginInvoke(IncomingFiles.Add, file);
        }

        public void AddTransfer(TransferViewModel transfer) {
            Application.Current.Dispatcher.BeginInvoke(Transfers.Add, transfer);
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
                outgoingFiles.Clear();
                foreach (string fileName in openFileDialog.FileNames) {
                    outgoingFiles.Add(
                        new OutgoingFileViewModel(new OutgoingFileModel(fileName)));
                }
            }
        }

        private void BeginUpload() {
            Thread thread = new(UploadWorker);
            thread.Start();
        }

        private void UploadWorker() {
            // TODO: only upload files that haven't been uploaded successfully
            List<Task<bool>> uploadingTasks = new();
            foreach (OutgoingFileViewModel file in OutgoingFiles) {
                uploadingTasks.Add(file.Upload());
            }
            Task.WaitAll(uploadingTasks.ToArray());

            foreach (var (task, file) in uploadingTasks.Zip(outgoingFiles)) {
                mainWindow.Log("Uploading completed.");
                mainWindow.Log("  file name: " + file.FileName);
                mainWindow.Log("  url: " + file.Url);
                mainWindow.Log("  result: " + (task.Result ? "Success" : "Failure"));
            }
            UpdateCanExecute();

            // TODO: only add successful uploads
            foreach (OutgoingFileViewModel file in OutgoingFiles) {
                TransferViewModel transfer = new(
                direction: TransferViewModel.Direction.Upload,
                fileName: file.FileName, url: file.Url!, result: TransferViewModel.Result.Success);
                AddTransfer(transfer);
            }
        }

        private void BeginDownload() {
            Thread thread = new(DownloadWorker);
            thread.Start();
        }

        private void DownloadWorker() {
            List<IncomingFileViewModel> filesToDownload =
                incomingFiles.Where(file => !file.IsDownloaded && !file.IsDownloading).ToList();

            List<Task<bool>> downloadingTasks = new();
            foreach (IncomingFileViewModel file in filesToDownload) {
                downloadingTasks.Add(file.Download());
            }
            Task.WaitAll(downloadingTasks.ToArray());

            foreach (var (task, file) in downloadingTasks.Zip(filesToDownload)) {
                mainWindow.Log("Downloading completed.");
                mainWindow.Log("  file name: " + file.FileName);
                mainWindow.Log("  url: " + file.Url);
                mainWindow.Log("  result: " + (task.Result ? "Success" : "Failure"));
            }

            foreach (IncomingFileViewModel file in filesToDownload) {
                TransferViewModel transfer = new(
                direction: TransferViewModel.Direction.Download,
                fileName: file.FileName, url: file.Url, result: TransferViewModel.Result.Success);
                AddTransfer(transfer);
            }
        }
    }
}
