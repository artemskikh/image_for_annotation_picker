# Qt Installation and Setup Instructions

## Installing Qt6 Community Edition

### macOS (using Homebrew)
```bash
brew install qt@6
```

### Alternative: Official Qt Installer
1. Visit https://www.qt.io/download-open-source
2. Download the Qt Online Installer
3. Run the installer and select Qt 6.x with the following components:
   - Qt 6.x.x Desktop (macOS)
   - Qt Multimedia
   - CMake integration
   - Qt Creator (optional but recommended)

## Building the Project

### Prerequisites
- CMake 3.16 or higher
- Qt6 with Multimedia components
- C++17 compatible compiler

### Build Steps

1. **Configure the project:**
   ```bash
   mkdir build
   cd build
   cmake ..
   ```

2. **Build the project:**
   ```bash
   cmake --build .
   ```

3. **Run the application:**
   ```bash
   ./bin/ImageAnnotationPicker
   ```

### Troubleshooting

If CMake can't find Qt6, you may need to set the Qt6_DIR variable:

```bash
cmake -DQt6_DIR=/path/to/qt6/lib/cmake/Qt6 ..
```

For Homebrew installation on macOS:
```bash
cmake -DQt6_DIR=/opt/homebrew/lib/cmake/Qt6 ..
```

## Project Structure

```
image_for_annotation_picker/
├── CMakeLists.txt          # Main CMake configuration
├── src/                    # Source code directory
│   ├── main.cpp           # Application entry point
│   ├── MainWindow.h       # Main window header
│   └── MainWindow.cpp     # Main window implementation
├── build/                 # Build output directory
└── README.md              # Project documentation
```

## Features

This application provides:
- Video playback controls (play/pause, frame navigation)
- Frame-by-frame video analysis
- Image capture and annotation selection
- Export functionality for selected frames
- Configurable output formats (PNG, JPEG, BMP, TIFF)

## Development

The project uses Qt6 with the following modules:
- Qt6::Core - Core Qt functionality
- Qt6::Widgets - GUI widgets
- Qt6::Multimedia - Video playback
- Qt6::MultimediaWidgets - Video display widgets

## Licensing

This project uses Qt6 Community Edition, which is licensed under the GNU Lesser General Public License (LGPL) v3. Key points:

- Qt libraries are dynamically linked (not statically linked or modified)
- You can distribute applications using Qt Community Edition
- Users can replace Qt libraries with compatible versions
- Full LGPL v3 license terms apply to Qt components

For commercial Qt licensing or if you need to statically link Qt, consider Qt Commercial License from The Qt Company.

**Important**: If you modify this application for distribution, ensure compliance with:
- LGPL v3 for Qt components
- MIT License for the application code
- Provide access to Qt source code as required by LGPL v3

More information:
- Qt Licensing: https://www.qt.io/licensing/
- LGPL v3 License: https://www.gnu.org/licenses/lgpl-3.0.html
