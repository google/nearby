using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Net;
using System.Runtime.InteropServices;
using System.Security.RightsManagement;
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
using static hello_cloud_wpf.NearbyConnections;

namespace hello_cloud_wpf
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

        IntPtr router = IntPtr.Zero;
        IntPtr core = IntPtr.Zero;

        public ObservableDictionary<string, string> Endpoints { get; }

        public MainWindow()
        {
            Endpoints = new ObservableDictionary<string, string>();
            InitializeComponent();

            AllocConsole();

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

            BooleanMediumSelector mediumSelector = new()
            {
                ble = true,
                bluetooth = true,
                web_rtc = true,
                wifi_direct = true,
                wifi_hotspot = true,
                wifi_lan = true,
            };

            NearbyConnections.DiscoveryOptions discoveryOptions = new()
            {
                strategy = NearbyConnections.kP2pCluster,
                allowed = mediumSelector,
                auto_upgrade_bandwidth = true,
                enforce_topology_constraints = true,
                is_out_of_band_connection = false,
                low_power = true,
            };


            EndpointFoundCallback endpointFoundCallback = this.OnEndpointFound;
            EndpointLostCallback endpointLostCallback = this.OnEndpointLost;
            EndpointDistanceChangedCallback endpointDistanceChangedCallback = this.OnEndpointDistanceChanged;
            ResultCallback discoveryStartedCallback = this.OnDiscoveryStarted;

            // Allocate memory these delegates so that they don't get GCed.
            // TODO: free up the memory in the destructor.
            GCHandle.Alloc(endpointFoundCallback);
            GCHandle.Alloc(endpointLostCallback);
            GCHandle.Alloc(endpointDistanceChangedCallback);
            GCHandle.Alloc(discoveryStartedCallback);

            NearbyConnections.StartDiscovery(
                this.core,
                "com.google.location.nearby.apps.helloconnections",
                discoveryOptions,
                endpointFoundCallback,
                endpointLostCallback,
                endpointDistanceChangedCallback,
                discoveryStartedCallback);
        }

        void OnDiscoveryStarted(NearbyConnections.Status status)
        {
            System.Console.WriteLine("OnDiscoveryStarted: " + status.ToString());
        }

        void AddEndpointOnUIThread(string id, string name)
        {
            Action<string, string> addMethod = Endpoints.Add;
            Application.Current.Dispatcher.BeginInvoke((string id, string name) => Endpoints.Add(id, name), id, name);
        }

        void RemoveEndpointOnUIThread(string id)
        {
            Application.Current.Dispatcher.BeginInvoke((string id) => Endpoints.Remove(id), id);
        }

        void OnEndpointFound(string endpointId, byte[] endpointInfo, int size, string serviceId)
        {
            var _ = size;
            string endpointName = Encoding.UTF8.GetString(endpointInfo);
            System.Console.WriteLine("OnEndPointFound: ");
            System.Console.WriteLine("  endpoint_id: " + endpointId);
            System.Console.WriteLine("  endpoint_info: " + endpointName);
            System.Console.WriteLine("  service_id: " + serviceId);

            AddEndpointOnUIThread(endpointId, endpointName);
        }

        void OnEndpointLost(string endpointId)
        {
            System.Console.WriteLine("OnEndpointLost: " + endpointId);
            RemoveEndpointOnUIThread(endpointId);
        }

        void OnEndpointDistanceChanged(string endpointId, NearbyConnections.DistanceInfo distanceInfo)
        {
            System.Console.WriteLine("OnEndpointDistanceChanged: ");
            System.Console.WriteLine("  endpoint_id: " + endpointId);
            System.Console.WriteLine("  distance_info: " + distanceInfo);
        }

        void OnInitiated(string endpointId, NearbyConnections.ConnectionResponseInfo connectionResponseInfo)
        {
            System.Console.WriteLine("OnInitiated: ");
            System.Console.WriteLine("  endpoint_id: " + endpointId);
        }
    }
}