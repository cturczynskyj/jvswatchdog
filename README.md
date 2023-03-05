# JVSWatchdog

This repository contains source code for a Windows console application, `jvswatchdog.exe`, which pings COM2 once per minutes to reset the Taito Type X3 DMAC's watchdog timer, keeping the DMAC from rebotting the Type X3.

---

The main logic of this was extracted from [JoeHowse/KB2FastIO](https://github.com/JoeHowse/FastIO2KB) application.

---

## Credits

* **Maintainer:** Christopher Turczynskyj
* **Authors:** Joseph Howse, Christopher Turczynskyj

## System Requirements

* **Operating system:** Windows 10 or later. Tested successfully with:
  * Windows 10 Pro, 64-bit
  * Note this may work on other Windows versions but it is untested at this time
* **Fast I/O DMAC board and drivers**: Tested successfully with:
  * Taito Type X<sup>2</sup> PCIe board (K91X1209A J9100636A). This device is manufactured by Oki Information Systems and is found in NESiCAxLive and Dariusburst versions of Taito Type X<sup>2</sup> arcade machines. Device Manager was used to install a driver package containing `oem1.inf`, `iDmacDrv32.sys`, and `iDmacDrv32.dll`, all pulled from a working system.
  * Taito Type X<sup>3</sup> PCIe board (K91X1217C J9100638A) + connector board (K92X0281C J9200167C). This device is manufactured by Oki Information Systems and is found in Taito Type X<sup>3</sup> arcade machines. Device Manager was used to install a driver package containing `idmacdrv64.inf`, `iDmacDrv64.sys`, and `iDmacDrv64.dll`, `iDmacDrv64.cat`, and `idmacdrv64.PNF`, all pulled from a working system.
* **Fast I/O microcontroller board**: Taito boards with the designation J9100634A (having a JAMMA edge) or J9100633A (not having a JAMMA edge) are believed to be compatible.

## Troubleshooting

* **General issues:**
  * If a COM2 port is present on your system, ensure that it is either unused or connected to a JVS board. If `jvswatchdog.exe` is able to open COM2, it assumes that COM2 is connected to a JVS board.

## Changelog

### 03-02-2023

* Forked from [JoeHowse/FastIO2KB](https://github.com/JoeHowse/FastIO2KB)
* Removed unused files
* Removed unused code

