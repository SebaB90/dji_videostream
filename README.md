# DJI Video Stream

A modified version of the [DJI Edge SDK](https://developer.dji.com/doc/edge-sdk-tutorial/en/) that enables streaming drone video to an HTTP endpoint for dashboard integration, with runtime camera source switching via shared memory.

## Overview

This repository extends the DJI Edge SDK with two main features:

1. **HTTP Video Streaming**: Stream decoded drone video frames to an HTTP endpoint (`http://localhost:8889/drone`) instead of displaying locally
2. **Dynamic Camera Source Switching**: Switch between camera sources (Wide, Zoom, IR) at runtime using shared memory, controllable from a Python script

## Architecture

```
┌─────────────────┐     ┌──────────────────────────┐     ┌─────────────────┐
│   DJI Drone     │────▶│   test_liveview (C++)    │────▶│  HTTP Endpoint  │
│  (Video Source) │     │  - FFmpeg decoder        │     │  localhost:8889 │
└─────────────────┘     │  - H.264 encoder         │     │     /drone      │
                        │  - MPEGTS streaming      │     └─────────────────┘
                        └──────────────────────────┘
                                    ▲
                                    │ Shared Memory
                                    │ (/my_shm)
                        ┌───────────┴───────────┐
                        │  video_selector.py    │
                        │  (Camera Source       │
                        │   Switcher)           │
                        └───────────────────────┘
```

## Features

### HTTP Video Streaming

The video stream is encoded using FFmpeg with low-latency settings:
- **Codec**: H.264
- **Container**: MPEGTS
- **Preset**: ultrafast
- **Tune**: zerolatency
- **Bitrate**: 2 Mbps

The stream is sent to `http://localhost:8889/drone` and can be consumed by any compatible HTTP media server or dashboard.

### Dynamic Camera Source Switching

A background thread monitors a POSIX shared memory location (`/my_shm`) for camera source change requests. This allows switching between:
- **1**: Wide lens
- **2**: Zoom lens  
- **3**: IR (Infrared) lens

The camera source can be changed at runtime without restarting the application.

## Components

### C++ Application (`Edge-SDK/examples/liveview/test_liveview_main.cc`)

The main application that:
- Initializes the DJI Edge SDK
- Decodes incoming H.264 video stream from the drone
- Re-encodes and streams video to the HTTP endpoint
- Monitors shared memory for camera source change commands

### Python Video Selector (`PythonVIdeoSelector/video_selector.py`)

A Python script that creates and manages the shared memory for camera source switching:
- Creates shared memory region `/my_shm`
- Accepts user input for camera source selection (1-3)
- Writes the selected value to shared memory
- Handles cleanup on exit

## Usage

### Prerequisites

- DJI Dock with connected aircraft
- Docker (recommended) or native Linux environment
- Dependencies: OpenCV, FFmpeg 4.x, libssh2, OpenSSL

### Running with Docker

1. Build the Docker image:
   ```bash
   cd docker
   ./build.sh
   ```

2. Run the container:
   ```bash
   ./run.sh
   ```

3. Start the video streaming application:
   ```bash
   ./test_liveview [CAMERA_TYPE] [QUALITY] [LENS]
   ```

   Parameters:
   - `CAMERA_TYPE`: 0 = FPV, 1 = Payload
   - `QUALITY`: 1 = 540p, 2 = 720p, 3 = 720pHigh, 4 = 1080p, 5 = 1080pHigh
   - `LENS` (optional): 1 = Wide, 2 = Zoom, 3 = IR

   Example:
   ```bash
   ./test_liveview 1 4 2  # Payload camera, 1080p, Zoom lens
   ```

### Switching Camera Source at Runtime

1. In a separate terminal, run the Python video selector:
   ```bash
   cd PythonVIdeoSelector
   python3 video_selector.py
   ```

2. Enter the camera source number when prompted:
   - `1` for Wide lens
   - `2` for Zoom lens
   - `3` for IR lens
   - `0` to exit

The C++ application will detect the change and switch the camera source automatically.

### Receiving the Video Stream

Your dashboard or media server should listen on `http://localhost:8889/drone` to receive the MPEGTS video stream.

## Building from Source

```bash
cd Edge-SDK
mkdir build && cd build
cmake ..
make
```

## Configuration

### Stream URL

To change the streaming endpoint, modify the URL in `Edge-SDK/examples/liveview/test_liveview_main.cc`:

```cpp
ImageProcessor::Options image_processor_option = {
    .name = std::string("httpstream"),
    .alias = camera,
    .userdata = g_liveview_sample,
    .url = std::string("http://localhost:8889/drone")  // Change this
};
```

### Shared Memory Name

Both the C++ and Python applications use `/my_shm` as the shared memory name. To change it:

- In `test_liveview_main.cc`: `constexpr const char* SHM_NAME = "/my_shm";`
- In `video_selector.py`: `SHM_NAME = "my_shm"`

## Project Structure

```
dji_videostream/
├── Edge-SDK/                    # DJI Edge SDK with modifications
│   ├── examples/
│   │   ├── common/
│   │   │   ├── image_processor_httpstream.cc   # HTTP streaming processor
│   │   │   └── image_processor_httpstream.h
│   │   └── liveview/
│   │       └── test_liveview_main.cc           # Main application
│   └── ...
├── PythonVIdeoSelector/
│   └── video_selector.py        # Camera source switcher
├── docker/                      # Docker configuration
└── README.md
```

## License

This project is based on the DJI Edge SDK which is MIT-licensed. See the [LICENSE](Edge-SDK/LICENSE) file for details.

## Acknowledgments

- [DJI Edge SDK](https://developer.dji.com/doc/edge-sdk-tutorial/en/) - The base SDK for DJI Dock edge computing
