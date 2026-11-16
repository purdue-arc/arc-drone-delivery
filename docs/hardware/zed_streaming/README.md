This is the streaming instructions to allow you to view the zed camera UDP stream over a network like in VLC.

# Streaming
## Access the jetson's terminal from your computer
First connect the jetson to your computer using its usbc port and then open serial monitor in vscode using the serial monitor vscode extension. it's by the terminal below when you have the extension installed.

Connect to the com port shown and enable terminal mode. type chars to see that you can seee the login prompt. login then with `dronedelivery` as the username and password.

## Connect the jetson to a network 
Next, both the jetson and the viewing computer need to be connected to the same LAN. you can and should use a personal hotspot like on windows or on your iphone to accomplish this.

run 
```
nmcli device wifi list
```
to view what networks the jetson sees. make sure you can see your network you want to connect to

then connect to the network using, filling in your username and password
```
nmcli device wifi connect "<SSID>" password "<PASSWORD>"
```

## Start the server
To start the media streaming of streaming the zed camera over udp to the viewing computer which will run and use vlc to view it, change the ip and run:

```
gst-launch-1.0 zedsrc stream-type=0 camera-resolution=3 ! video/x-raw, width=1280, height=720, framerate=15/1 ! videoconvert ! x264enc tune=zerolatency bitrate=1500 speed-preset=veryfast key-int-max=15 ! h264parse ! rtph264pay config-interval=1 pt=96 ! udpsink host=<IP OF VIEWING COMPUTER> port=5000 sync=false async=false
```


## Connect
then create/edit the values you see in `zed.sdp` or here:
```
v=0
o=- 0 0 IN IP4 <IP OF JETSON>
s=ZED H264 Stream
c=IN IP4 <IP OF VIEWING COMPUTER>
t=0 0
m=video 5000 RTP/AVP 96
a=rtpmap:96 H264/90000
a=framerate:15
```

then on the viewing computer, open vlc using the `zed.sdp` file using cmd prompt on windows for example by running
```
"C:\Program Files\VideoLAN\VLC\vlc.exe" zed.sdp --network-caching=125
```

# Future



want to run a mediamtx server


You ran into a critical GStreamer syntax error because you were missing the **`udpsink` element** before specifying the port. The GStreamer pipeline requires an element to handle the data before any properties (like `port` or `host`) can be applied.

The command you ran was essentially trying to link `rtph264pay` directly to a property (`port=5000`), which is not a valid element, causing the segmentation fault (`SIGSEGV`).

Here is the **corrected command** and an explanation of the GStreamer syntax.

---

## 🛑 Corrected GStreamer Command (Sender)

You must explicitly name the sink element (`udpsink`) and specify its required parameters (`host` and `port`).

This single command will stream the video to your local laptop for control (low latency) and to your Base Station for recording (via a forwarder):

```bash
gst-launch-1.0 zedsrc stream-type=0 camera-resolution=3 ! \
video/x-raw, width=1280, height=720, framerate=15/1 ! videoconvert ! \
x264enc tune=zerolatency bitrate=2000 speed-preset=veryfast key-int-max=15 ! \
tee name=t ! \
queue ! rtph264pay pt=96 ! udpsink host=<LAPTOP_IP> port=5000 sync=false t. ! \
queue ! rtph264pay pt=96 ! udpsink host=<BASE_STATION_IP> port=5001 sync=false

```

### Key Corrections Made:

1. **Added `udpsink`:**
* `... ! rtph264pay pt=96 ! port=5000 sync=false`  **WRONG**
* `... ! rtph264pay pt=96 ! udpsink host=<IP> port=5000 sync=false`  **CORRECT**


2. **Added `host`:** The `udpsink` requires a destination **`host`** IP address to know where to send the packets. You cannot omit this unless you are using multicast (a different configuration).
3. **Added `sync=false`:** This is crucial for low-latency streaming as it prevents the sink from waiting to synchronize with the clock, forcing it to drop frames rather than accumulate delay.

---

## 📝 Analysis of Your Example Command

The example command you provided from the ZED repo is a perfectly valid alternative, but it uses a slightly different structure:

```bash
split. ! queue ... ! autovideoconvert ! x264enc ... ! h264parse ! rtph264pay ... ! queue ! udpsink clients=127.0.0.1:5000 ...

```

### Key Takeaways from the Example:

* **`split.`** The `tee` element is named `split` in their example, so the branches are referred to as `split.`
* **`h264parse`:** This element ensures that the H.264 stream is correctly formatted and includes essential metadata (like SPS/PPS headers) before being packetized by `rtph264pay`. While `x264enc` often does this automatically when piping directly to `rtph264pay`, adding `h264parse` is a **good practice** for robustness and often solves issues with decoders failing to start.
* **`udpsink clients=...`:** Their example uses the **`clients`** property (a non-standard GStreamer property often used for multicast or to simplify point-to-point UDP streaming) instead of the standard `host` and `port` properties. It is better to stick to `host=<IP> port=<PORT>` for clearer unicast streaming.

If you incorporate the `h264parse` element into your corrected command, the pipeline becomes even more robust:

### 💪 Robust Two-Stream GStreamer Command

```bash
gst-launch-1.0 zedsrc stream-type=0 camera-resolution=3 ! \
video/x-raw, width=1280, height=720, framerate=15/1 ! videoconvert ! \
x264enc tune=zerolatency bitrate=2000 speed-preset=veryfast key-int-max=15 ! \
h264parse ! tee name=t ! \
queue ! rtph264pay pt=96 ! udpsink host=<LAPTOP_IP> port=5000 sync=false t. ! \
queue ! rtph264pay pt=96 ! udpsink host=<BASE_STATION_IP> port=5001 sync=false

```

Remember to replace `<LAPTOP_IP>` and `<BASE_STATION_IP>` with the correct local IP addresses in your Halo Wi-Fi network.

That's an excellent question that gets into the core difference between network routing techniques. For your specific drone scenario, **unicast is the correct choice**, but it's important to know why, and where multicast is used.

## 🎯 Unicast vs. Multicast for Performance

It is generally better to stick to **unicast** in your specific scenario for **simplicity and stability**, even though **multicast** can offer better **bandwidth efficiency** for one particular use case.

| Feature | Unicast (Point-to-Point) | Multicast (Point-to-Multipoint) |
| --- | --- | --- |
| **Routing** | Dedicated traffic path to *one* IP address. | Traffic sent once to a special *group* IP (e.g., 224.x.x.x). |
| **Client Control** | Each receiver is independent; easy to track and manage. | Receivers join/leave the group without notifying the sender. |
| **Network Load (Sender)** | Sender must send a separate stream for *every* receiver. | Sender sends **only one stream**, regardless of 1 or 1,000 receivers. |
| **Internet Use** | **Works everywhere** (can be tunneled or routed easily). | **Rarely works** over the public internet (ISPs filter it). |
| **Complexity** | Simple (standard routing). | Requires specialized router configuration (IGMP/PIM protocols). |
| **Recommendation** | **Best for your two distinct destinations** (Laptop/Server). | Best for local delivery to a large number of unknown viewers. |

### Why Unicast is Better for Your Scenario:

1. **Fixed Destinations:** You only have **two** known and distinct destinations (your Laptop and your Base Station). Sending two separate unicast streams using `tee` and two `udpsink` elements is straightforward, reliable, and uses minimal CPU overhead.
2. **Internet Reliability:** Multicast streams generally **cannot be routed across the public internet** because most backbone routers do not support it or filter it out. You must use unicast (or a unicast-wrapped protocol like SRT) to reach your remote server.
3. **Base Station Requirement:** You need your Base Station to receive its own stream so it can forward the video reliably over the internet. A dedicated unicast link to the Base Station makes this forwarding task clear and simple.

---

## 🌐 UDP and Unicast/Multicast

UDP (User Datagram Protocol) is often associated with both, but for different reasons:

* **UDP and Unicast:** Yes, UDP is commonly used for unicast streaming (like your low-latency link to the laptop) because it avoids the overhead of TCP handshakes and retransmissions, making it faster. The `udpsink host=<IP> port=<PORT>` configuration is the standard way to achieve this.
* **UDP and Multicast:** UDP is **required** for multicast streaming. Since multicast sends a single stream to a group, there can be no handshake or error correction (which would defeat the low-latency purpose). Therefore, multicast **must** use an unreliable, connectionless protocol like UDP underneath the RTP layer.

In summary, while multicast is incredibly efficient for broadcasting a single stream to many devices on a **local network**, **unicast is simpler, more stable, and necessary for connecting your Base Station to your remote server over the internet.** Sticking to unicast ensures your mission-critical recording backup functions correctly.

That's an excellent idea. Switching to a web-based client fundamentally changes your protocol requirements.

The short answer is: **Yes, real-time, ultra-low latency streaming in a web browser is possible, but it requires a different protocol than the raw UDP/RTP stream you're currently using.**

You should move away from direct UDP/RTP to a protocol specifically designed for web browsers, like **WebRTC**.

---

## 🌐 Web App vs. Dedicated Client

| Client Type | Recommended Protocol | Latency | Reliability | Notes |
| --- | --- | --- | --- | --- |
| **Dedicated Client (VLC/FFplay)** | **RTP/UDP** | **Lowest (sub-500ms)** | Low (glitches) | Best for drone *control*. Requires knowing the laptop's IP (unicast). |
| **Web Browser (Web App)** | **WebRTC** or **WebSockets/LL-HLS** | **Ultra-Low to Low (50ms - 2 seconds)** | Variable | Best for accessibility and quick viewing. Requires a media server. |

For your drone control link, where latency is critical, a **dedicated client using UDP** is generally the **absolute lowest latency** method.

However, if you prioritize accessibility (viewing from any browser), you should use a modern web-friendly protocol.

---

## 🚀 Ultra-Low Latency for Web Apps (WebRTC)

**WebRTC (Web Real-Time Communication)** is the gold standard for real-time, bidirectional communication in a browser (used by Zoom, Google Meet, etc.). It is the closest you'll get to real-time, often achieving **sub-100ms** latency.

To integrate your GStreamer video into a web app, you would need to modify your architecture to:

1. **Change the Jetson Output:** Instead of sending raw RTP/UDP directly to the laptop, the Jetson sends a single, reliable stream to an **Ultra-Low Latency Media Server** located at your Base Station.
2. **Use a Media Server:** The media server transcodes or repackages the incoming stream into a WebRTC format.
3. **Client-Server Setup:** The web browser client connects to the Media Server, which acts as the intermediary.

### 💡 P2P (WebRTC) is Technically Possible, but Complex

WebRTC is technically a **P2P protocol**, but in your drone-to-laptop scenario, you don't use it in a pure P2P sense.

* A **signaling server** is required to establish the connection (no actual video flows through it).
* **NAT/Firewall traversal** (STUN/TURN servers) is always required, making the setup much more complicated than a direct UDP or server-based link, especially when your drone is behind a Halow link.

**Recommendation:** Stick to a **server-based WebRTC solution** (like MediaMTX, which supports WebRTC, or a commercial solution) at your Base Station.

---

## 🛠️ Updated Dual-Stream Architecture (Web App)

To support the web app for monitoring and the SRT link for recording, your best setup remains a **tee** on the drone, feeding two distinct streams to your Base Station:

1. **Jetson Orin Nano (Sender) - New Command:**
Send **two separate unicast streams** to your Base Station, using different ports.
```bash
gst-launch-1.0 zedsrc stream-type=0 camera-resolution=3 ! \
video/x-raw, width=1280, height=720, framerate=15/1 ! videoconvert ! \
x264enc tune=zerolatency bitrate=2000 speed-preset=veryfast key-int-max=15 ! \
h264parse ! tee name=t ! \
queue ! rtph264pay pt=96 ! udpsink host=<BASE_STATION_IP> port=5000 sync=false t. ! \
queue ! rtph264pay pt=96 ! udpsink host=<BASE_STATION_IP> port=5001 sync=false

```


2. **Base Station (Server) - Two Services:**
* **Service A (Web App - Port 5000):** A **WebRTC Media Server (e.g., MediaMTX)** listens on port 5000, receives the raw RTP/UDP stream, and repackages it for the web browser client.
* **Service B (Recording - Port 5001):** A **SRT Forwarder (GStreamer)** listens on port 5001 and forwards the stream reliably to the remote server.



This decoupled design is robust: you only send one low-latency encoded stream from the drone, and the stationary Base Station handles the resource-intensive tasks of repackaging for the web and ensuring reliable backup.

Would you like the GStreamer command for the **Base Station WebRTC Forwarder** (using `rtspclientsink` into a WebRTC server, which is often the easiest path with software like MediaMTX)?

That's right, the previous command was sending two streams to the Base Station's IP, which is wasteful and incorrect for your goals. You are aiming for **three distinct destinations/goals** from one source.

Here is the **corrected GStreamer command** on the Jetson Orin Nano for your triple-output setup, followed by the explanation of why it works and the logic behind three destinations.

---

## 🚀 Triple-Stream GStreamer Command (Jetson Orin Nano)

This single command uses the `tee` element to split the encoded stream into three branches:

1. **Branch 1 (UDP 5000):** Direct to Laptop/VLC (Lowest Latency, Unreliable)
2. **Branch 2 (UDP 5001):** To Base Station Media Server (for Web App, Ultra-Low Latency)
3. **Branch 3 (UDP 5002):** To Base Station Recorder/Forwarder (for Remote Server Backup, Reliable)

```bash
gst-launch-1.0 zedsrc stream-type=0 camera-resolution=3 ! \
video/x-raw, width=1280, height=720, framerate=15/1 ! videoconvert ! \
x264enc tune=zerolatency bitrate=2000 speed-preset=veryfast key-int-max=15 ! \
h264parse ! tee name=t ! \
queue ! rtph264pay pt=96 ! udpsink host=<LAPTOP_IP> port=5000 sync=false t. ! \
queue ! rtph264pay pt=96 ! udpsink host=<BASE_STATION_IP> port=5001 sync=false t. ! \
queue ! rtph264pay pt=96 ! udpsink host=<BASE_STATION_IP> port=5002 sync=false

```

### Explanation of the `tee` Branches:

* **`t. ! queue ! rtph264pay pt=96 ! udpsink host=<LAPTOP_IP> port=5000 sync=false`**
* **Destination:** Your laptop's IP address.
* **Goal:** **Lowest latency video** via VLC/ffplay for drone control.


* **`t. ! queue ! rtph264pay pt=96 ! udpsink host=<BASE_STATION_IP> port=5001 sync=false`**
* **Destination:** The Base Station's IP address, listening on **Port 5001**.
* **Goal:** Feed the **WebRTC Media Server** (e.g., MediaMTX) running on the Base Station to serve the web browser client (aiming for ms latency).


* **`t. ! queue ! rtph264pay pt=96 ! udpsink host=<BASE_STATION_IP> port=5002 sync=false`**
* **Destination:** The Base Station's IP address, listening on **Port 5002**.
* **Goal:** Feed the **SRT Forwarder** (another process on the Base Station) to reliably send the stream to your remote recording server.



---

## ❓ Why Not Just Have 1 Stream to the Base Station?

It is technically possible to send **one stream** to the Base Station and then use **another `tee` element** on the Base Station to split that single stream into the Web App and the Recorder.

### Option A: Two Streams from Drone (Recommended)

* **Drone Load:** Encodes once, streams three times.
* **Base Station Load:** Two simple receiving processes (WebRTC Server and SRT Forwarder).
* **Decoupling:** **Crucially, the WebRTC server (Branch 2) and the SRT forwarder (Branch 3) have different, dedicated purposes.** By separating them on the drone, you ensure that if one process on the Base Station crashes or gets overloaded, it only affects that specific purpose.

### Option B: One Stream from Drone, `tee` on Base Station (Complex)

* **Drone Load:** Encodes once, streams two times (one to laptop, one to base station).
* **Base Station Load:** Requires a complex GStreamer command on the Base Station to receive the stream, then `tee` it, and then pipe one branch to the WebRTC server and the other to the SRT forwarder. This complicates the Base Station setup and potentially introduces more CPU usage there.

Since your drone is a highly mobile source on a potentially unstable wireless link (Halow Wi-Fi), it's far **more robust** to have the drone focus on only **encoding and simple UDP delivery**, and let the **stable, wired Base Station** handle the resource-heavy tasks of **protocol conversion** (for WebRTC) and **reliable forwarding** (for SRT).

Splitting the stream into two distinct inputs on the Base Station (Ports 5001 and 5002) is cleaner and more modular than trying to pipe a single GStreamer stream into two entirely different applications.

Yes, running **MediaMTX solely on the remote server** is definitely possible, and it simplifies your Base Station setup dramatically.

The trade-off, as you correctly guessed, is that it will result in **higher latency** for the WebRTC stream compared to having the WebRTC server (MediaMTX) run locally on the Base Station.

---

## ☁️ Server-Only MediaMTX: Latency & Accessibility

| Aspect | Server-Only MediaMTX Setup | Notes |
| --- | --- | --- |
| **Accessibility** | **Excellent.** Since the server is on the public internet, the WebRTC stream is accessible from any web browser globally (provided Cloudflare/security allows). | This achieves your **"accessible from anywhere"** goal easily. |
| **Latency Impact** | **Higher.** The video must travel drone  Base Station  Internet  Remote Server  Internet  Laptop/Browser. The **double trip over the public internet** adds the full round-trip time (RTT) latency (hundreds of milliseconds). | You might see latencies in the range of **300ms to 1.5 seconds**, depending on the geographical distance and the protocol used between the Base Station and the Server (SRT would keep it closer to 300ms). |
| **Recording** | **Perfect.** The video arrives at the server reliably via SRT/RTSP/TCP and is recorded directly, eliminating the need for a separate recorder. | This consolidates the **recording and WebRTC distribution** roles. |

### Conclusion on Server-Only:

This is the **most practical and simplest architecture** for global access and guaranteed recording. You must decide if the potentially higher latency (ms+) is acceptable for your real-time drone control. Since you already have a **direct UDP link for the laptop (lowest latency)**, the WebRTC stream's higher latency should be fine for general monitoring.

---

## 🛡️ NAT/Firewall Traversal and P2P

**Why is NAT/Firewall traversal more complicated, and what's involved with direct P2P?**

### P2P and NAT Complications

Direct Peer-to-Peer (P2P) connections (like those established by WebRTC) are complicated when one or both peers are behind a **Network Address Translator (NAT)**, which is true for nearly every device on a home or business network.

A NAT router translates a single public IP address into many private internal IPs. When a device behind a NAT (like your laptop or the drone) wants to receive an *incoming* connection, the router has no way to know which internal device the external traffic should go to, so it usually blocks it—a firewall function.

The process of dealing with NAT/Firewall is handled by a suite of protocols:

* **1. STUN (Session Traversal Utilities for NAT):**
* **Involved:** The client contacts a STUN server to learn its own **public IP address and port**. This is the first step in figuring out how the rest of the world sees you.


* **2. TURN (Traversal Using Relays around NAT):**
* **Involved:** If STUN fails (usually due to restrictive firewalls), TURN acts as a **public relay server**. The data is sent from the sender to the TURN server, and then the TURN server forwards it to the receiver. This method is reliable but **adds latency** (because of the extra hop) and **costs money** (because it uses a lot of bandwidth).


* **3. ICE (Interactive Connectivity Establishment):**
* **Involved:** This is the overall framework that tries various combinations of connections (direct, via STUN, via TURN) until a successful media path is found.



### The Complexity of Direct P2P for Your Drone:

For your drone setup, the complexity is high because:

1. The drone is on a **mobile Halow Wi-Fi** network, adding another layer of NAT.
2. Setting up the required **STUN/TURN/Signaling servers** adds significant infrastructure overhead that you currently don't have.
3. The Base Station approach (Base Station is the server) is simpler because you only need to configure port forwarding once on the Base Station's router, treating the Base Station as a standard server on a fixed IP.

Using **MediaMTX on the remote server** is the simplest solution because the server is already publicly routable, simplifying the WebRTC connection process significantly. You only need to deal with the Base Station  Server link, which should be reliable if you use **SRT/TCP**.


That's an insightful question about IPv6. It touches upon one of the main reasons IPv6 was developed: to eliminate the headaches of NAT and open up true end-to-end connectivity.

Here's a concise answer:

**No, not all routers in between need to support IPv6 for your devices to communicate via IPv6.**

The internet has been transitioning for years, and it uses **tunneling and translation mechanisms** to allow IPv6 traffic to pass over the existing IPv4-only portions of the network.

---

## 🌉 How IPv6 Works Across an IPv4 Internet

The concept that lets your IPv6 packets travel from the Base Station (with a public IPv4 address) to the remote server (also potentially behind an IPv4 structure) is **Tunneling**.

### 1. The Core Requirement: Dual-Stack Endpoints

The two most critical points that **must** support IPv6 are the **sender** (Jetson Orin Nano / Base Station gateway) and the **receiver** (Remote Server).

* **Dual-Stack:** In the transition phase, devices are typically **dual-stack**, meaning they speak both IPv4 and IPv6.

### 2. The Tunneling Mechanism

When an IPv6 packet needs to cross an IPv4-only section of the internet, the **edge router** (or the client itself) uses a tunneling mechanism:

1. The entire **IPv6 packet** (with its own header) is **encapsulated** (wrapped) inside an **IPv4 packet** (with an IPv4 header).
2. The IPv4 packet is then routed normally across the IPv4 internet.
3. When the packet reaches an IPv6-aware router (usually the endpoint or a gateway close to it), the IPv4 wrapper is **decapsulated** (removed), and the original IPv6 packet is delivered to the final destination.

This mechanism ensures that **intermediate routers** that only understand IPv4 can still forward the traffic.

### 3. The Big Win: Eliminating NAT

The primary benefit of using IPv6 for your remote server streaming is that it **eliminates the need for NAT** (Network Address Translation).

* With IPv6, your Base Station and Remote Server can have **globally routable public addresses** directly, removing the barrier that forced you to use complex protocols like Cloudflare Tunnels or rely on TCP for port forwarding and reliability.
* **Simpler WebRTC:** While WebRTC still uses NAT traversal protocols (STUN/TURN) as a fallback, having native IPv6 support makes establishing the direct connection much simpler and more reliable than struggling with IPv4 NAT.

## ⚠️ Caveats and Practicality

While technically possible, using native IPv6 for your deployment still depends on a few factors:

* **ISP Support:** Both the Base Station's ISP and the Remote Server's hosting provider must offer **native IPv6 connectivity**. This is becoming common but is not universal.
* **Firewall Configuration:** Since IPv6 removes NAT, your devices are directly exposed to the internet. You must **configure an explicit IPv6 firewall** on both the Base Station router and the Remote Server to only allow traffic on the ports you need (like 8554 for SRT/RTSP/WebRTC). If your router's IPv6 firewall is not well-developed, this can introduce a security risk.

In summary, IPv6 is an **excellent choice** for your remote server link because it cuts through the complexity of IPv4 NAT, but you still benefit most from using a **reliable transport protocol like SRT** over that connection to compensate for the *packet loss* that still occurs on any long-distance network.