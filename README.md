FiST
====

## Simple, single-threaded POSIX metadata gathering tool

Produces a CSV-like (separator ':') dump of the POSIX metadata of a directory (recursively),
with one line per (filesystem) object.

Object (files, directories/folders, etc.) names are percent-encoded (RFC3986-like) to cope with
special characters properly.
Dates and UIDs/GIDs are not resolved/pretty printed, but displayed as numbers.

The output can then be processed for whatever purpose, including:
- accounting/billing
- data management: who stores what?, how old is the data?, etc.

The basic `Makefile` provided works for Linux and requires some adjustements for other Unices.
