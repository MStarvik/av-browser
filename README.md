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

## Troubleshooting
If a resource fails to open, it may be due to a preferred application not being set for that resource type, or the preferred application not being able to handle HTTP URLs. Try running av-browser with the environment variable `G_MESSAGES_DEBUG=av-browser` to see the MIME type of the resource and the preferred application for that MIME type.
