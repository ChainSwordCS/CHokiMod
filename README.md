# ChirunoMod

A continuation of HzMod, built for Chokistream.

A utility background process for the Nintendo 3DS, purpose-built for screen-streaming over WiFi.

`ChirunoMod` is licensed under the `GNU GPLv3` license. See the [License](#license) section of this file for details, and see the file `LICENSE` for more details including the full terms of the license.

## Current features
* screen streaming from 3DS to PC using the Chokistream client

## Getting started

Get the latest stable release of [Chokistream](https://github.com/Eiim/Chokistream/releases) to run on PC.

On 3DS, use FBI to install the `.cia` files...

Install [`ChLoad.cia`](HzLoad/ChLoad.cia) which is the loader. If you are using an Old-3DS (not a "New" 3DS/XL or "New" 2DS XL) and you're going to play games which use the High Memory Mode (Super Smash Bros, etc.) then also install [`ChLoad_HIMEM.cia`](HzLoad/ChLoad_HIMEM.cia).

#### For the main ChirunoMod application, you have a few options:
- The [latest Stable release](https://github.com/ChainSwordCS/ChirunoMod/releases) is strongly recommended, and specifically `ChirunoMod.cia`.
- There are two alternate cias:
  - `ChirunoMod_nodebug.cia` omits all debug logging for a tiny performance boost and slightly smaller size. This isn't recommended because the performance improvement is basically unnoticeable.
  - `ChirunoMod_verbosedebug.cia` has much more debug logging and records frame-time statistics in the PC-side log, but it generally runs a bit slower.
- Nightly builds can be found in the root directory of this repository, namely [`ChirunoMod.cia`](ChirunoMod.cia). Nightly builds are usually less stable, and may not function as intended. Note: for the most part, only `ChirunoMod.cia` is compiled on a regular basis.

###### Please note, the PC HorizonScreen application in this repository is old and obsolete. Additionally, all release versions of ChirunoMod are strictly incompatible with HorizonScreen.

> //TODO the rest of the readme

## Credits
- ### ChirunoMod Credits
  - ChainSwordCS - C/C++ programming, code comments, logo design.
  - Eiim - Assistance with code logic and commenting, assistance with logo design.
  - herronjo - Assistance with code logic and commenting.
  - savvychipmunk57 - Assistance with code logic and commenting.
  - bol0gna - Assistance with code logic and commenting.
  
- ### Chokistream Credits (Indirect help with this project)
  - Eiim - Documentation and reverse-engineering of HzMod, development of Chokistream
  - herronjo - Documentation and reverse-engineering of HzMod, development of Chokistream
  - ChainSwordCS - Documentation and reverse-engineering of HzMod, development of Chokistream
  
- ### Original Code
  - Sono - for creating the HzMod project and the code from which ChirunoMod was forked.
  
  - #### Additional credits for original HorizonM / HzMod
    - Minnow - figuring out how Base processes can be used
    - ihaveamac - pointing me towards the right direction for fixing memory allocation on new3DS and getting HorizonScreen to compile on macOS
    - Stary - help with WinSockets in HorizonScreen
    - flamerds - betatesting
    - 916253 - betatesting
    - NekoWasHere @ Reddit - betatesting
    - JayLine_ @ Reddit - betatesting

- ### ChirunoMod Logo assets
  - Cell9 - [NTR rocket graphic](https://github.com/44670/BootNTR/blob/master/resources/icon.png)
  - Mister Man (The Spriters Resource) - Mario sprites from Super Mario World
  - WordedPuppet (The Spriters Resource) - Cirno sprites from "Touhou Kaeizuka (Phantasmagoria of Flower View)"
  - gabrielwoj (The Spriters Resource) - Projectile sprites from "Touhou Koumakyou (The Embodiment of Scarlet Devil)"
  - Saigyou R. (The Spriters Resource) - Stage backgrounds from "Touhou Fuujinroku (Mountain of Faith)" (IIRC the Final Stage background with the moon was used)
  - Ryan914 and MaidenTREE (The Spriters Resource) - Extra Stage background from "Touhou Gensoukyou (Lotus Land Story)" (Pixel stars were used)

## Build Instructions

If you'd like to grab a nightly build, a CIA file that's updated nearly every commit is in the root directory of this repository. [ChirunoMod.cia](ChirunoMod.cia)

It's not recommended to set up a build environment and build the project manually. Really you should only do it if you're contributing code.

### Prerequisites

* git
* [makerom](https://github.com/3DSGuy/Project_CTR/releases?q=makerom) in PATH
* Legacy versions of devkitARM and libctru (detailed below)
* Libraries: 3ds-libjpeg-turbo and 3ds-zlib (should be automatically handled by devkitPro)

Currently, all active branches need to be compiled with a legacy version of the libctru library and the devkitARM toolchain. Specifically:
* devkitARM r46
* libctru 1.2.1

An archive containing these can be downloaded here (Windows only): <https://chainswordcs.com/dl/hzmod_dependencies_2017_v1.zip>

Installation (tailored to Windows users):
1. Contents of the `devkitarm-r46` folder should be moved to `C:devkitPro:devkitARM` (Please copy or rename the up-to-date devkitARM folder so it can be restored if you need)
2. Contents of the `libctru-1.2.1` folder should be moved to `C:devkitPro:libctru` (Same precaution as above)
3. The `portlibs` folder can be copied over and merged with the existing folder `C:devkitPro:portlibs`. 2017 libctru/devkitARM uses "armv6k", whereas current libctru/devkitARM uses "3ds".

Alternatively, archives of libctru can be found here: <https://wii.leseratte10.de/devkitPro/libctru/2017/>

### Building

ChirunoMod

1. Clone the repository with `git clone https://github.com/ChainSwordCS/ChirunoMod.git`
2. If desired, change debug variables at the top of `/soos/main.cpp` to enable verbose debug logging or disable debug logging altogether.
3. Run `make`.
4. The compiled file is `ChirunoMod.cia`. Copy that to the 3DS and install it using FBI or another cia installer homebrew.

ChLoad (HzLoad)

1. Switch to the current versions of devkitARM and libctru.
2. Navigate to the `/HzLoad/` directory. Run `make` to compile `ChLoad.cia`, or `make HIMEM=1` to compile `ChLoad_HIMEM.cia`.

# License

Copyright (c) 2022-2023 ChainSwordCS (https://chainswordcs.com), (chainswordcs@gmail.com)<br>
`ChirunoMod` is licensed under the `GNU GPLv3` license. See `LICENSE` for details.

`HzMod` code and project are licensed under `GNU GPLv3` license.<br>
`HzMod` code and project are Copyright (c) 2017 Sono (https://github.com/SonoSooS), (https://gbatemp.net/members/sono.373734/).<br>
The original `HzMod` project is otherwise also known as `HorizonM`, `HorizonModule`, and `HorizonMod`, and includes `HorizonScreen` and `HzLoad`.

**See `LICENSE` for more details.**

###### ~~And if you order now, you'll get 2 data aborts for the price of one!~~
