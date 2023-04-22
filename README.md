<!--
SPDX-FileCopyrightText: 2023 chrdev

SPDX-License-Identifier: MIT
-->

# SDP - SCSI Disk Power Utility For Windows v1.0

MIT License

Caution:
Improper use of this utility can cause major damage to your data and hardware.
Alawys unmount your HDD before issuing STOP command.

SDP helps spin down HDDs before cutting power. Especially handy when working
with external HDDs.

SDP can also write a drive's power managment timers, if the drive supports it.

### Installaton Example

Rename sdp64-gcc.exe sdp.exe, put it to \Windows\System32\ Directory.

### Usage

Open Command Prompt (Admin), run SDP.

```
SDP [command] [driveID] [driveID] ...

Only one command is allowed, with optional leading - or /

Command is case-insensitive. Command and driveID(s) can appear in any order

Commands:
  L: List, can be omitted if specified driveID
  S: Start
  P: Stop
  W: Write power condition timer. "SDP W" to see more help
	
Examples:
  List all drives: SDP L
  List drive0 and drive2: SDP L 0 2
  Stop drive2 and drive3: SDP P 2 3
```

### Usage Example

Scenario: an HDD is used as external drive docking on a USB bay.

1. The USB bay doesn't spin it down after being safely removed from system.
So instead of safely remove, we go to Disk Managment, offline the drive, then
```
SDP P #
```
Without further operation, we can remove the drive or cut off the dock power.

2. The drive is connected to our working system 24-7. we want it to spin down
if not accessed for longer than 1 hour and 50 minutes, which is 6600 seconds:
```
SDP WY6600 #
```
Done.

Note: use SDP WL to see whether a drive suppports power managment, if you see
only "-" in the drive info second line, then it doesn't support it.

### BlahBlah

The famous sdparm covers all functionality of this software. I wasn't aware
that it also works on Windows when I wrote SDP.
