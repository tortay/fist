FiST
====

## Simple, single-threaded POSIX metadata gathering tool

Produces a CSV-like (separator ':', no header) dump of the POSIX metadata of a directory
(recursively), with one line per (filesystem) object.

Object (files, directories/folders, etc.) names are percent-encoded (RFC3986-like) to cope with
special characters properly.
Dates and UIDs/GIDs are not resolved/pretty printed, but displayed as numbers.

The output can then be processed for whatever purpose, including:
- accounting/billing
- data management: who stores what?, how old is the data?, etc.

The basic `Makefile` provided works for Linux and requires some adjustements for other Unices.

The output looks like this:
```
% ./fist .
4:40755:2:12345:4567:4096:1726156208:1726156206:1726156208:.
32:100755:1:12345:4567:30560:1726156208:1726156212:1726156208:./fist
4:100644:1:12345:4567:654:1726156201:1726156201:1726156201:./README.md
12:100644:1:12345:4567:11750:1726156202:1726156208:1726156202:./fist.c
4:100644:1:12345:4567:753:1726156201:1726156201:1726156201:./LICENSE
4:100644:1:12345:4567:263:1726156201:1726156206:1726156201:./Makefile
4:100644:1:12345:4567:22:1726156201:1726156201:1726156201:./.gitignore
%
```

The fields are:
`blocks perms nlinks uid gid size mtime atime ctime name`

- `blocks` is in KiB not `512` bytes sector
- `perms` is displayed in octal (it's the full `mode_t` not just the permissions)
- `nlinks` id the number of hardlinks to this object
- `uid`, `gid`, `atime`, `ctime`, `mtime` are displayed as numbers
- dates are Unix-epoch (1st Jan 1970) based number of seconds

A faster/more modern [Golang implementation](https://gitlab.in2p3.fr/tortay/gofist) exists.
