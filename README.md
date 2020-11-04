# bt-dev

### Introduction
The goal of this repository is to learn how to develop Bluetooth
programs.

### Prerequisites
- These programs were developed and tested on Ubuntu 20.04 LTS.
- Superuser privilege is required.

### Install libbluetooth development files
```
$ sudo apt install libbluetooth-dev
$ sudo updatedb && locate bluetooth.h
```

I'm using BlueZ version 5.53, which is included in Ubuntu 20.04 LTS.
To find out the version that you have, use this command:
```
$ bluetoothctl -v
```

### Build programs
```
$ make
```

### License
The software contained herein is released into the public domain under the
[Unlicense](https://unlicense.org/). In layman's terms, there are no
restrictions and you are free to do whatever you want with it.

### References
- http://www.bluez.org/
