using Google.Apis.Upload;
using Google.Cloud.Storage.V1;
using System;
using System.ComponentModel;
using System.IO;
using System.Threading.Tasks;
using System.Windows;

namespace HelloCloudWpf {
    public class OutgoingFileModel {
        public readonly string localPath;
        public string? remotePath = null;
        public bool isUploading = false;

        public OutgoingFileModel(string localPath) {
            this.localPath = localPath;
        }
    }

    public class OutgoingFileViewModel : 
        INotifyPropertyChanged, 
        IViewModel<OutgoingFileModel>,
        IProgress<IUploadProgress> {
        public event PropertyChangedEventHandler? PropertyChanged;

        public string LocalPath { get => Model!.localPath; }
        public string? RemotePath { get => Model!.remotePath; }
        public bool IsUploaded { get => !string.IsNullOrEmpty(RemotePath); }
        public bool IsUploading { get => Model!.isUploading; }

        public Visibility UploadedIconVisibility { 
            get => IsUploaded ? Visibility.Visible : Visibility.Hidden; 
        }
        public Visibility UploadingIconVisibility { 
            get => IsUploading ? Visibility.Visible : Visibility.Hidden; 
        }
        public Visibility NotUploadedIconVisibility { 
            get => IsUploaded || IsUploading ? Visibility.Hidden : Visibility.Visible; 
        }

        public OutgoingFileModel? Model { get; set; }

        public OutgoingFileViewModel() { }

        public async Task<string?> Upload(StorageClient client) {
            MainViewModel.Instance.Log("Begin uploading " + LocalPath);

            Model!.isUploading = true;
            PropertyChanged?.Invoke(this, new (nameof(UploadedIconVisibility)));
            PropertyChanged?.Invoke(this, new (nameof(NotUploadedIconVisibility)));
            PropertyChanged?.Invoke(this, new (nameof(UploadingIconVisibility)));
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
            } else {
                MainViewModel.Instance.Log("Finished uploading " + LocalPath + ", failed.");
            }

            Model!.isUploading = false;
            PropertyChanged?.Invoke(this, new (nameof(UploadedIconVisibility)));
            PropertyChanged?.Invoke(this, new (nameof(NotUploadedIconVisibility)));
            PropertyChanged?.Invoke(this, new (nameof(UploadingIconVisibility)));

            PropertyChanged?.Invoke(this, new (nameof(RemotePath)));
            EndpointViewModel.UpdateCanExecute();
            return result == null? null : fileName;
        }

        public void Report(IUploadProgress value) {
        }
    }
}