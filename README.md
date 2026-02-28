# Trigon-Legacy

## Overview

Trigon-Legacy is a Kernel exploit which exploits an integer overflow in the VM layer when creating memory entries. This allows arbitrary physical memory read/write, which is then used to build a tfp0 primitive. The techniques used are entirely deterministic and unusable in higher iOS versions, not overlapping with the original [Trigon](https://github.com/alfiecg24/Trigon).

Write-up eta s0n.

## CVEs
- CVE-2020-3836 ([cuck00](https://github.com/Siguza/cuck00)) - port info leak by Siguza
- CVE-2023-32434 ([Trigon](https://github.com/alfiecg24/Trigon)) - arbitrary physical memory read/write by Alfie

### Supported Devices

- All 64-bit devices running iOS 7 to iOS 9

## Build Requirements

- Xcode
- ldid-procursus

## Building

### Build IPA

```bash
make ipa
```

### Build standalone binary

```bash
make binary
```

## Credits

- Alfie
- Staturnz
- Dora
- Siguza