# libfprint with a driver for the Goodix 27c6:5125 fingerprint sensor

This is [libfprint](https://gitlab.freedesktop.org/libfprint/libfprint) 1.94.100
(the version Fedora 44 ships) plus a new driver, **goodixtls5125**, for the
Goodix `27c6:5125` fingerprint sensor found in the power button of the
Huawei MateBook 16. With it, the unmodified `fprintd` can enrol fingers and
use them for login, screen unlock and `sudo`.

> **Status: recognition works, with Goodix's own matching engine.**
> The sensor's 64 × 80 pixel area (about 3 × 4 mm) is too small for the
> open-source matchers in libfprint (NBIS, SIGFM). The driver therefore uses
> Goodix's proprietary engine from the official Windows driver, loaded
> in-process. **The engine is not part of this repository or package**; a
> script downloads it from the Microsoft Update Catalog (see Install).
>
> Tested on one Huawei MateBook 16 with Fedora 44. Not part of upstream
> libfprint.

## How well it works

Measured on the development device only (one person, one sensor), with the
match threshold at engine score 30:

| Test | Result |
|---|---|
| Offline, 23 captures of the enrolled finger | 12 of 23 recognised |
| Offline, 24 captures of other fingers | 0 of 24 accepted |
| Driver, enrolled finger | 8 of 13 recognised |
| Driver, other fingers | 0 of 19 accepted |

Expect roughly every second touch to be recognised; lift and touch again if
it fails. Enrolling carefully helps a lot: rest the finger about a second
per touch and move it a little each time (tip, centre, left, right, up,
down).

**This is convenience, not proven security.** Zero false accepts in a few
dozen attempts from one person says little about strangers' fingers. Keep a
strong password; `fprintd` always lets you fall back to it.

## Supported hardware

| USB ID | Chip ID | Tested on |
|---|---|---|
| `27c6:5125` | `0x2504` | Huawei MateBook 16 (2021), Fedora 44 |

The driver checks the chip ID and refuses other sensors. x86-64 only (the
engine is a Windows x86-64 DLL).

## Install (Fedora)

1. Build and install the RPM from `packaging/libfprint.spec` (it replaces
   Fedora's `libfprint`):

   ```sh
   rpmbuild -ba packaging/libfprint.spec   # after fetching Source0 with spectool -g
   sudo dnf install ~/rpmbuild/RPMS/x86_64/libfprint-1.94.100-100.goodix5125.2*.rpm
   ```

2. Download the Goodix engine (checks the package's SHA-1; you can also
   download the package yourself and pass `--cab FILE`):

   ```sh
   sudo goodix5125-fetch-engine
   ```

   It installs `/usr/lib64/libfprint-2/goodix5125/AlgoChicago.dll`.

3. On SELinux systems, allow `fprintd` to map the engine. `fprintd` keeps its
   `MemoryDenyWriteExecute` hardening; the loader maps the engine through a
   memfd, which the stock policy denies:

   ```sh
   cd /usr/share/libfprint-goodix5125/selinux
   checkmodule -M -m -o /tmp/goodix5125-engine.mod goodix5125-engine.te
   semodule_package -o /tmp/goodix5125-engine.pp -m /tmp/goodix5125-engine.mod
   sudo semodule -i /tmp/goodix5125-engine.pp
   ```

4. Restart `fprintd` and enrol:

   ```sh
   sudo systemctl restart fprintd
   fprintd-enroll
   fprintd-verify
   ```

**The sensor is part of the power button: rest your finger on it lightly,
do not press.** Pressing suspends the laptop.

Without the engine, opening the sensor fails with
`Could not load the Goodix engine ... (run goodix5125-fetch-engine ...)`.

## Development

```sh
meson setup build -Ddrivers=goodixtls5125
meson compile -C build
meson test -C build goodix5125-calib
GOODIX5125_ENGINE_DLL=/path/to/AlgoChicago.dll meson test -C build goodix5125-engine
GOODIX5125_ENGINE_DLL=/path/to/AlgoChicago.dll build/examples/enroll
```

`GOODIX5125_ENGINE_DLL` overrides the engine path. Never commit the DLL.

## Going back to Fedora's libfprint

```sh
sudo dnf distro-sync libfprint
sudo semodule -r goodix5125-engine
sudo rm -r /usr/lib64/libfprint-2/goodix5125
```

## Known limitations

- The engine is proprietary Goodix code running inside `fprintd`. Its bugs
  cannot be fixed here, and its download URL may disappear; the fetch
  script then explains how to find the package in the catalogue manually.
- The driver talks to the sensor over TLS with the all-zero pre-shared key.
  A sensor that was provisioned with a different key (for example by a
  Windows installation) is reported as unsupported; the driver never
  rewrites the key.
- There is no automated end-to-end test: a USB recording of a scan would
  contain a decryptable fingerprint image. The hardware-independent parts
  are covered by unit tests (`meson test goodix5125-calib`).

## Reporting a bug

```sh
sudo systemctl stop fprintd
sudo G_MESSAGES_DEBUG=all /usr/libexec/fprintd -t 2>&1 | tee fprintd.log
# in a second terminal: fprintd-verify
```

Attach `fprintd.log` to an issue. Logs contain no images. Never attach
enrolled prints from `/var/lib/fprint`; they are derived from your
fingerprint.

## License and credits

LGPL-2.1-or-later, like libfprint.

- Transport and TLS code: the `goodixtls` driver by the goodix-fp-linux-dev
  authors (<https://github.com/goodix-fp-linux-dev/libfprint>,
  LGPL-2.1-or-later).
- Sensor configuration:
  [goodix-fp-dump](https://github.com/goodix-fp-linux-dev/goodix-fp-dump) (MIT).
- In-process engine loader and SELinux module: adapted from
  [goodix-5e0a](https://github.com/jitendradara12/goodix-5e0a)
  (LGPL-2.1-or-later).
- The idea of running the vendor's Windows matcher under libfprint follows
  ft9201-libfprint.
- The Goodix engine (`AlgoChicago.dll`) is Goodix's property and is not
  distributed here.

---



<div align="center">

# LibFPrint

*LibFPrint is part of the **[FPrint][Website]** project.*

<br/>

[![Button Website]][Website]
[![Button Documentation]][Documentation]

[![Button Supported]][Supported]
[![Button Unsupported]][Unsupported]

[![Button Contribute]][Contribute]
[![Button Contributors]][Contributors]

</div>

## History

**LibFPrint** was originally developed as part of an
academic project at the **[University Of Manchester]**.

It aimed to hide the differences between consumer
fingerprint scanners and provide a single uniform
API to application developers.

## Goal

The ultimate goal of the **FPrint** project is to make
fingerprint scanners widely and easily usable under
common Linux environments.

## License

`Section 6` of the license states that for compiled works that use
this library, such works must include **LibFPrint** copyright notices
alongside the copyright notices for the other parts of the work.

**LibFPrint** includes code from **NIST's** **[NBIS]** software distribution.

We include **Bozorth3** from the **[US Export Controlled]**
distribution, which we have determined to be fine
being shipped in an open source project.

## Get in *touch*

 - [IRC] - `#fprint` @ `irc.oftc.net`
 - [Matrix] - `#fprint:matrix.org` bridged to the IRC channel
 - [MailingList] - low traffic, not much used these days

<br/>

<div align="right">

[![Badge License]][License]

</div>


<!----------------------------------------------------------------------------->

[Documentation]: https://fprint.freedesktop.org/libfprint-dev/
[Contributors]: https://gitlab.freedesktop.org/libfprint/libfprint/-/graphs/master
[Unsupported]: https://gitlab.freedesktop.org/libfprint/wiki/-/wikis/Unsupported-Devices
[Supported]: https://fprint.freedesktop.org/supported-devices.html
[Website]: https://fprint.freedesktop.org/
[MailingList]: https://lists.freedesktop.org/mailman/listinfo/fprint
[IRC]: ircs://irc.oftc.net:6697/#fprint
[Matrix]: https://matrix.to/#/#fprint:matrix.org

[Contribute]: ./HACKING.md
[License]: ./COPYING

[University Of Manchester]: https://www.manchester.ac.uk/
[US Export Controlled]: https://fprint.freedesktop.org/us-export-control.html
[NBIS]: http://fingerprint.nist.gov/NBIS/index.html


<!---------------------------------[ Badges ]---------------------------------->

[Badge License]: https://img.shields.io/badge/License-LGPL2.1-015d93.svg?style=for-the-badge&labelColor=blue


<!---------------------------------[ Buttons ]--------------------------------->

[Button Documentation]: https://img.shields.io/badge/Documentation-04ACE6?style=for-the-badge&logoColor=white&logo=BookStack
[Button Contributors]: https://img.shields.io/badge/Contributors-FF4F8B?style=for-the-badge&logoColor=white&logo=ActiGraph
[Button Unsupported]: https://img.shields.io/badge/Unsupported_Devices-EF2D5E?style=for-the-badge&logoColor=white&logo=AdBlock
[Button Contribute]: https://img.shields.io/badge/Contribute-66459B?style=for-the-badge&logoColor=white&logo=Git
[Button Supported]: https://img.shields.io/badge/Supported_Devices-428813?style=for-the-badge&logoColor=white&logo=AdGuard
[Button Website]: https://img.shields.io/badge/Homepage-3B80AE?style=for-the-badge&logoColor=white&logo=freedesktopDotOrg
