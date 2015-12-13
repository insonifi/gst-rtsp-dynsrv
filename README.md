# gst-rtsp-dynsrv
Dynamically configured GStreamer RTSP Servers.

Builds and mounts a GStreamer pipeline provided in RTSP URI path. For example:
```
rtsp://localhost:8554/videotestsrc`!`jpegenc`!`rtpjpeg`name=pay0`pt=96
```
Which represents a _launch string_:
```
videotestsrc ! jpegenc ! rtpjpeg name=pay0 pt=96
```
## Usage
1. Run server
```
> rtsp-dynsrv
rtsp://127.0.0.1:8554/
```
2. Open RTSP URI that represents GStreamer launch string, e.g.:
```
ffplay 'rtsp://localhost:8554/videotestsrc`!`jpegenc`!`rtpjpeg`name=pay0`pt=96'
```
Append query parameter `?mcast` to allow UDP multicast as transport protocol.
3. You should see a test pattern in your player.
### Parameters
Append `--help` to see applicable parameters.

## Compiling
All you need is run:
```
./autogen.sh
make
```
### Dependecies
- gstreamer >=1.0
- gstreamer-rtsp-server >=1.0

# Credits
Thanks to great GStreamer project and [gst-template](http://cgit.freedesktop.org/gstreamer/gst-template/).
