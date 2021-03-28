# Ducky Detector Daemon
This daemon protect aims at detecting and protecting against any rubberducky attacks.

Currently tested against: Maldruino elite / Bash Bunny

![CMake](https://github.com/0xDEADC0DEx/duckydd/workflows/CMake/badge.svg?branch=master)
![CodeQL](https://github.com/0xDEADC0DEx/duckydd/workflows/CodeQL/badge.svg?branch=master)

## Compatibility Note:
This daemon depends on the following libraries:
```
udev
libc
```

The following libraries can optionaly be linked against to provide
the daemon with keymaps from the x server.
```
xkbcommon
xkbcommon-x11
xcb
```

If you want to build and run the cmocka tests as well you will also need to have cmocka installed.

Systemd is not required although you will have to write your own init script
if you don't want to use the provided service file.

## Install:
```
# Clone the code and the submodule
$ git clone --recurse-submodules -j8 https://github.com/0xDEADC0DEx/duckydd
$ cd duckydd

# Compile the code in the build directory
# (You can optionaly change the -DENABLE_XKB_EXTENSION flag to OFF if you don't want to link against x server dependent libraries)
$ cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DENABLE_XKB_EXTENSION=ON && cmake --build build
```

If you use Systemd then you can install the project with a service file like this:

```
cd build
sudo make install
```

## Arguments:
```
Usage: duckydd [Options]
		-c <file>	Specify a config file path
		-d		    Daemonize the process
		-v		    Increase verbosity of the console output (The maximum verbosity is 2)
		-h		    Shows this help section
```

__Important Note:__
THE -v OPTION IS FOR DEBUGGING!
DISABLE IT IF YOU DON'T NEED IT BECAUSE IT COULD POTENTIALY LOG AND THEIRFORE EXPOSE PASSWORDS!

## Uninstall:
Run the following commands from the project root to get rid of duckydd
```
$ cd build
$ sudo xargs rm < install_manifest.txt
$ sudo rm -rf /etc/duckydd
```
