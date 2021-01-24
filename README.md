## Proxy Audio Driver

A HAL virtual audio driver for macOS that sends all output to another audio device. It's main purpose is to make it possible to use macOS's system volume controls such as the volume menu bar icon or volume keyboard keys to change the volume of external audio interfaces that don't allow it. It might be useful for something else too.

### Installation

#### Manual installation
1. Download the latest release from this GitHub repository

2. Create the directory `HAL` if it does not exist. Open a terminal window, execute the following command and enter your administrator password when prompted.

        sudo mkdir /Library/Audio/Plug-Ins/HAL

3. Move the directory `ProxyAudioDriver.driver` to `/Library/Audio/Plug-Ins/HAL`. Execute in the root directory of the unzipped file.

        sudo mv ./ProxyAudioDevice.driver /Library/Audio/Plug-Ins/HAL/

4. Either reboot your system or reboot Core Audio by executing the following command.

        sudo launchctl kickstart -k system/com.apple.audio.coreaudiod

5. Run Proxy Audio Device Settings to configure the proxy output device's name, which output device the driver will proxy to, and how large you want its audio buffer to be.

#### Install with macports
1. Install proxy-audio-device

        sudo port install proxy-audio-device

2. Run Proxy Audio Device Settings app to configure your new audio device.

### Building

Clone the repo, open the Xcode project and build the driver and the settings application. Then follow the above installation instructions to install it.


### Issues

If you make the audio buffer too small then the driver will introduce pops, crackles, or distortion. If you notice that then try increasing the buffer size.


### Possible Future Work

- Indicator in the settings app for when the proxy audio device overruns its buffer and causes audio artifacts
- Proxying more than two channels of audio
- Ability to increase the number of proxy devices
