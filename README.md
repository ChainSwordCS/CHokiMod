# ChirunoMod

A continuation of HzMod, built for Chokistream.

A utility background process for the Nintendo 3DS, purpose-built for screen-streaming over WiFi.

`ChirunoMod` is licensed under the `GNU GPLv3` license. See the [License](#license) section of this file for details, and see the file `LICENSE` for more details including the full terms of the license.

## Current features
* screen streaming from 3DS to PC using the Chokistream client
* Hold the Y button while launching the loader to boot HzMod instead

## Getting started

Get the latest stable release of [Chokistream](https://github.com/Eiim/Chokistream/releases) to run on PC.

Please use the [latest stable release](https://github.com/ChainSwordCS/ChirunoMod/releases) versions of `ChirunoMod.cia` and `ChLoad.cia`.

On 3DS, use FBI to install the `.cia` files...

Install `ChLoad.cia`, which is the loader.

If you are using an original 3DS, 3DS XL, or 2DS, which are otherwise referred to as "Old 3DS" systems,
if you're going to use ChirunoMod while playing games which use the High/Extended Memory Mode (such as Super Smash Bros. or Pokemon Sun/Moon,) then also install `ChLoad_HIMEM.cia`.

#### Development builds
For posterity, development "nightly" builds are available in the repo. Note that nightly builds are usually less stable, and may not function as intended. 
- [`ChirunoMod.cia` (nightly)](ChirunoMod.cia)
- [`ChLoad.cia` (dev/nightly)](HzLoad/ChLoad.cia)
- [`ChLoad_HIMEM.cia` (dev/nightly)](HzLoad/ChLoad_HIMEM.cia)


## ChirunoMod RGB LED color codes

### Status Codes
- light-blue / teal = connected to wifi, waiting for Chokistream client to connect
- green = connected to client
- yellow = waiting to connect to wifi...
    - dev: sometimes this indicates when certain games switch the 3DS into Local Wireless (WLAN) mode.
- flashing yellow + magenta = stopping secondary thread, disconnecting...

### Error Codes
- flashing yellow + black = fatal error. if possible, hold Start and Select to shutdown ChirunoMod.
    - dev: main() function encountered an issue and panicked (hangmacro). check HzLog.log for details.
- flashing red + green = stuck (or crashed) in netfuncTestFramebuffer() function.
    - dev: encountered an issue trying to obtain process ID of foreground process (?)
- flashing red + white = misc. error
    - dev: possibly C++ exception; CPPCrashHandler(). check HzLog.log for details.
- flashing red + other color = failed to start secondary thread (out of resources)
- red = encountered an error near the start of the main() function
- dark blue = trying to shut down ChirunoMod (perhaps softlocked?)


## Credits

see (CREDITS.md)[CREDITS.md]


## Build Instructions

If you'd like to grab a nightly build, a CIA file that's updated nearly every commit is in the root directory of this repository. [ChirunoMod.cia](ChirunoMod.cia)

It's not recommended to set up a build environment and build the project manually, as it's a bit of a chore.

### Prerequisites / Build Dependencies

* git
* devkitPro stuff (latest versions of msys2 and cmake) - https://devkitpro.org/wiki/Getting_Started
* devkitARM r46 (2017.01.16) - https://wii.leseratte10.de/devkitPro/devkitARM/r46%20%282017%29/
* libctru 1.2.1 (2017.02.20) - https://wii.leseratte10.de/devkitPro/libctru/2017/
* in `/devkitPro/tools/bin/` or in PATH ...
    * makerom, [latest version](https://github.com/3DSGuy/Project_CTR/releases?q=makerom), in `/devkitPro/tools/bin/` or in PATH
    * bannertool, version [1.2.0](https://www.gamebrew.org/wiki/Bannertool_3DS) or [compatible fork](https://github.com/Epicpkmn11/bannertool), in `/devkitPro/tools/bin/` or in PATH
* in `/devkitPro/portlibs/armv6k/` ...
    * 3ds-zlib (aka libzip), any version <pre>//TODO: optional?</pre>
    * 3ds-libjpeg-turbo (aka libturbojpeg), either A or B detailed as follows:
        a. v??? (2016.??.??), from the 3ds_portlibs git repo, compiled with a compatible version of devkitARM, preferably devkitARM r46.
        b. latest version from devkitPro, compiled with a compatible version of devkitARM.

This project requires legacy versions of some dependencies to build and to function as intended. If using a later version of said dependencies, your mileage may vary.

#### pre-made downloads

// TODO: update this.

An archive containing these can be downloaded here (Windows only): <https://chainswordcs.com/dl/hzmod_dependencies_2017_v1.zip>

Installation (tailored to Windows users):
1. Contents of the `devkitarm-r46` folder should be moved to `C:/devkitPro/devkitARM` (Please copy or rename the up-to-date devkitARM folder so it can be restored if you need)
2. Contents of the `libctru-1.2.1` folder should be moved to `C:/devkitPro/libctru` (Same precaution as above)
3. The `portlibs` folder can be copied over and merged with the existing folder `C:/devkitPro/portlibs`. 2017 libctru/devkitARM uses "armv6k", whereas current libctru/devkitARM uses "3ds".

### Building

ChirunoMod

1. Clone the repository with `git clone https://github.com/ChainSwordCS/ChirunoMod.git`
2. If desired, change `#define` build flags at the top of `/soos/main.cpp` to enable verbose debug logging or disable debug logging altogether.
3. Run `make`.
4. The compiled file is `ChirunoMod.cia`. Copy that to the 3DS and install it using FBI or another cia installer homebrew.

ChLoad (HzLoad)

1. Clone the repository with `git clone https://github.com/ChainSwordCS/ChirunoMod.git`
2. Navigate to the `/HzLoad/` directory. Run `make` to compile `ChLoad.cia`, or `make HIMEM=1` to compile `ChLoad_HIMEM.cia`.

# License

Copyright (c) 2022-2024 ChainSwordCS (https://chainswordcs.com), (chainswordcs@gmail.com)

`ChirunoMod` is licensed under the `GNU GPLv3` license. See `LICENSE` for details.

`ChirunoMod` is based on `HzMod`.

`HzMod` code and project are licensed under `GNU GPLv3` license.

`HzMod` code and project are Copyright (c) 2017 Sono (https://github.com/SonoSooS), (https://gbatemp.net/members/sono.373734/).

The original `HzMod` project is sometimes otherwise referred to as `HorizonM`, `HzModHax`, `HorizonMod`, or `HorizonModule`. The `HzMod` project includes `HorizonScreen` and `HzLoad`.

**See `LICENSE` for details.**

###### ~~And if you order now, you'll get 2 data aborts for the price of one!~~
