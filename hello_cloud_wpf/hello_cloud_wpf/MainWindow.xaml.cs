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
    }
}