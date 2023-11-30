using Google.Cloud.Storage.V1;
using System.ComponentModel;
using System.IO;
using System.Threading.Tasks;
using System.Windows;

namespace HelloCloudWpf {
    public class IncomingFileModel {
        public enum State {
            Received, Downloading, Downloaded
        }

        // the path of the file when it was uploaded, not necessarily the path for downloading.
        public readonly string localPath;
        public readonly string remotePath;
        public readonly long fileSize;
        public State state = State.Received;

        public IncomingFileModel(string localPath, string remotePath, long fileSize) {
            this.localPath = localPath;
            this.remotePath = remotePath;
            this.fileSize = fileSize;
        }
    }

    public class IncomingFileViewModel : INotifyPropertyChanged, IViewModel<IncomingFileModel> {
        public event PropertyChangedEventHandler? PropertyChanged;

        public string LocalPath => Model!.localPath;
        public string RemotePath => Model!.remotePath;
        public long FileSize => Model!.fileSize;

        public Visibility DownloadedIconVisibility => Model?.state == IncomingFileModel.State.Downloaded? Visibility.Visible : Visibility.Hidden;
        public Visibility DownloadingIconVisibility => Model?.state == IncomingFileModel.State.Downloading ? Visibility.Visible : Visibility.Hidden;
        public Visibility ReceivedIconVisibility => Model?.state == IncomingFileModel.State.Received ? Visibility.Visible : Visibility.Hidden;

        public IncomingFileModel? Model { get; set; }

        public IncomingFileModel.State State => Model?.state ?? IncomingFileModel.State.Received;

        public IncomingFileViewModel() { }

        public async Task<string?> Download(StorageClient storageClient) {
            MainViewModel.Instance.Log("Beginning downloading " + RemotePath);

            Model!.state = IncomingFileModel.State.Downloading;
            PropertyChanged?.Invoke(this, new(nameof(DownloadedIconVisibility)));
            PropertyChanged?.Invoke(this, new(nameof(ReceivedIconVisibility)));
            PropertyChanged?.Invoke(this, new(nameof(DownloadingIconVisibility)));

            // Quick and dirty way to get a local path
            string localPath;
            int index = LocalPath.LastIndexOf('/');
            if (index == -1) {
                index = LocalPath.LastIndexOf("\\");
            }
            if (index == -1) {
                localPath = LocalPath;
            } else {
                localPath = App.DocumentsPath + LocalPath[(index + 1)..];
            }
            if (File.Exists(localPath)) {
                string basePath;
                string extension;
                index = localPath.LastIndexOf('.');
                if (index != -1) {
                    basePath = localPath[..index];
                    extension = "." + localPath[(index + 1)..];
                } else {
                    basePath = localPath;
                    extension = string.Empty;
                }

                int n = 1;
                while (true) {
                    localPath = basePath + n.ToString() + extension;
                    if (!File.Exists(localPath)) {
                        break;
                    }
                }
            }

            FileStream outputFile = File.OpenWrite(localPath);
            var result = await storageClient.DownloadObjectAsync(App.GcsBucketName, RemotePath, outputFile);
            outputFile.Close();

            if (result != null) {
                MainViewModel.Instance.Log("Finished downloading " + RemotePath + " to " + localPath);
            } else {
                MainViewModel.Instance.Log("Finished downloading " + RemotePath + ", failed.");
            }

            Model.state = IncomingFileModel.State.Downloaded;
            PropertyChanged?.Invoke(this, new(nameof(DownloadedIconVisibility)));
            PropertyChanged?.Invoke(this, new(nameof(DownloadingIconVisibility)));
            PropertyChanged?.Invoke(this, new(nameof(ReceivedIconVisibility)));

            PropertyChanged?.Invoke(this, new(nameof(RemotePath)));
            return result == null ? null : localPath;
        }
    }
}
