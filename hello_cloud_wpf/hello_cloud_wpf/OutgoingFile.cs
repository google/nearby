using Google.Apis.Upload;
using Google.Cloud.Storage.V1;
using System;
using System.ComponentModel;
using System.IO;
using System.Threading.Tasks;
using System.Windows;

namespace HelloCloudWpf {
    public class OutgoingFileModel {
        public enum State {
            Picked, Uploading, Uploaded
        }

        public readonly string localPath;
        public readonly long fileSize;
        public string? remotePath = null;
        public State state = State.Picked;

        public OutgoingFileModel(string localPath, long fileSize) {
            this.localPath = localPath;
            this.fileSize = fileSize;
        }
    }

    public class OutgoingFileViewModel :
        INotifyPropertyChanged,
        IViewModel<OutgoingFileModel>,
        IProgress<IUploadProgress> {
        public event PropertyChangedEventHandler? PropertyChanged;

        public string LocalPath => Model!.localPath;
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

        public OutgoingFileViewModel() { }

        public async Task<string?> Upload(StorageClient client) {
            MainViewModel.Instance.Log("Beginning uploading " + LocalPath);

            Model!.state = OutgoingFileModel.State.Uploading;
            PropertyChanged?.Invoke(this, new(nameof(UploadedIconVisibility)));
            PropertyChanged?.Invoke(this, new(nameof(PickedIconVisibility)));
            PropertyChanged?.Invoke(this, new(nameof(UploadingIconVisibility)));
            EndpointViewModel.UpdateCanExecute();

            FileStream stream = File.Open(LocalPath, FileMode.Open);
            string fileName = Guid.NewGuid().ToString();
            var result = await client.UploadObjectAsync(App.GcsBucketName, fileName, null, stream, progress: this);
            stream.Close();
            if (result != null) {
                Model!.remotePath = fileName;
            }

            if (result != null) {
                MainViewModel.Instance.Log("Finished uploading " + LocalPath + " to " + RemotePath);
                Model!.state = OutgoingFileModel.State.Uploaded;
            } else {
                MainViewModel.Instance.Log("Finished uploading " + LocalPath + ", failed.");
                Model!.state = OutgoingFileModel.State.Picked;
            }

            PropertyChanged?.Invoke(this, new(nameof(UploadedIconVisibility)));
            PropertyChanged?.Invoke(this, new(nameof(PickedIconVisibility)));
            PropertyChanged?.Invoke(this, new(nameof(UploadingIconVisibility)));

            PropertyChanged?.Invoke(this, new(nameof(RemotePath)));
            EndpointViewModel.UpdateCanExecute();
            return result == null ? null : fileName;
        }

        public void Report(IUploadProgress value) {
        }
    }
}