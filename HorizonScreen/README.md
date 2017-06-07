# HorizonScreen
slave application for HorizonM's screen streaming feature

`HorizonScreen` is licensed under the `MIT` license. See `LICENSE` for details.


## Current features
* framebuffer rendering received from HorizonM


## Building (Linux)
- install `gcc` and `g++` 5.x
- compile and install `libSDL2-dev` **2.0.4+**
- compile and install libjpeg-turbo
- `make`

## Building (Linux -> Windows 32bit)
- install `i686-w64-mingw32-gcc` and `i686-w64-mingw32-g++`
- `bash mkwin.sh`

## Building (Windows, 32bit only)
- install TDM-GCC and msys (the devkitPro one will suffice)
- compile and install libjpeg-turbo (force 32bit!)
- `make`

## Building (macOS)
- install GNU find and alias it as `find`
- install SDL2 development binaries
- compile and install libjpeg-turbo
- `make`

## Getting started
- Get the IP address of your 3DS
- Launch HorizonM
- Open a console/terminal/command interpreter
  - Linux and macOS: `out/PC/HorizonScreen-PC.elf 10.0.0.103`
  - Windows: `HzScreen 10.0.0.103`
  - replace `10.0.0.103` with your 3DS's actual IP address
- If HorizonScreen freezes, terminate it either by pressing `CTRL-C`/`STRG-C` in the terminal, or send the application a `SIGTERM` or `SIGKILL` (Linux and macOS), or close the terminal window.


###### ~~And if you order now, you'll get a `SIGSEGV` or `SIGFPT` for FREE!~~
                                                          