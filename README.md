# LinVst3-X

Vst3 wrapper (beta).

For 64 bit vst3's only.

Not all vst3 features are supported.

Unlike Vst2, saved projects using wrapped Vst3 plugins are not compatible/transferable between Linux daws and their Windows versions and vice versa.

LinVst3-X (the X stands for Extra) adds support for Windows vst3 plugins to be used in Linux vst capable DAW's.

LinVst3-X runs vst plugins in a single Wine process so plugins that communicate with each other or plugins that can use shared samples between instances will be able to communicate with their other instances.

It means that plugin instances can communicate with each other (which is not possible with LinVst3), for example, Voxengo GlissEQ and Fabfilter ProQ-3 instances on different tracks simultaneously displaying the track spectrums and Kontakt instances on different tracks that use the same library not having to load samples for each new instance etc.

Plugins running in one process are not sandboxed, so if one plugin crashes then the whole lot might crash.

It's best to use plugins that already run with LinVst3 and/or use TestVst3 to test how a plugin might run under Wine.

The LinVst3-X server can be preloaded

wine /usr/bin/lin-vst3-server-x.exe.so for 64 bit plugins

wine /usr/bin/lin-vst3-server-x32.exe.so for 32 bit plugins

LinVst3-X usage is basically the same as LinVst-X except that the file to be renamed to the vst3 dll name is linvst3x.so (rather than linvst.so for LinVst). https://github.com/osxmidi/LinVst/wiki https://github.com/osxmidi/LinVst/blob/master/README.md https://github.com/osxmidi/LinVst/tree/master/Detailed-Guide

So for example, linvst3x.so would be renamed to Delay.so for Delay.vst3 (see convert folder for batch name conversion utilities)

The vst3 dlls are most likely going to be in ~/.wine/drive_c/Program Files/Common Files/VST3

LinVst3-X will try to produce multiple loader part files for vst3 plugins that contain multiple plugins. 
The multiple loader part files should be picked up on the daw's next plugin scan and then the multiple plugins should be available for use in the daw.

Automatic window resizing is not supported, after a resize the UI needs to be closed and then reopened for the new window size to take effect.

Some vst3 plugins might not work due to Wines current capabilities or for some other reason.

Use TestVst3 for testing how a vst3 plugin might run under Wine.

Some vst3 plugins rely on the d2d1 dll which is not totally implemented in current Wine.

If a plugin has trouble with it's display then disabling d2d1 in the winecfg Libraries tab can be tried.

The Sforzando VST3 runs in a better way with d2d1 disabled for instance.

-------

LinVst3-X binaries are on the releases page (under Assets) https://github.com/osxmidi/LinVst3-X/releases

To Make

sudo apt-get install cmake

Libraries that need to be pre installed, 

sudo apt-get install libfreetype6-dev libxcb-util0-dev libxcb-cursor-dev libxcb-keysyms1-dev libxcb-xkb-dev libxkbcommon-dev libxkbcommon-x11-dev libgtkmm-3.0-dev libsqlite3-dev

Wine libwine development files.

------

For Ubuntu/Debian, sudo apt-get install libwine-development-dev (For Debian, Wine might need to be reinstalled after installing libwine-development-dev)

wine-devel packages for other distros (sudo apt-get install wine-devel).

libX11 development (sudo apt-get install libx11-dev)

------

For Fedora 

sudo yum -y install wine-devel wine-devel.i686 libX11-devel libX11-devel.i686
sudo yum -y install libstdc++.i686 libX11.i686

------

For Manjaro/Arch

sudo pacman -Sy wine-staging libx11 gcc-multilib

sudo pacman -Sy cmake freetype2 sqlite libxcb xcb-util gtkmm3 xcb-util-cursor

------

(Optional libraries, Maybe needed for some systems),

libx11-xcb-dev
libxcb-util-dev
libxcb-cursor-dev
libxcb-xkb-dev
libxkbcommon-dev
libxkbcommon-x11-dev
libfontconfig1-dev
libcairo2-dev
libgtkmm-3.0-dev
libsqlite3-dev
libxcb-keysyms1-dev

-------

This LinVst3-X source folder needs to be placed within the VST3 SDK main folder (the VST3_SDK folder or the VST3 folder that contains the base, public.sdk, pluginterfaces etc folders) ie the LinVst3-X source folder needs to be placed alongside the base, public.sdk, pluginterfaces etc folders of the VST3 SDK.

Then change into the LinVst3-X folder and run make and then sudo make install

Then use the batch name conversion utilities (in the convert/binaries folder) to name convert linvst3x.so to the vst3 plugin names ie first select linvst3x.so and then select the ~/.wine/drive_c/Program Files/Common Files/VST3 folder https://github.com/osxmidi/LinVst/wiki

Currently builds ok with the vst-sdk_3.6.14_build-24_2019-11-29 and the previous vstsdk3613_08_04_2019_build_81 versions https://download.steinberg.net/sdk_downloads/vstsdk3613_08_04_2019_build_81.zip

To make using the vst2sdk, remove the -DVESTIGE entries from the Makefile and place the vst2sdk pluginterfaces folder inside the main LinVst3-X source folder.

----------

````//-----------------------------------------------------------------------------
// LICENSE
// (c) 2018, Steinberg Media Technologies GmbH, All Rights Reserved
//-----------------------------------------------------------------------------
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
// 
//   * Redistributions of source code must retain the above copyright notice, 
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation 
//     and/or other materials provided with the distribution.
//   * Neither the name of the Steinberg Media Technologies nor the names of its
//     contributors may be used to endorse or promote products derived from this 
//     software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
// IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE 
// OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE  OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.
//-----------------------------------------------------------------------------
