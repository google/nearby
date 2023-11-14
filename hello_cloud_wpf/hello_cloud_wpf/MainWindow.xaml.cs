using System.Runtime.InteropServices;
using System.Windows;

namespace HelloCloudWpf
{
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
            AllocConsole();
            InitializeComponent();
        }

        private void isAdvertising_Checked(object sender, RoutedEventArgs e) {
            (DataContext as MainWindowViewModel).StartAdvertising();
        }

        private void isAdvertising_Unchecked(object sender, RoutedEventArgs e) {
            (DataContext as MainWindowViewModel).StopAdvertising();
        }

        private void isDiscovering_Checked(object sender, RoutedEventArgs e) {
            (DataContext as MainWindowViewModel).StartDiscovering();
        }

        private void isDiscovering_Unchecked(object sender, RoutedEventArgs e) {
            (DataContext as MainWindowViewModel).StopDiscovering();
        }
    }
}