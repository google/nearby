using System.ComponentModel;
using System.Threading.Tasks;
using System.Windows;

namespace HelloCloudWpf {
    public class OutgoingFileModel {
        public readonly string fileName;
        public string? url = null;
        public bool isUploading = false;

        public OutgoingFileModel(string fileName) {
            this.fileName = fileName;
        }
    }

    public class OutgoingFileViewModel : INotifyPropertyChanged, IViewModel<OutgoingFileModel> {
        public event PropertyChangedEventHandler? PropertyChanged;

        public string FileName { get => Model!.fileName; }
        public string? Url { get => Model!.url; }
        public bool IsUploaded { get => !string.IsNullOrEmpty(Url); }
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

        public async Task<bool> Upload() {
            Model!.isUploading = true;
            PropertyChanged?.Invoke(this, new (nameof(UploadedIconVisibility)));
            PropertyChanged?.Invoke(this, new (nameof(NotUploadedIconVisibility)));
            PropertyChanged?.Invoke(this, new (nameof(UploadingIconVisibility)));

            await Task.Delay(1000);
            Model!.url = "http://foo.bar/awesome.jpg";
            Model!.isUploading = false;
            PropertyChanged?.Invoke(this, new (nameof(UploadedIconVisibility)));
            PropertyChanged?.Invoke(this, new (nameof(NotUploadedIconVisibility)));
            PropertyChanged?.Invoke(this, new (nameof(UploadingIconVisibility)));

            PropertyChanged?.Invoke(this, new (nameof(Url)));
            return true;
        }
    }
}