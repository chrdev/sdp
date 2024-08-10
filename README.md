# SDP - SCSI Disk Power Utility For Windows

Caution: Improper usage may cause MAJOR DAMAGE to DATA/DEVICES!

SDP can lock, dismount, offline, then spin down HDDs at one command. It's handy for removing external HDDs.

SDP can also write power condition timers, given the drive supports it.

### Installaton

Copy sdp.exe to a directory covered by %PATH%, e.g. %SystemRoot%\System32.

### Usage

From a command line with administrator's rights, run SDP.

```
SDP [command] [diskNum] [diskNum] ...

Only one command is allowed, - or / is optional

Command is case-insensitive. Command and drive numbers can be input in any order

Commands:
  L: List, can be omitted if specified diskNum
  P: Stop
  W: Write power condition timer. Use "SDP W" for more help

Examples:
  List all drives: SDP L
  List drive0 and drive2: SDP L 0 2
  Stop drive2 and drive3: SDP P 2 3
```

Working with timers:

```
SDP WL [diskNum] [diskNum] ...
  List power condition timers
  L can be omitted if specified diskNum

SDP W[I|A#][B#][C#][Y#][S|Z#] [diskNum] [diskNum] ...
  Write power condition timers as # seconds

Example:
  Set drive5 Standby_Z timer to 7200 seconds: SDP 5 WZ7200
  Set drive3 Idle_A to 1800 and Standby_Z to 3600: SDP 3 Wa1800z3600

Power Consumption: Idle_A >= Idle_B >= Idle_C > Standby_Y >= Standby_Z

Caution:
  Avoid setting timers to excessively low values, because
  short spin down/up and head unload/load cycles
  can harm your hard drives!
```
