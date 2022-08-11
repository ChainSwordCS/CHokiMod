# CHokiMod

A continuation of HzMod, built for ChokiStream.

A utility background process for the Nintendo 3DS, purpose-built for screen-streaming over WiFi. Name is derived from "ChokiStream", and "HzMod" which itself is derived from "HorizonM"

`CHokiMod` is licensed under the `GNU GPLv3` license. See the [License](#license) section of this file for details, and see the file `LICENSE` for more details including the full terms of the license.

## Getting started

Please note, the HorizonScreen application in this repository is old and obsolete. We recommend using [ChokiStream](https://github.com/Eiim/Chokistream), but it may also work with Snickerstream and/or other old versions of HorizonScreen.

> //TODO the rest of the readme

## Credits
- ### CHokiMod Credits
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

If you really need a nightly build, the CIA in the /chokimod-build-out/ folder is updated with some (but not all) commits. We make no guarantee it will function as intended. It's experimental.

I generally wouldn't recommend setting up a build environment unless you are contributing code to the project, etc.

### Build Requirements
* [DevKitPro](https://devkitpro.org/wiki/Getting_Started)
* DevKitPro 3DS extra libraries (should be done automatically by the Makefile)
  * lib-jpeg-turbo
  * lib-zip
* [makerom](https://github.com/3DSGuy/Project_CTR/releases?q=makerom&expanded=true), in DevKitPro's / MSYS2's PATH.


# License

Copyright (c) 2022 ChainSwordCS (https://chainswordcs.com), (chainswordcs@gmail.com)<br>
`CHokiMod` is licensed under the `GNU GPLv3` license. See `LICENSE` for details.

`HzMod` code and project are licensed under `GNU GPLv3` license.<br>
`HzMod` code and project are Copyright (c) 2017 Sono (https://github.com/SonoSooS), (https://gbatemp.net/members/sono.373734/).<br>
The original `HzMod` project is otherwise also known as `HorizonM`, `HorizonModule`, and `HorizonMod`, and includes `HorizonScreen` and `HzLoad`.

**See `LICENSE` for more details.**

###### ~~And if you order now, you'll get 2 data aborts for the price of one!~~
