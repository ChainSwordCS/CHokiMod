# ChirunoMod

A continuation of HzMod, built for Chokistream.

A utility background process for the Nintendo 3DS, purpose-built for screen-streaming over WiFi.

`ChirunoMod` is licensed under the `GNU GPLv3` license. See the [License](#license) section of this file for details, and see the file `LICENSE` for more details including the full terms of the license.

## Current features
* screen streaming from 3DS to PC using the Chokistream client

## Getting started

Get the latest stable release of [Chokistream](https://github.com/Eiim/Chokistream) to run on PC.

On 3DS, use FBI to install the `.cia` files...

Install [ChLoad.cia](https://github.com/ChainSwordCS/ChirunoMod/blob/nightly-v4/HzLoad/ChLoad.cia) which is the loader. If you are using an Old-3DS (not a "New" 3DS/XL or "New" 2DS XL) and you're going to play games which use the High Memory Mode (Super Smash Bros, etc.) then also install [ChLoad_HIMEM.cia](https://github.com/ChainSwordCS/ChirunoMod/blob/nightly-v4/HzLoad/ChLoad_HIMEM.cia).

#### For the main ChirunoMod application, you have a few options:
- The [latest Stable release](https://github.com/ChainSwordCS/ChirunoMod/releases) is strongly recommended, and specifically the standard version [ChirunoMod.cia](https://github.com/ChainSwordCS/ChirunoMod/releases/download/v0.2/ChirunoMod.cia) is recommended.
- There are two extra variations: "ChirunoMod_nodebug.cia" has no debug logging but isn't recommended because there is hardly any performance improvement, and "ChirunoMod_verbosedebug.cia" has much more debug logging and records frame-time statistics in the PC-side log but generally runs slower.
- Unstable semi-nightly releases of all three variants can be found in the root directory of this repository. These are experimental, and may not function as intended.

Please note, the PC HorizonScreen application in this repository is old and obsolete. Additionally, modern versions of ChirunoMod are incompatible with any client other than Chokistream (as of Feb. 2023) due to changes in the network packet header format.

> //TODO the rest of the readme

## Credits
- ### ChirunoMod Credits
  - ChainSwordCS - C/C++ programming, code comments, logo design.
  - Eiim - Assistance with code logic and commenting, assistance with logo design.
  - herronjo - Assistance with code logic and commenting.
  - savvychipmunk57 - Assistance with code logic and commenting.
  - bol0gna - Assistance with code logic and commenting.
  - #### ChirunoMod Logo assets
    - Cell9 - [NTR rocket graphic](https://github.com/44670/BootNTR/blob/master/resources/icon.png)
    - Mister Man (The Spriters Resource) - Mario sprites from Super Mario World
    - WordedPuppet (The Spriters Resource) - Cirno sprites from "Touhou Kaeizuka (Phantasmagoria of Flower View)"
    - gabrielwoj (The Spriters Resource) - Projectile sprites from "Touhou Koumakyou (The Embodiment of Scarlet Devil)"
    - Saigyou R. (The Spriters Resource) - Stage backgrounds from "Touhou Fuujinroku (Mountain of Faith)" (IIRC the Final Stage background with the moon was used)
    - Ryan914 and MaidenTREE (The Spriters Resource) - Extra Stage background from "Touhou Gensoukyou (Lotus Land Story)" (Pixel stars were used)
  
- ### ChokiStream Credits (Indirect help with this project)
  - Eiim - Documentation and reverse-engineering of HzMod, development of ChokiStream
  - herronjo - Documentation and reverse-engineering of HzMod, development of ChokiStream
  - ChainSwordCS - Documentation and reverse-engineering of HzMod, development of ChokiStream
  
- ### Original Code
  - Sono - for creating the HzMod project and the code from which CHokiMod was forked.
  
  - #### Additional credits for original HorizonM / HzMod
    - Minnow - figuring out how Base processes can be used
    - ihaveamac - pointing me towards the right direction for fixing memory allocation on new3DS and getting HorizonScreen to compile on macOS
    - Stary - help with WinSockets in HorizonScreen
    - flamerds - betatesting
    - 916253 - betatesting
    - NekoWasHere @ Reddit - betatesting
    - JayLine_ @ Reddit - betatesting

## Build Instructions

If you'd like to grab a nightly build, a CIA file that's updated nearly every commit is in the root directory of this repository. [ChirunoMod.cia](ChirunoMod.cia)

I generally wouldn't recommend setting up a build environment unless you are contributing code to the project, etc.

### Build Requirements
* [DevKitPro](https://devkitpro.org/wiki/Getting_Started)
* Legacy versions of libraries (below)
  * DevKitARM r46
  * libctru 1.2.1
  * 3ds-libjpeg-turbo 2.1.2-2 (Current version as of writing)
  * 3ds-zlib (libzip) 1.2.11-2 (Current version as of writing)
* [makerom](https://github.com/3DSGuy/Project_CTR/releases?q=makerom&expanded=true), in DevKitPro's / MSYS2's PATH.

Note: This branch (the current Main branch) needs to be compiled with legacy libraries. If using Windows, an archive of the libraries can be downloaded from here: https://chainswordcs.com/dl/hzmod_dependencies_2017_v1.zip

###### Alternatively, archives of libctru can be found here: https://wii.leseratte10.de/devkitPro/libctru/2017/<br />and the extra libraries can be found here: https://wii.leseratte10.de/devkitPro/3ds/<br />Please try to choose the same version as I included in hzmod_dependencies_2017_v1.zip<br /> // Off the top of my head, I forget exactly what versions I used. Slightly newer or older may work too.

* `makerom.exe` must be in the PATH or the root of the repository
* Contents of the `devkitarm-r46` folder should be moved to `C:devkitPro:devkitARM` (Please copy or rename the up-to-date devkitARM folder so it can be restored if you need)
* Contents of the `libctru-1.2.1` folder should be moved to `C:devkitPro:libctru` (Same precaution as above)
* The `portlibs` folder can be copied over and merged with the existing folder `C:devkitPro:portlibs`. 2017 libctru/devkitarm uses "armv6k", while current libctru/devkitarm uses "3ds".

# License

Copyright (c) 2022 ChainSwordCS (https://chainswordcs.com), (chainswordcs@gmail.com)<br>
`ChirunoMod` is licensed under the `GNU GPLv3` license. See `LICENSE` for details.

`HzMod` code and project are licensed under `GNU GPLv3` license.<br>
`HzMod` code and project are Copyright (c) 2017 Sono (https://github.com/SonoSooS), (https://gbatemp.net/members/sono.373734/).<br>
The original `HzMod` project is otherwise also known as `HorizonM`, `HorizonModule`, and `HorizonMod`, and includes `HorizonScreen` and `HzLoad`.

**See `LICENSE` for more details.**

###### ~~And if you order now, you'll get 2 data aborts for the price of one!~~
