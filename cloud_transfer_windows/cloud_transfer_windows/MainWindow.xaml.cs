using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Controls.Primitives;
using Microsoft.UI.Xaml.Data;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Navigation;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using Windows.Foundation;
using Windows.Foundation.Collections;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace cloud_transfer_windows
{
    /// <summary>
    /// An empty window that can be used on its own or navigated to within a Frame.
    /// </summary>
    public sealed partial class MainWindow : Window
    {
        IntPtr router = IntPtr.Zero;
        IntPtr core = IntPtr.Zero;

        public MainWindow()
        {
            this.InitializeComponent();

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
        }

        public void On_MainWindow_Closed(Object sender, WindowEventArgs args)
        {
            System.Console.WriteLine(args.ToString());

            if (this.core != IntPtr.Zero)
            {
                NearbyConnections.CloseCore(this.core);
            }
        }
    }
}
