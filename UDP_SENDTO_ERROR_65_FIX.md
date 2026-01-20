# Fix for UDP sendto Error 65 (EHOSTUNREACH) on macOS

## Problem
When trying to forward OSC data over UDP (e.g., to QLab5), users may encounter the error:
```
sendto failed with error 65
```

This error (EHOSTUNREACH on macOS/BSD) occurs when:
1. UDP output is attempted while TCP connections exist on the same port
2. The UDP socket lacks proper socket options for port reuse

## Root Cause
The `EosUdpOut` socket initialization in EosSyncLib did not set the `SO_REUSEPORT` and `SO_REUSEADDR` socket options. On macOS/BSD systems, these options are required when:
- Multiple sockets (e.g., TCP + UDP) need to use the same port
- Sending to localhost/loopback interfaces with ambiguous routing

## Solution
Added socket options to UDP output initialization in both macOS and Windows implementations:

### macOS (`EosUdp_Mac.cpp`)
```cpp
// Set SO_REUSEPORT to allow UDP socket to coexist with TCP on same port
int optval = 1;
setsockopt(m_Socket, SOL_SOCKET, SO_REUSEPORT, ...);

// Set SO_REUSEADDR for additional compatibility  
setsockopt(m_Socket, SOL_SOCKET, SO_REUSEADDR, ...);
```

Note: The `int optval = 1;` declaration was moved to the beginning of the socket initialization block since it's now used by multiple socket options (both the new REUSEPORT/REUSEADDR options and the existing BROADCAST option).

### Windows (`EosUdp_Win.cpp`)
```cpp
// Set SO_REUSEADDR to allow UDP socket to coexist with TCP on same port
int optval = 1;
setsockopt(m_Socket, SOL_SOCKET, SO_REUSEADDR, ...);
```

## Files Modified
- `EosSyncLib/EosSyncLib/EosUdp_Mac.cpp` - Added SO_REUSEPORT and SO_REUSEADDR
- `EosSyncLib/EosSyncLib/EosUdp_Win.cpp` - Added SO_REUSEADDR

## Testing
To verify the fix:
1. Set up a route with UDP input on a port (e.g., 53000)
2. Set up a TCP connection on the same port
3. Configure UDP output to send OSC data (e.g., to QLab5 at 127.0.0.1:53001)
4. Verify that UDP packets are sent successfully without error 65
5. Check logs for "socket initialized" instead of "sendto failed with error 65"

## Implementation Note
Since EosSyncLib is a separate repository (https://github.com/ElectronicTheatreControlsLabs/EosSyncLib), this fix requires:
1. Updating EosSyncLib with the socket option changes
2. Rebuilding OSCRouter with the updated EosSyncLib

### Applying the Patch
The patch file `eossynclib-udp-fix.patch` is included in this repository. To apply it:

```bash
cd /path/to/EosSyncLib
git apply /path/to/OSCRouter/eossynclib-udp-fix.patch
cd /path/to/OSCRouter
cmake --build build
```

Alternatively, if the patch has already been merged into the EosSyncLib repository, simply update your EosSyncLib submodule:

```bash
cd /path/to/OSCRouter
git submodule update --remote
cmake --build build
```

## Related Issue
This addresses the issue where UDP forwarding to QLab5 fails with error 65 when TCP connections are active on the same port.
