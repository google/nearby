# Nearby

![Nearby Logo](https://upload.wikimedia.org/wikipedia/commons/thumb/2/2f/Google_Nearby_Logo.svg/1200px-Google_Nearby_Logo.svg.png)

**Nearby** is a collection of projects focused on **connectivity** that enable building **cross-device experiences**.

✨ This is not an officially supported Google product, but an open-source effort to empower developers to create smarter, more connected apps.

---

## 🚀 How It Works

Nearby enables **peer-to-peer communication** between devices using Bluetooth, Wi-Fi, and ultrasonic audio.  
It automatically discovers devices nearby and lets you exchange messages, files, or even extend app functionality across devices.

### 🔄 Device-to-Device Exchange

![Nearby Device Exchange](https://media.giphy.com/media/v1.Y2lkPTc5MGI3NjExZmxidHQwY2prYjNoa2tscDRtY2NsNjd4Z2d4aWF5cTRuN21iYTR2eSZlcD12MV9naWZzX3NlYXJjaCZjdD1n/3o6nUR4Zb9FW3WqgXu/giphy.gif)

---

## 📂 Projects

### [Nearby Connections](connections/)
A **peer-to-peer networking API** that allows apps to easily discover, connect to, and exchange data with nearby devices in real-time, regardless of network connectivity.

### [Nearby Presence](presence/)
An **extension to Nearby Connections** that features:
- Extensible identity model for authentication and restricted visibility  
- Resource management for system health  
- Proximity detection through sensor fusion  

### [Nearby for Embedded Systems](embedded/)
A **lightweight implementation** of Fast Pair intended for **embedded systems**.

---

## 🛠 Getting Started

Follow these steps to build and run Nearby samples:

1. Install [Android Studio](https://developer.android.com/studio).
2. Clone this repository:
   ```bash
   git clone https://github.com/google/nearby.git
   cd nearby
   ```
3. Open the project in Android Studio.
4. Select a sample module (e.g., `connections/sample`).
5. Build & run on **two physical Android devices** (emulators may not support Bluetooth/Wi-Fi).

---

## 💻 Example Usage

Here’s how to advertise a service using **Nearby Connections**:

```java
Nearby.getConnectionsClient(context)
    .startAdvertising(
        "MyApp",
        SERVICE_ID,
        connectionLifecycleCallback,
        new AdvertisingOptions.Builder().setStrategy(Strategy.P2P_CLUSTER).build()
    );
```

And here’s how to discover and connect:

```java
Nearby.getConnectionsClient(context)
    .startDiscovery(
        SERVICE_ID,
        endpointDiscoveryCallback,
        new DiscoveryOptions.Builder().setStrategy(Strategy.P2P_CLUSTER).build()
    );
```

---

## 🌟 Use Cases

- 📱 **Cross-device file sharing** (like Nearby Share / AirDrop)  
- 🎮 **Local multiplayer games** without internet  
- 🔧 **IoT device control** (smart devices communication)  
- 📂 **Distributed storage prototype** → If a device lacks storage, it can temporarily store files on a nearby device (experimental concept).  

---

## 🤝 Contributing

We encourage you to contribute to Nearby!  
Here’s how you can help:

1. Fork this repo 🍴  
2. Create a branch: `git checkout -b feature-new-docs`  
3. Commit your changes: `git commit -m "Added setup guide"`  
4. Push to your fork and open a Pull Request  

👉 Check out the full [Contributing Guide](CONTRIBUTING.md).

---

## 📜 License

Nearby is released under the [Apache License 2.0](LICENSE).

---

🌐 *Let’s build the future of cross-device connectivity together!* 🚀
