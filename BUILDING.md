# Building your driver

**Note**: Ready-to-use driver builds are provided in the [/deployable](/deployable) folder in this repository, but if you would prefer to build the driver yourself instructions are documented below.

## Getting Started - Building

### Requirements
* Ensure that you have **cmake** installed (along with the path variable set): https://cmake.org/download/
* Install the `C++ CMake tools for Windows` for Visual Studio:
  * Modify Visual Studio in Visual Studio installer

### Generate the project files
1. Navigate into the project folder.
   ```sh
   $ cd etee-OpenVR-Driver
   ```
2. Make a build directory and change your working directory.
   ```sh
   $ mkdir build
   $ cd build
   ```
* Run CMake. This should generate Visual Studio project files in the `build/` folder, which you can then compile.
  ```sh
  $ cmake ..
  ```

## Building with Visual Studio Build Tools

You can run a cmake build in the `build/` directory with the following commands:

To build a **Release** version (recommended):
  ```sh
  $ cmake --build . --config Release
  ```

To build a **Debug** version (includes more event printouts for easier development):
  ```sh
  $ cmake --build . --config Debug
  ```

The artifacts of the build will be outputted to `build/Debug/`, or `build/Release/` depending on build configuration. To add the driver build to SteamVR, follow the steps described in [Step 3: How to add the driver](./README.md#add-driver).

## Building & Debugging with Visual Studio IDE

### Building in Visual Studio
1. Open the Visual Studio project (.sln file) in the `build/` directory.
2. You should already have the ability to build the driver by pressing `Ctrl + Shift + B`.

**Note**: The artifacts of the build will be outputted to `build/Debug/`, or `build/Release/` depending on build configuration

### Setup the Debugger
1. Install the Microsoft Process Debugging Tool from [here](https://marketplace.visualstudio.com/items?itemName=vsdbgplat.MicrosoftChildProcessDebuggingPowerTool).
2. Navigate to `Debug > Other Debug Targets > Child Process Debug Settings`.
   * Check `Enable child process debugging`.
   * On the first row (with the process name `<All other processes>`, make sure that the `Action` is set to `Do not debug`.  
   * Add a new row (double-click on the empty `Process name` underneath `<All other processes>`).  
   * Add `vrserver.exe` as the process name.
   * Ensure that `Action` is set to `Attach Debugger`.  

### Launch SteamVR when building through Visual Studio
It's usually quite useful to build then automatically launch SteamVR for debugging purposes.

To launch SteamVR for debugging:  
1. Click on the arrow next to `Local Windows Debugger`
2. Select `ALL_BUILD Debug Properties`

<p align="center">
  <img width="350" src="./wiki/img/Screenshots/Setup/vs-code-debugger-1.png">
  <br/>
  <em> ALL_BUILD Debug Properties</em>
</p>

3. Navigate to the `Debugger` Property (under Configuration Properties)
4. Set `Command` to the location of `vrstartup.exe`:
     * This is usually located at `C:\Program Files (x86)\Steam\steamapps\common\SteamVR\bin\win64\vrstartup.exe`

  <br/>

<p align="center">
  <img width="900" src="./wiki/img/Screenshots/Setup/vs-code-debugger-2.png">
  <br/>
  <em> Debugging Configuration Properties </em>
</p>
