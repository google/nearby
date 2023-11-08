using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;

namespace hello_cloud_wpf
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        IntPtr router = IntPtr.Zero;
        IntPtr core = IntPtr.Zero;

        public MainWindow()
        {
            InitializeComponent();

            this.endpoints.Items.Add("Apple");
            this.endpoints.Items.Add("Banana");
            this.endpoints.Items.Add("Watermelon");
            this.router = NearbyConnections.InitServiceControllerRouter();
            if (this.router == IntPtr.Zero)
            {
                throw new Exception("Failed: InitServiceControllerRouter");
            }

            this.core = NearbyConnections.InitCore(this.router);
            if (this.core == IntPtr.Zero)
            {
                throw new Exception("Failed: InitCore");
            }

            // Start Discovery
        }

        // End point discovered callback

    }
}