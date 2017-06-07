# HorizonM
utility background process for the Nintendo 3DS (codename "Horizon")

`HorizonM` is licensed under the `GNU GPLv3` license. See `LICENSE` for details.


## Current features
* screen streaming from 3DS using HorizonScreen
* VRAM corruptor (hold `ZL`+`ZR`)


## Credits
- Minnow - figuring out how Base processes can be used
- ihaveamac - pointing me towards the right direction for fixing memory allocation on new3DS and getting HorizonScreen to compile on macOS
- Stary - help with WinSockets in HorizonScreen
- flamerds - betatesting
- 916253 - betatesting
- NekoWasHere @ reddit - betatesting
- JayLine_ @ reddit - betatesting

## Building
- install devkitARM r45 (bundled with [devkitPro](http://devkitpro.org))
- install [ctrulib#5725ec2dedfffb2ee721dd8cddb3d24c288f452f](https://github.com/smealum/ctrulib/tree/5725ec2dedfffb2ee721dd8cddb3d24c288f452f)
- install zlib and libjpeg-turbo from [3DS portlibs](https://raw.githubusercontent.com/devkitPro/3ds_portlibs/master/Makefile)
- `make`

See other directories for more info about them.

## Getting started
> //TODO usage

- Install `HorizonM.cia` using an application manager  
  - For end-users [FBI](https://github.com/Steveice10/FBI) is recommended (optionally with [sockfile.py](#))  
  - For contributors and developers [socks](https://github.com/MarcuzD/socks) is recommended for faster testing debugging

- Install `HzLoad.cia` or `HzLoad_HIMEM.cia` (for high memory mode on old3DS systems) using an application manager
  - Look in the nested `HzLoad` project directory for compiling instructions

- Open `HzLoad` or `HzLoad_HIMEM` and wait for the RGB LED to turn light blue
  - Look at the [troubleshooting](#tshoot) section for help if the console softlocks on a black screen or the RGB LED has a color other than light blue after the launcher exits
  - Look in the nested `HzLoad` project directory for `HzLoad`-specific issues and troubleshooting steps

- Press and hold `START`+`SELECT` until the RGB LED turns off to exit HorizonM
  - Look at the [troubleshooting](#troubleshooting) section for help if the the RGB LED didn't turn off after a small amount of time

## Troubleshooting

##### RGB LED color codes
- off -> off ??? - failed to start HorizonM at all
  - old3DS: not enough memory
  - general: HorizonM is not even installed
  - contrib/dev: check if rsf doesn't contain invalid entries
- off -> static red - HorizonM failed to enter the main loop, it means that it softlocked or crashed during initialization
  - contrib/dev: check rsf permissions
- anything -> static white - a C++ exception occurred (most likely memory allocation failure)
- anything -> bright yellow - waiting for wifi availability
- bright yellow -> flashing white and yellow - failed to reinitialize networking (due to a program bug or a failed race condition)
- anything -> blinking dark yellow - `hangmacro()`, indicates a fatal error that didn't crash the process
- light blue -> rapid flashing black and red - failed to create the network thread (out of resources)
- light blue -> rapid flashing white and red - failed to accept new connections because we ran out of memory (memory leak)
- light blue -> green - network thread started, initializing stuff
- green && pink blink - connection estabilished with HorizonScreen
- green -> flashing pink&yellow - disconnected, waiting for cleanup
- flashing pink and yellow -> light blue - successfully disconnected from HorizonScreen
- anything -> dark blue - HorizonM failed to finalize services, it means that it softlocked or crashed while finalizing services
  - contrib/dev: `SOCU_ShutdownSockets()` may have failed or blocked the main thread

###### ~~And if you order now, you'll get 2 data aborts for the price of one!~~
