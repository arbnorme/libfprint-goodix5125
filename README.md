# libfprint with a driver for the Goodix 27c6:5125 fingerprint sensor

This is [libfprint](https://gitlab.freedesktop.org/libfprint/libfprint) 1.94.100
(the version Fedora 44 ships) plus a new driver, **goodixtls5125**, for the
Goodix `27c6:5125` fingerprint sensor found in the power button of the
Huawei MateBook 16. With it, the unmodified `fprintd` can enroll fingers and
use them for login, screen unlock and `sudo`.

> **⚠️ Status: not usable for login, `sudo` or unlocking.**
> The driver talks to the sensor, detects a finger and captures real
> fingerprint images, but **fingerprint recognition (matching) does not work**
> on this sensor. Its 64 × 80 pixel area (about 3 × 4 mm) is too small for
> the open-source matchers available to libfprint: NBIS finds too few
> minutiae, and SIGFM (SIFT) cannot tell different fingers apart. Enrolment
> succeeds, but verification either never matches or cannot be made safe.
> Please do not install this expecting working fingerprint login.
>
> What this repository offers is a working capture driver as a basis for
> anyone researching a matcher for small sensors. Tested on one Huawei
> MateBook 16 with Fedora 44. It is not part of upstream libfprint.

## Supported hardware

| USB ID | Chip ID | Tested on |
|---|---|---|
| `27c6:5125` | `0x2504` | Huawei MateBook 16 (2021), Fedora 44 |

The driver checks the chip ID and refuses other sensors.

## Build (for development only)

There is no binary package, because the driver is not usable for login yet.
To build and try image capture:

```sh
meson setup build -Ddrivers=goodixtls5125
meson compile -C build
meson test -C build goodix5125-calib
build/examples/img-capture    # writes finger.pgm
```

`packaging/libfprint.spec` builds a Fedora RPM from a release tag.

**The sensor is part of the power button: rest your finger on it lightly,
do not press.** Pressing suspends the laptop.

## Going back to Fedora's libfprint

If you installed a locally built RPM:

```sh
sudo dnf distro-sync libfprint
```

## Known limitations

- **Matching does not work** (see Status). Measured on the development
  device: libfprint's NBIS matcher scored 0 on every comparison; with a
  SIGFM matcher and a 30-image enrolment, at a threshold that rejects other
  fingers only 2 of 20 genuine attempts passed.
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

Attach `fprintd.log` to an issue. Logs contain no images, but do not attach
any `.pgm` image files; they show your fingerprint.

## License and credits

LGPL-2.1-or-later, like libfprint. The transport and TLS code is based on the
`goodixtls` driver by the goodix-fp-linux-dev authors
(<https://github.com/goodix-fp-linux-dev/libfprint>, LGPL-2.1-or-later), and the
sensor configuration comes from
[goodix-fp-dump](https://github.com/goodix-fp-linux-dev/goodix-fp-dump) (MIT).

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
