using Google.Cloud.Storage.V1;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Threading.Tasks;
using System.Windows;

namespace HelloCloudWpf {
    public class IncomingFileModel {
        public enum State {
            Received, Downloading, Downloaded
        }

        [JsonInclude] public readonly string mimeType;
        // the path of the file when it was uploaded, not necessarily the path for downloading.
        [JsonInclude] public readonly string fileName;
        [JsonInclude] public readonly string remotePath;
        [JsonInclude] public readonly long fileSize;

        public State state = State.Received;

        public IncomingFileModel(string mimeType, string fileName, string remotePath, long fileSize) {
            this.mimeType = mimeType;
            this.fileName = fileName;
            this.remotePath = remotePath;
            this.fileSize = fileSize;
        }

        public static IList<IncomingFileModel>? DecodeIncomingFiles(byte[] payload) {
            return JsonSerializer.Deserialize<List<IncomingFileModel>>(payload);
        }
    }

    public class IncomingFileViewModel : INotifyPropertyChanged, IViewModel<IncomingFileModel> {
        public event PropertyChangedEventHandler? PropertyChanged;

        public string FileName => Model!.fileName;
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

            string localPath = Path.Combine(App.DocumentsPath, FileName);
            if (File.Exists(localPath)) {
                string fileNameWithoutExt = Path.GetFileNameWithoutExtension(localPath);
                string extension = Path.GetExtension(localPath);

                for (int n = 1; true; n++) {
                    localPath = Path.Combine(App.DocumentsPath, fileNameWithoutExt + "(" + n.ToString() + ")" + extension);
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
