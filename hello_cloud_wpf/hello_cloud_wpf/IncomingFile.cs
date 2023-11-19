using System.ComponentModel;
using System.Threading.Tasks;
using System.Windows;

namespace HelloCloudWpf {
    public class IncomingFileModel {
        public readonly string fileName;
        public readonly string url;
        public bool isDownloading = false;
        public bool isDownloaded = false;

        public IncomingFileModel(string fileName, string url) {
            this.fileName = fileName;
            this.url = url;
        }
    }

    public class IncomingFileViewModel : INotifyPropertyChanged {
        public event PropertyChangedEventHandler? PropertyChanged;

        public string FileName { get => model.fileName; }
        public string Url { get => model.url; }
        public bool IsDownloaded { get => model.isDownloaded; }
        public bool IsDownloading { get => model.isDownloading; }

        public Visibility DownloadedIconVisibility { get => IsDownloaded ? Visibility.Visible : Visibility.Hidden; }
        public Visibility DownloadingIconVisibility { get => IsDownloading ? Visibility.Visible : Visibility.Hidden; }
        public Visibility NotDownloadedIconVisibility { get => IsDownloaded || IsDownloading ? Visibility.Hidden : Visibility.Visible; }

        private IncomingFileModel model;

        public IncomingFileViewModel(IncomingFileModel model) {
            this.model = model;
        }

        public async Task<bool> Download() {
            model.isDownloading = true;
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(DownloadedIconVisibility)));
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(NotDownloadedIconVisibility)));
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(DownloadingIconVisibility)));

            await Task.Delay(1000);
            model.isDownloading = false;
            model.isDownloaded = true;
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(DownloadedIconVisibility)));
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(NotDownloadedIconVisibility)));
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(DownloadingIconVisibility)));

            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(Url)));
            return true;
        }
    }
}
