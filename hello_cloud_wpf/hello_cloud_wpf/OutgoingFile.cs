using Google.Apis.Upload;
using Google.Cloud.Storage.V1;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Threading.Tasks;
using System.Windows;

namespace HelloCloudWpf {

    public class OutgoingFileModel {
        public enum State {
            Picked, Uploading, Uploaded
        }

        [JsonInclude] public readonly string mimeType;
        [JsonInclude] public readonly string fileName;
        [JsonInclude] public readonly long fileSize;
        [JsonInclude] public string remotePath = string.Empty;

        public State state = State.Picked;

        public OutgoingFileModel(string mimeType, string fileName, long fileSize) {
            this.mimeType = mimeType;
            this.fileName = fileName;
            this.fileSize = fileSize;
        }

        public static byte[] EncodeOutgoingFiles(IEnumerable<OutgoingFileModel> files) {
            string jsonString = JsonSerializer.Serialize(files);
            return Encoding.UTF8.GetBytes(jsonString);
        }
    }

    public class OutgoingFileViewModel :
        INotifyPropertyChanged,
        IViewModel<OutgoingFileModel>,
        IProgress<IUploadProgress> {
        public event PropertyChangedEventHandler? PropertyChanged;

        public string FileName => Model!.fileName;
        public string? RemotePath => Model!.remotePath;
        public long FileSize => Model!.fileSize;

        public Visibility UploadedIconVisibility {
            get => Model!.state == OutgoingFileModel.State.Uploaded ? Visibility.Visible : Visibility.Hidden;
        }
        public Visibility UploadingIconVisibility {
            get => Model!.state == OutgoingFileModel.State.Uploading ? Visibility.Visible : Visibility.Hidden;
        }
        public Visibility PickedIconVisibility {
            get => Model!.state == OutgoingFileModel.State.Picked ? Visibility.Visible : Visibility.Hidden;
        }

        public OutgoingFileModel? Model { get; set; }

        public OutgoingFileModel.State State => Model!.state;

        public string? LocalFilePath { get; set; }

        public OutgoingFileViewModel() { }

        public async Task<string?> Upload(StorageClient client) {
            MainViewModel.Instance.Log("Beginning uploading " + LocalFilePath);

            Model!.state = OutgoingFileModel.State.Uploading;
            PropertyChanged?.Invoke(this, new(nameof(UploadedIconVisibility)));
            PropertyChanged?.Invoke(this, new(nameof(PickedIconVisibility)));
            PropertyChanged?.Invoke(this, new(nameof(UploadingIconVisibility)));
            EndpointViewModel.UpdateCanExecute();

            FileStream stream = File.Open(LocalFilePath!, FileMode.Open);
            string remotePath = Guid.NewGuid().ToString().ToUpper() + Path.GetExtension(LocalFilePath!);

            var result = await client.UploadObjectAsync(App.GcsBucketName, remotePath, null, stream, progress: this);
            stream.Close();
            if (result != null) {
                Model!.remotePath = remotePath;
            }

            if (result != null) {
                MainViewModel.Instance.Log("Finished uploading " + LocalFilePath + " to " + RemotePath);
                Model!.state = OutgoingFileModel.State.Uploaded;
            } else {
                MainViewModel.Instance.Log("Finished uploading " + LocalFilePath + ", failed.");
                Model!.state = OutgoingFileModel.State.Picked;
            }

            PropertyChanged?.Invoke(this, new(nameof(UploadedIconVisibility)));
            PropertyChanged?.Invoke(this, new(nameof(PickedIconVisibility)));
            PropertyChanged?.Invoke(this, new(nameof(UploadingIconVisibility)));

            PropertyChanged?.Invoke(this, new(nameof(RemotePath)));
            EndpointViewModel.UpdateCanExecute();
            return result == null ? null : remotePath;
        }

        public void Report(IUploadProgress value) {
        }
    }
}