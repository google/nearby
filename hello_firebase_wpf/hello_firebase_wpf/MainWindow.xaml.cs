using Firebase.Storage;
using Google.Apis.Upload;
using Google.Cloud.Storage.V1;
using Microsoft.Win32;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
using System.Runtime.InteropServices;
using System.Security.AccessControl;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Input;

namespace HelloFirebaseWpf {
    public class RelayCommand : ICommand {
        readonly Action<object?> execute;
        readonly Predicate<object?> canExecute;

        public RelayCommand(Action<object?> execute, Predicate<object?> canExecute) {
            this.execute = execute ?? throw new ArgumentNullException(nameof(execute));
            this.canExecute = canExecute;
        }

        public bool CanExecute(object? parameter) {
            return canExecute == null || canExecute(parameter);
        }

        public event EventHandler? CanExecuteChanged {
            add { CommandManager.RequerySuggested += value; }
            remove { CommandManager.RequerySuggested -= value; }
        }

        public void Execute(object? parameter) { execute(parameter); }
    }

    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window, INotifyPropertyChanged, IProgress<IUploadProgress> {
        [DllImport("kernel32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        static extern bool AllocConsole();

        public event PropertyChangedEventHandler? PropertyChanged;
        public event EventHandler? CanExecuteChanged;

        public IEnumerable<string> PickedFileNames => openFileDialog.FileNames;
        public ICommand PickFilesCommand => pickFilesCommand;
        public ICommand UploadCommand => uploadCommand;

        private readonly ICommand pickFilesCommand;
        private readonly ICommand uploadCommand;
        private readonly OpenFileDialog openFileDialog;

        public MainWindow() {
            AllocConsole();

            openFileDialog = new() { Multiselect = true };
            pickFilesCommand = new RelayCommand(
                _ => PickFiles(),
                _ => true);
            uploadCommand = new RelayCommand(
                _ => UploadFiles(),
                _ => true);
            InitializeComponent();
        }

        private void PickFiles() {
            openFileDialog.FileName = string.Empty;
            bool? result = openFileDialog.ShowDialog();
            if (result == true) {
                Console.WriteLine(openFileDialog.FileName);
                PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(PickedFileNames)));
            }
        }

        private void UploadFiles1() {
            foreach (string file in PickedFileNames) {
                // Get any Stream - it can be FileStream, MemoryStream or any other type of Stream
                FileStream stream = File.Open(file, FileMode.Open);
                Guid guid = Guid.NewGuid();

                // Construct FirebaseStorage, path to where you want to upload the file and Put it there
                var storage = new FirebaseStorage("hello-cloud-5b73c.appspot.com")
                    .Child("transfer")
                    .Child(guid.ToString());

                var task = storage.PutAsync(stream);

                // Track progress of the upload
                task.Progress.ProgressChanged += (object? s, FirebaseStorageProgress e) => {
                    Console.WriteLine($"Progress: {e.Position}/{e.Length} %");
                };
                //string url = storage.GetDownloadUrlAsync().Result;
                //System.Console.WriteLine($"Download URL: {url}");
            }
            return;
        }

        private void UploadFiles() {
            // https://cloud.google.com/docs/authentication/provide-credentials-adc#local-dev
            StorageClient storage = StorageClient.Create();

            foreach (string file in PickedFileNames) {
                FileStream stream = File.Open(file, FileMode.Open);
                Guid guid = Guid.NewGuid();
                
                string bucketName = "hello-cloud-5b73c.appspot.com";
                string fileName = guid.ToString();
                
                storage.UploadObject(bucketName, fileName, null, stream, progress: this);
                //Task<object> task = storage.UploadObjectAsync(bucketName, fileName, null, stream);

                Console.WriteLine($"Uploaded {fileName}.");
            }

        }

        public void Report(IUploadProgress value) {
            Console.WriteLine("Bytes sent: " + value.BytesSent);
        }
    }
}
