Nearby
Nearby is a collection of projects focused on connectivity that enable building cross-device experiences.

✨ This is not an officially supported Google product, but an open-source effort to empower developers to create smarter, more connected apps.

🚀 How It Works
Nearby enables peer-to-peer communication between devices using Bluetooth, Wi-Fi, and ultrasonic audio.
It automatically discovers devices nearby and lets you exchange messages, files, or even extend app functionality across devices.

🔄 Device-to-Device Exchange
📂 Projects
Nearby Connections
A peer-to-peer networking API that allows apps to easily discover, connect to, and exchange data with nearby devices in real-time, regardless of network connectivity.

Nearby Presence
An extension to Nearby Connections that features:

Extensible identity model for authentication and restricted visibility

Resource management for system health

Proximity detection through sensor fusion

Nearby for Embedded Systems
A lightweight implementation of Fast Pair intended for embedded systems.

🛠 Getting Started
Follow these steps to build and run Nearby samples:

Install Android Studio.

Clone this repository:

Bash

git clone https://github.com/google/nearby.git
cd nearby
Open the project in Android Studio.

Select a sample module (e.g., connections/sample).

Build & run on two physical Android devices (emulators may not support Bluetooth/Wi-Fi).

💻 Example Usage
Here’s how to advertise a service using Nearby Connections:

Java

Nearby.getConnectionsClient(context)
    .startAdvertising(
        "MyApp",
        SERVICE_ID,
        connectionLifecycleCallback,
        new AdvertisingOptions.Builder().setStrategy(Strategy.P2P_CLUSTER).build()
    );
And here’s how to discover and connect:

Java

Nearby.getConnectionsClient(context)
    .startDiscovery(
        SERVICE_ID,
        endpointDiscoveryCallback,
        new DiscoveryOptions.Builder().setStrategy(Strategy.P2P_CLUSTER).build()
    );
🌟 Use Cases
📱 Cross-device file sharing (like Nearby Share / AirDrop)

🎮 Local multiplayer games without internet

🔧 IoT device control (smart devices communication)

📂 Distributed storage prototype → If a device lacks storage, it can temporarily store files on a nearby device (experimental concept).

🤝 Contributing
We encourage you to contribute to Nearby!
Here’s how you can help:

Fork this repo 🍴

Create a branch: git checkout -b feature-new-docs

Commit your changes: git commit -m "Added setup guide"

Push to your fork and open a Pull Request

👉 Check out the full Contributing Guide.

📜 License
Nearby is released under the Apache License 2.0.

🌐 Let’s build the future of cross-device connectivity together! 🚀
