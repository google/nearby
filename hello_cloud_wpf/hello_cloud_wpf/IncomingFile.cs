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

    public class IncomingFileViewModel : INotifyPropertyChanged, IViewModel<IncomingFileModel> {
        public event PropertyChangedEventHandler? PropertyChanged;

        public string FileName { get => Model!.fileName; }
        public string Url { get => Model!.url; }
        public bool IsDownloaded { get => Model!.isDownloaded; }
        public bool IsDownloading { get => Model!.isDownloading; }

        public Visibility DownloadedIconVisibility { get => IsDownloaded ? Visibility.Visible : Visibility.Hidden; }
        public Visibility DownloadingIconVisibility { get => IsDownloading ? Visibility.Visible : Visibility.Hidden; }
        public Visibility NotDownloadedIconVisibility { get => IsDownloaded || IsDownloading ? Visibility.Hidden : Visibility.Visible; }
        public IncomingFileModel? Model { get; set; }

        public IncomingFileViewModel() { }

        public async Task<bool> Download() {
            Model!.isDownloading = true;
            PropertyChanged?.Invoke(this, new(nameof(DownloadedIconVisibility)));
            PropertyChanged?.Invoke(this, new(nameof(NotDownloadedIconVisibility)));
            PropertyChanged?.Invoke(this, new(nameof(DownloadingIconVisibility)));

            await Task.Delay(1000);
            Model.isDownloading = false;
            Model.isDownloaded = true;
            PropertyChanged?.Invoke(this, new(nameof(DownloadedIconVisibility)));
            PropertyChanged?.Invoke(this, new(nameof(NotDownloadedIconVisibility)));
            PropertyChanged?.Invoke(this, new(nameof(DownloadingIconVisibility)));

            PropertyChanged?.Invoke(this, new(nameof(Url)));
            return true;
        }
    }
}
