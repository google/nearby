using Google.Cloud.Storage.V1;
using System.ComponentModel;
using System.IO;
using System.Threading.Tasks;
using System.Windows;

namespace HelloCloudWpf {
    public class IncomingFileModel {
        // the path of the file when it was uploaded, not necessarily
        // the path for downloading.
        public readonly string path;
        public readonly string remotePath;
        public bool isDownloading = false;
        public bool isDownloaded = false;

        public IncomingFileModel(string localPath, string remotePath) {
            this.path = localPath;
            this.remotePath = remotePath;
        }
    }

    public class IncomingFileViewModel : INotifyPropertyChanged, IViewModel<IncomingFileModel> {
        public event PropertyChangedEventHandler? PropertyChanged;

        public string Path { get => Model!.path; }
        public string RemotePath { get => Model!.remotePath; }
        public bool IsDownloaded { get => Model!.isDownloaded; }
        public bool IsDownloading { get => Model!.isDownloading; }

        public Visibility DownloadedIconVisibility { get => IsDownloaded ? Visibility.Visible : Visibility.Hidden; }
        public Visibility DownloadingIconVisibility { get => IsDownloading ? Visibility.Visible : Visibility.Hidden; }
        public Visibility NotDownloadedIconVisibility { get => IsDownloaded || IsDownloading ? Visibility.Hidden : Visibility.Visible; }
        public IncomingFileModel? Model { get; set; }

        public IncomingFileViewModel() { }

        public async Task<string?> Download(StorageClient storageClient) {
            MainViewModel.Instance.Log("Begin uploading " + RemotePath);

            Model!.isDownloading = true;
            PropertyChanged?.Invoke(this, new(nameof(DownloadedIconVisibility)));
            PropertyChanged?.Invoke(this, new(nameof(NotDownloadedIconVisibility)));
            PropertyChanged?.Invoke(this, new(nameof(DownloadingIconVisibility)));

            // Quick and dirty way to get a local path
            string localPath;
            int index = Path.LastIndexOf('/');
            if (index == -1) {
                index = Path.LastIndexOf("\\");
            }
            if (index == -1) {
                localPath = Path;
            } else {
                localPath = App.DocumentsPath + Path[(index + 1)..];
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
            return result == null ? null :localPath;
        }
    }
}
