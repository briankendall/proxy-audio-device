## Proxy Audio Driver

A HAL virtual audio driver for macOS that sends all output to another audio device. It's main purpose is to make it possible to use macOS's system volume controls such as the volume menu bar icon or volume keyboard keys to change the volume of external audio interfaces that don't allow it. It might be useful for something else too.

### Installation

1. Download the latest release from this GitHub repository

2. Copy ProxyAudioDriver.driver to: /Library/Audio/Plug-Ins/HAL

3. Either reboot your system or reboot Core Audio by opening a terminal window and executing this command and entering your administrator password when prompted:

        sudo launchctl kickstart -k system/com.apple.audio.coreaudiod

4. Run Proxy Audio Device Settings to configure the proxy output device's name, which output device the driver will proxy to, and how large you want its audio buffer to be.


### Building

Clone the repo, open the Xcode project and build the driver and the settings application. Then follow the above installation instructions to install it.


### Issues

If you make the audio buffer too small then the driver will introduce pops, crackles, or distortion. If you notice that then try increasing the buffer size.


### Possible Future Work

- Indicator in the settings app for when the proxy audio device overruns its buffer and causes audio artifacts
- Proxying more than two channels of audio
- Ability to increase the number of proxy devices
