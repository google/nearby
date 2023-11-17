using System.Runtime.InteropServices;
using System.Windows;

namespace HelloCloudWpf {
    public struct EndpointEntry
    {
        public string id;
        public string name;
    }

    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        [DllImport("kernel32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        static extern bool AllocConsole();

        public MainWindow()
        {
            InitializeComponent();
        }

        private void IsAdvertisingChecked(object sender, RoutedEventArgs e) {
            (DataContext as MainWindowViewModel)?.StartAdvertising();
        }

        private void IsAdvertisingUnchecked(object sender, RoutedEventArgs e) {
            (DataContext as MainWindowViewModel)?.StopAdvertising();
        }

        private void IsDiscoveringChecked(object sender, RoutedEventArgs e) {
            (DataContext as MainWindowViewModel)?.StartDiscovering();
        }

        private void IsDiscoveringUnchecked(object sender, RoutedEventArgs e) {
            (DataContext as MainWindowViewModel)?.StopDiscovering();

        }
    }
}