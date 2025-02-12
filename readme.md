# Spinnaker Camera Tracking System

A C++ application for high-performance camera tracking using FLIR's Spinnaker SDK with OpenGL visualization support.

## Features

- Real-time video capture from FLIR cameras
- OpenGL-based live preview
- Automatic frame rate management
- Binary video recording
- Frame ID tracking and backup
- Robust error recovery system
- GPIO line configuration
- JSON metadata export

## Prerequisites

### Required SDKs and Tools
- FLIR Spinnaker SDK (must be installed separately)
- C++17 compatible compiler
- CMake (recommended for building)

### Dependencies manageable through vcpkg
- OpenCV
- GLFW
- OpenGL
- nlohmann/json

Note: The FLIR Spinnaker SDK must be installed manually as it's a proprietary SDK and cannot be managed through vcpkg. After installing the Spinnaker SDK, you'll need to manually configure its properties in Visual Studio:

1. Right-click project → Properties
2. C/C++ → Additional Include Directories: Add path to Spinnaker SDK include folder
3. Linker → Additional Library Directories: Add path to Spinnaker SDK lib folder
4. Linker → Input → Additional Dependencies: Add Spinnaker_v140.lib (or appropriate version)
5. Ensure Spinnaker DLLs are in your system PATH or executable directory

## Supported Cameras

The system supports multiple FLIR camera models with the following configurations:

| Camera Number | Serial Number | Max FPS |
|--------------|---------------|---------|
| 1            | 22181614      | 170.0   |
| 2            | 20530175      | 170.0   |
| 3            | 24174008      | 170.0   |
| 4            | 24174020      | 170.0   |
| 5 (Colour)   | 23606054      | 170.0   |
| 6 (6.3MP)    | 21423798      | 59.60   |

## Usage

```bash
./tracker --id <mouse_id> --date <date_time> --path <output_path> --rig <camera_number> --fps <frame_rate> --windowWidth <width> --windowHeight <height>
```

### Command Line Arguments

- `--id`: Subject identifier (default: "NoID")
- `--date`: Recording date/time (format: YYMMDD_HHMMSS, default: current time)
- `--path`: Output directory path (default: "E:\test_vid_output")
- `--rig`: Camera number (1-6, default: 2)
- `--fps`: Frame rate (max depends on camera model)
- `--windowWidth`: Preview window width (default: 800)
- `--windowHeight`: Preview window height (default: 600)

## Output Files

The system generates several output files:

- `{date_time}_{mouse_id}_binary_video.bin`: Raw video data
- `{date_time}_{mouse_id}_frame_ids_backup.txt`: Frame ID tracking
- `{date_time}_{mouse_id}_Tracker_data.json`: Session metadata
- `rig_{camera_number}_camera_finished.signal`: Session completion signal

## Key Features

### Auto Recovery System
The system includes a robust recovery mechanism that:
- Attempts up to 3 recovery tries on camera errors
- Implements a 5-second cooldown between attempts
- Automatically resets camera settings during recovery
- Maintains data integrity during recovery process

### Camera Configuration
Automatic configuration of:
- Frame rate limits based on camera model
- GPIO Line 2 output setup
- Exposure time limits
- Acquisition mode settings

### Performance Optimization
- Buffered frame ID writing (200 frames buffer)
- Optimized display refresh rate (30 FPS default)
- Efficient binary video storage
- Memory-managed frame tracking

## Controls

- Press `ESC` to stop recording
- Close the preview window to end the session
- System also responds to external stop signals (`stop_camera_{number}.signal`)

## Error Handling

The system handles various error conditions:
- Camera initialization failures
- Frame capture errors
- File I/O issues
- Resource conflicts
- Memory management issues

## Building

### Windows
```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### Dependencies Installation

#### Spinnaker SDK Installation
1. Download the FLIR Spinnaker SDK from the FLIR website
2. Run the installer
3. Configure Visual Studio project properties manually as described in Prerequisites section

#### Other Dependencies
Install using vcpkg (recommended):
```bash
vcpkg install opencv:x64-windows
vcpkg install glfw3:x64-windows
vcpkg install nlohmann-json:x64-windows
```

These vcpkg-managed dependencies will be automatically configured in your project - no manual property setup required.

## License

[Add your license information here]

## Contributing

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add some amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## Authors

[Add author information here]

## Acknowledgments

- FLIR Systems for the Spinnaker SDK
- OpenCV team
- GLFW contributors
- nlohmann/json library maintainers