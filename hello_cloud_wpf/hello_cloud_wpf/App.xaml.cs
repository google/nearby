using System;
using System.Windows;

namespace HelloCloudWpf {
    /// <summary>
    /// Interaction logic for App.xaml
    /// </summary>
    public partial class App : Application
    {
        public static string GcsBucketName = "hello-cloud-5b73c.appspot.com";
        public static string DocumentsPath = Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments) + "\\";

        protected override void OnStartup(StartupEventArgs e) {
            string path = Environment.GetEnvironmentVariable("PATH") ?? "";
            Environment.SetEnvironmentVariable("PATH", path + @";C:\repo\nearby\bazel-bin\connections\dart");
            base.OnStartup(e);
        }
    }
}
