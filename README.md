# SAMUEL v1.0

SAMUEL is a tool for extracting assets from DOOM Eternal. 

It is cross-platform, open-source, and features an easy-to-use graphical interface.

# Usage:

1. Run SAMUEL.exe
2. Click the "Load Resource" button.
3. Select a file with the .resources or .resources.backup file extensions (these are in your DOOM Eternal "base" directory).
4. Wait for the program to load the resource. 
5. Select the file(s) you want to export.

Files will be extracted to the "exports" folder, which is in the same location as SAMUEL.exe.

# SAMUEL vs. VEGA

This will obviously get compared to VEGA (the other asset extraction tool), so its probably necessary to explain the differences here:

## Improvements:

1. SAMUEL is cross-platform and open-source. It works on Windows, Linux, MacOS.
2. Supports extracting of .decl files in bulk or individually.
3. Successfully extracts several types of textures that VEGA doesn't support.
4. Supports extracting non-streamed or modded textures - texture modders can now recover their source images.
5. Has improved search functionality - can search with multiple keywords separated by spaces.
6. Extracts "normals" in RG format (greenish color) used by DOOM Eternal, which should reduce steps needed to modify them.
7. Runs a bit faster than VEGA in general.

## Limitations:

SAMUEL is not yet capable of exporting models (.lwo/.md6 files) for use in Substance Painter or Blender. However, it is now possible to export these files in their *native* formats for the very first time. This is an important step and it makes it possible for us to start reverse-engineering the format. Once the model format is better understood, we can add export to .obj/.fbx features.

*Texture modders will generally prefer VEGA until we can replicate this feature.*

## Credits:

* SamPT ([@brongo](https://github.com/brongo)) - Main author/developer of the SAMUEL tool. First person to fully and publically reverse-engineer the .streamdb file format, .resources format, .tga file format, .lwo/.md6mesh header formats, and discover how to export the files, after many long hours of reverse-engineering work.
* [@Powerball253](https://github.com/PowerBall253) - Many significant contributions in the form of code & testing. Responsible for Linux/MacOS compatibility, packageMapSpec parser, and figuring out Qt multithreading. Also assisted with reverse-engineering of TGA header formats.
* [DOOM 2016+ Modding Discord](https://discord.gg/ymRvQaU) for support and testing, and encouraging future developments.


## License:

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with the SAMUEL program.  If not, see <https://www.gnu.org/licenses/>.


## Building from Source:

There is no need to build/compile this program from source code. Simply download and use the included binaries for Windows or Linux (under "Releases").

If you *want* to build/compile from source, you will need a copy of the [Qt development library](https://www.qt.io/). The SAMUEL program uses Qt for its cross-platform GUI features. Please note that usage of Qt is subject to a separate licensing agreement. SAMUEL uses Qt under the [Qt for Open-Source Development](https://www.qt.io/download-open-source). The Qt source code can be acquired here: https://www.qt.io/offline-installers.

SAMUEL for Windows is tested and compiled using a static build of Qt version 6.1.2.
