# Image Annotation Picker

A CMake-based C++ application using Qt6 Community Edition to help go through video frame by frame and pick & choose what images to save for dataset creation.

## Features

- **Video Playback**: Load and play various video formats (MP4, AVI, MOV, MKV, WMV, FLV, WebM)
- **Frame Navigation**: Step through videos frame by frame with precise control
- **Image Capture**: Save specific frames as images for annotation datasets
- **Multiple Formats**: Export frames in PNG, JPEG, BMP, or TIFF formats
- **Batch Operations**: Select multiple frames and export them all at once
- **User-friendly Interface**: Intuitive Qt-based GUI with video preview and frame management

## Quick Start

1. **Install Qt6 Community Edition** from https://www.qt.io/download-open-source
2. **Build the project**:
   ```bash
   mkdir build && cd build
   cmake ..
   cmake --build .
   ```
3. **Run the application**:
   ```bash
   ./bin/ImageAnnotationPicker
   ```

## Requirements

- Qt6 with Multimedia components (Community Edition or higher)
- CMake 3.16 or higher
- C++17 compatible compiler

For detailed setup instructions, see [SETUP.md](SETUP.md)

## Project Structure

```
src/
├── main.cpp           # Application entry point
├── MainWindow.h       # Main window header
└── MainWindow.cpp     # Main window implementation
```

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

### Third-Party Licenses

This software uses the following third-party libraries:

- **Qt6 Community Edition**: Licensed under GNU Lesser General Public License (LGPL) v3
  - Website: https://www.qt.io/
  - License: https://www.gnu.org/licenses/lgpl-3.0.html
  - Qt is dynamically linked and not modified

- **spdlog**: Licensed under MIT License
  - Repository: https://github.com/gabime/spdlog
  - License: MIT (included in third_party/spdlog)

## Legal Notice

This application uses Qt Community Edition under the terms of the GNU Lesser General Public License (LGPL) version 3. The Qt libraries are dynamically linked to this application and are not modified. Users are free to replace the Qt libraries with compatible versions in accordance with the LGPL v3 license terms.

Qt and the Qt logo are trademarks of The Qt Company Ltd. and/or its subsidiaries.
