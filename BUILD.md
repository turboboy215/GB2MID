## Linux Build Instructions

### 1. Prerequisites

To build this project on Linux, you will need `cmake`, a C compiler (aka `gcc`), and the zlib development headers.

Install the dependencies based on your distribution:

* **Debian / Ubuntu / Linux Mint:** `sudo apt install build-essential cmake zlib1g-dev`
* **Fedora / RHEL / Rocky Linux:** `sudo dnf install gcc cmake zlib-devel`
* **openSUSE:** `sudo zypper install gcc cmake zlib-devel`
* **Arch Linux / Manjaro:** `sudo pacman -S base-devel cmake zlib`

    *(For **CachyOS** users: CachyOS comes with zlib-ng installed, a zlib alternative. For zlib compatibility, `zlib-ng-compat` should already be installed. If it's not for some reason, you can install with `sudo pacman -S zlib-ng-compat`).*

### 2. Building from Source
Open your terminal in the root directory of the cloned repository. Run the following standard CMake commands to configure and compile the release binary:

```bash
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

### 3. Running the Program

Once the build is complete, the native gb2mid executable will be generated inside the build directory.

**Important**: The program requires the GAMELIST.XML database to be in the same directory as the executable to function properly.

Move the compiled binary back to the root directory where the XML file is located, and run it from there:

```bash
# Copy the binary to the root directory
cp gb2mid ..
cd ..

# Extract MIDI files from the Game Boy file
./gb2mid game.gb
```