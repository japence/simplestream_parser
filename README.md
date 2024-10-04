# simplestream_parser
CLI tool for fetching and displaying Simplestream information for Ubuntu Cloud, including:
* A list of all currently supported Ubuntu releases.
* The current Ubuntu LTS version.
* The SHA256 checksum of the disk1.img item of a given Ubuntu release.

## Build Instructions

    mkdir build
    cmake -S . -B ./build
    cmake --build ./build

## Usage
`simplestream [OPTION]... <release>...`
### Options
* `-l, --list` List currently supported Ubuntu releases.
* `-c, --current` Current Ubuntu LTS version.
* `-s, --sha256 <release>...` SHA256 checksum of disk1.img for the given release(s).
* `-h, --help` Display help and exit.
### Arguments
The `release` argument(s) can be any of the following:
* A release version: `24.04`
* A release name: `noble`
* A release initial: `n`
* Any string that contains the release version: `Ubuntu-24.04`
