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
