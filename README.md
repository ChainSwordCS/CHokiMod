# CHmod

A continuation of HzMod, built for Chokistream.

A utility background process for the Nintendo 3DS, purpose-built for screen-streaming over WiFi. Name is subject to change

`CHmod` is licensed under the `GNU GPLv3` license. See the [License](#license) section of this file for details, and see the file `LICENSE` for more details including the full terms of the license.

## Current features
* screen streaming from 3DS using HorizonScreen
* VRAM corruptor (hold `ZL`+`ZR`)

## Getting started

Please note, the PC HorizonScreen application in this repository is old and obsolete. We recommend using [ChokiStream](https://github.com/Eiim/Chokistream), but it may also work with Snickerstream and/or other old versions of HorizonScreen.

> //TODO the rest of the readme

## Credits
- ### CHmod Credits
  - ChainSwordCS - C++ and C programming, code comments
  - savvychipmunk57 - C++ and C programming, code comments, assistance with C++/C logic and syntax.
  - bol0gna - c++ and C programming, code comments, assistance with C++/C logic and syntax.
  
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

If you'd like to grab a nightly build, a CIA file that's updated nearly every commit is in the root directory of this repository. (CHmod.cia)

I generally wouldn't recommend setting up a build environment unless you are contributing code to the project, etc.

### Build Requirements
* [DevKitPro](https://devkitpro.org/wiki/Getting_Started)
* DevKitPro 3DS extra libraries (should be done automatically by the Makefile)
  * lib-jpeg-turbo
  * lib-zip
* [makerom](https://github.com/3DSGuy/Project_CTR/releases?q=makerom&expanded=true), in DevKitPro's / MSYS2's PATH.

Note: This branch (the current Main branch) needs to be compiled with legacy libraries. If using Windows, an archive of the libraries can be downloaded from here: https://chainswordcs.com/dl/hzmod_dependencies_2017_v1.zip

* `makerom.exe` must be in the PATH or the root of the repository
* Contents of the `devkitarm-r46` folder should be moved to `C:devkitPro:devkitARM` (Please copy or rename the up-to-date devkitARM folder so it can be restored if you need)
* Contents of the `libctru-1.2.1` folder should be moved to `C:devkitPro:libctru` (Same precaution as above)
* The `portlibs` folder can be copied over and merged with the existing folder `C:devkitPro:portlibs`. 2017 libctru/devkitarm uses "armv6k", while current libctru/devkitarm uses "3ds".

# License

Copyright (c) 2022 ChainSwordCS (https://chainswordcs.com), (chainswordcs@gmail.com)<br>
`CHmod` is licensed under the `GNU GPLv3` license. See `LICENSE` for details.

`HzMod` code and project are licensed under `GNU GPLv3` license.<br>
`HzMod` code and project are Copyright (c) 2017 Sono (https://github.com/SonoSooS), (https://gbatemp.net/members/sono.373734/).<br>
The original `HzMod` project is otherwise also known as `HorizonM`, `HorizonModule`, and `HorizonMod`, and includes `HorizonScreen` and `HzLoad`.

**See `LICENSE` for more details.**

###### ~~And if you order now, you'll get 2 data aborts for the price of one!~~
