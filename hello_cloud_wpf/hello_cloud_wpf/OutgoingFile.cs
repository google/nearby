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

    public class OutgoingFileViewModel : INotifyPropertyChanged {
        public event PropertyChangedEventHandler? PropertyChanged;

        public string FileName { get => model.fileName; }
        public string? Url { get => model.url; }
        public bool IsUploaded { get => !string.IsNullOrEmpty(Url); }
        public bool IsUploading { get => model.isUploading; }

        public Visibility UploadedIconVisibility { get => IsUploaded ? Visibility.Visible : Visibility.Hidden; }
        public Visibility UploadingIconVisibility { get => IsUploading ? Visibility.Visible : Visibility.Hidden; }
        public Visibility NotUploadedIconVisibility { get => IsUploaded || IsUploading ? Visibility.Hidden : Visibility.Visible; }

        private readonly OutgoingFileModel model;

        public OutgoingFileViewModel(OutgoingFileModel model) {
            this.model = model;
        }

        public async Task<bool> Upload() {
            model.isUploading = true;
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(UploadedIconVisibility)));
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(NotUploadedIconVisibility)));
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(UploadingIconVisibility)));

            await Task.Delay(1000);
            model.url = "http://foo.bar/awesome.jpg";
            model.isUploading = false;
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(UploadedIconVisibility)));
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(NotUploadedIconVisibility)));
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(UploadingIconVisibility)));

            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(Url)));
            return true;
        }
    }
}
