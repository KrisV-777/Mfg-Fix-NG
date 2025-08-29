# MfgFix NG
Version independent implementation of [MfgFix by andrelo1](https://github.com/andrelo1/mfgfix) based on [antpillager's animation changes](https://github.com/antpillager/mfgfix). 

## Requirements
* [xmake](https://xmake.io/#/)
	* Add this to your `PATH`
* [PowerShell](https://github.com/PowerShell/PowerShell/releases/latest)
* [Visual Studio Community 2022](https://visualstudio.microsoft.com/)
	* Desktop development with C++
* [CommonLibSSE](https://github.com/alandtse/CommonLibVR/tree/ng)
	* You need to build from the alandtse/ng branch
* Create Environment Variables:
  * `XSE_TES5_MODS_PATH`: Path to your MO2/Vortex `mods` folder
  * `XSE_TES5_GAME_PATH`: Path to your SSE install directory

## User Requirements
- [Address Library for SKSE](https://www.nexusmods.com/skyrimspecialedition/mods/32444)
  - Needed for SSE/AE
- [VR Address Library for SKSEVR](https://www.nexusmods.com/skyrimspecialedition/mods/58101)
  - Needed for VR

## Building
```
git clone https://github.com/KrisV-777/Mfg-Fix-NG.git
cd Mfg-Fix-NG
git submodule update --init --recursive
xmake f -m release [
	--copy_to_papyrus=(y/n)		# create/update papyrus mod instance
]
xmake
```
