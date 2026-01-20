Simple UDP/TCP packet router for Mac and Windows, with optional OSC specific options

⭐ Now with support for sACN, ArtNet, PosiStageNet (PSN), MIDI, and Javascript

🌐 **NEW**: Modern responsive web interface with JSON API on port 8081

![OSCRouter](https://github.com/user-attachments/assets/3255fbe8-712d-4797-b0c0-1d313a51a6c9)


## Web Interface

OSCRouter now includes a built-in web server that provides:
- **Real-time status monitoring** - View server status and statistics
- **Live logging** - Monitor logs in real-time with auto-refresh
- **Configuration display** - View current routes, connections, and settings
- **JSON API** - Programmatic access to status, config, and logs

Access the web interface at `http://localhost:8081` when OSCRouter is running.

### API Endpoints

- `GET /` - Web dashboard (HTML)
- `GET /api/status` - Current status and statistics (JSON)
- `GET /api/config` - Current configuration (JSON)
- `GET /api/logs` - Recent log messages (JSON)


## TCP Connections

OSCRouter supports both TCP client and TCP server modes for OSC communication.

### TCP Server Mode
To receive OSC over TCP using Packet Length framing:
1. **TCP Tab**: Add a connection with Mode="Server", Framing="OSC 1.0" (4-byte packet length header), and specify IP/Port
2. **Routing Tab**: Create routes with the **same port number** to forward incoming packets

The TCP server listens for incoming connections. When a client connects, packets received on that connection are processed against the routing table. **Important**: The TCP tab only sets up the connection - you must create routes in the Routing tab to forward packets.

### TCP Client Mode
To send OSC over TCP:
1. **TCP Tab**: Add a connection with Mode="Client", Framing="OSC 1.0", and specify the server IP/Port
2. **Routing Tab**: Create routes with the destination IP/Port matching the TCP client connection

When a route's destination matches a TCP client connection, packets are sent over TCP instead of UDP.

### Framing Options
- **OSC 1.0**: Packets framed by 4-byte packet size header (Packet Length framing)
- **OSC 1.1**: Packets framed by SLIP (RFC 1055)

## Multiple Outputs per Input

You can route one incoming source to multiple outputs, each with different path transformations:

1. Create multiple routes with the **same incoming** IP/Port/Path
2. Each route can have a **different outgoing** destination and path

**Path Remapping Examples:**
- Use `%1`, `%2`, `%3`, etc. to reference segments of the incoming OSC path (1-indexed)
- Input: `/eos/out/event/cue/1/25/fire` (segments: 1=eos, 2=out, 3=event, 4=cue, 5=1, 6=25, 7=fire)
  - Path: `/cue/%6/start` → Output: `/cue/25/start` (uses segment 6 = "25")
- Remap path to argument: `/cue/25/start` → Path: `/eos/cue/fire=%2` → Output: `/eos/cue/fire, 25(i)`

## Example File (pictured above)

[example.osc.txt](https://github.com/user-attachments/files/24332375/example.osc.txt)


## About this ETCLabs Project
OSCRouter is open-source software (developed by a combination of end users and ETC employees in their free time) designed to interact with Electronic Theatre Controls products. This is not official ETC software. For challenges using, integrating, compiling, or modifying this software, we encourage posting on the [Issues](https://github.com/ElectronicTheatreControlsLabs/OSCRouter/issues) page of this project. ETC Support is not familiar with this software and will not be able to assist if issues arise. (Hopefully issues won't happen, and you'll have a lot of fun with these tools and toys!)

We also welcome pull requests for bug fixes and feature additions.


# Documentation

[Eos Manual: OSC](https://www.etcconnect.com/WebDocs/Controls/EosFamilyOnlineHelp/en/Content/23_Show_Control/08_OSC/OPEN_SOUND_CONTROL.htm)


# Dependencies

- Requires [EosSyncLib](https://github.com/ElectronicTheatreControlsLabs/EosSyncLib)
- Requires [Qt](https://www.qt.io/)


# Download

[Download Now For Mac or Windows](https://github.com/ElectronicTheatreControlsLabs/OSCRouter/releases/)
