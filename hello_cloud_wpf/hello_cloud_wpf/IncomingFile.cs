using Google.Cloud.Storage.V1;
using System.ComponentModel;
using System.IO;
using System.Threading.Tasks;
using System.Windows;

namespace HelloCloudWpf {
    public class IncomingFileModel {
        // the path of the file when it was uploaded, not necessarily the path for downloading.
        public readonly string localPath;
        public readonly string remotePath;
        public readonly long fileSize;

        public bool isDownloading = false;
        public bool isDownloaded = false;

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
        public bool IsDownloaded => Model!.isDownloaded;
        public bool IsDownloading => Model!.isDownloading;

        public Visibility DownloadedIconVisibility => IsDownloaded ? Visibility.Visible : Visibility.Hidden;
        public Visibility DownloadingIconVisibility => IsDownloading ? Visibility.Visible : Visibility.Hidden;
        public Visibility NotDownloadedIconVisibility => IsDownloaded || IsDownloading ? Visibility.Hidden : Visibility.Visible;
        public IncomingFileModel? Model { get; set; }

        public IncomingFileViewModel() { }

        public async Task<string?> Download(StorageClient storageClient) {
            MainViewModel.Instance.Log("Beginning downloading " + RemotePath);

            Model!.isDownloading = true;
            PropertyChanged?.Invoke(this, new(nameof(DownloadedIconVisibility)));
            PropertyChanged?.Invoke(this, new(nameof(NotDownloadedIconVisibility)));
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

            Model.isDownloading = false;
            Model.isDownloaded = true;
            PropertyChanged?.Invoke(this, new(nameof(DownloadedIconVisibility)));
            PropertyChanged?.Invoke(this, new(nameof(NotDownloadedIconVisibility)));
            PropertyChanged?.Invoke(this, new(nameof(DownloadingIconVisibility)));

            PropertyChanged?.Invoke(this, new(nameof(RemotePath)));
            return result == null ? null : localPath;
        }
    }
}
