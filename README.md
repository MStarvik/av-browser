# av-browser
A simple browser for UPnP-AV/DLNA resources on a local network.

<p align="center">
  <img src="https://github.com/user-attachments/assets/f354c9b0-9dd8-4c9a-930f-743d9f540e89" width="450" alt="av-browser screenshot">
</p>

## Features
- Browse UPnP-AV/DLNA resources on a local network
- Open resources using the preferred application

## Caveats
- Requires a preferred application to be set for each resource type
- Requires the preferred application to handle HTTP URLs
- Does not support authentication

## Installation
### Dependencies
- gtk4
  - Arch: gtk4 (build/runtime)
  - Ubuntu 24.04: libgtk-4-dev (build), libgtk-4-1 (runtime)
- gupnp
  - Arch: gupnp (build/runtime)
  - Ubuntu 24.04: libgupnp-1.6-dev (build), libgupnp-1.6-0 (runtime)
- gupnp-av
  - Arch: gupnp-av (build/runtime)
  - Ubuntu 24.04: libgupnp-av-1.0-dev (build), libgupnp-av-1.0-3 (runtime)

### Building from source
```sh
git clone https://github.com/mstarvik/av-browser.git
cd av-browser
make
sudo mv av-browser /usr/local/bin # optional
sudo mv no.mstarvik.av-browser.desktop /usr/share/applications # optional
```

## Troubleshooting
If a resource fails to open, it may be due to a preferred application not being set for that resource type, or the preferred application not being able to handle HTTP URLs. Try running av-browser with the environment variable `G_MESSAGES_DEBUG=av-browser` to see the MIME type of the resource and the preferred application for that MIME type.
