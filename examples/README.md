
## `fist-summary-fisidi.py`
# JSON users summary output
This example script can be used to get a summary of the content in a FiST file, and
optionally an histogram of the file sizes in the FiST file

See the script top comments for more details of the script quirks.

JSON output looks like this:
```
% python3 fist-summary-fisidi.py -g fake-fist fist-basic-example.fist
{
    "name": "fake-fist",
    "date": "2024-09-13T10:53:14Z",
    "users": [
        {
            "name": "#23456:#3456",
            "nfiles": 6,
            "hlinkedfiles": 0,
            "ndirs": 2,
            "nlinks": 0,
            "bytes": 14937,
            "overhead": 13735,
            "minfsize": 22,
            "q1fsize": 203,
            "medfsize": 660,
            "avgfsize": 2490,
            "q3fsize": 4123,
            "maxfsize": 11750,
            "latime": 1726224488,
            "lmtime": 1726224488,
            "lctime": 1726224488,
            "mindepth": 1,
            "maxdepth": 3,
            "minlength": 1,
            "maxlength": 24
        }
    ],
    "totals": {
        "nusers": 1,
        "nfiles": 6,
        "hlinkedfiles": 0,
        "ndirs": 2,
        "nlinks": 0,
        "bytes": 14937,
        "overhead": 13735,
        "minfsize": 22,
        "q1fsize": 203,
        "medfsize": 660,
        "avgfsize": 2490,
        "q3fsize": 4123,
        "maxfsize": 11750,
        "latime": 1726224488,
        "lmtime": 1726224488,
        "lctime": 1726224488,
        "mindepth": 1,
        "maxdepth": 3,
        "minlength": 1,
        "maxlength": 24
    }
}
%
```

A structure appears in `users[]`, for each user owning any object in the FiST file
with values for *this* user:
- `name` is the resolved user ID, if resolution fails the name is replaced by `#UID`;
  if the GID does not resolve to the expected group name (`-g` argument), the GID
  is included in the user's name (separated from the user ID with `:`), unresolved
  GIDs are displayed as `#GID`
- `nfiles` is the number of files
- `hlinkedfiles` is the number of individual files with multiple names (hardlinks)
- `ndirs` is the number of directories (folders)
- `nlinks` is the number of symlinks
- `bytes` is the number of bytes used by files content
- `overhead` is the number of extra bytes required to store the files content due
  to allocation granularity, it can be negative with sparse files, hardlinks, ...
- `minfsize` is the minimum file size
- `q1fsize` is the 1st quartile of file sizes
- `medfsize` is the median (2nd quartile) of file sizes
- `avgfsize` is the average (arithmetic mean) of file sizes
- `q3fsize` is the 3rd quartile of file sizes
- `maxfsize` is the maximum file size
- `latime` is the most recent access time for any file belonging to this user
- `lctime` is the most recent change time for any file belonging to this user
- `lmtime` is the most recent modification time for any file belonging to this user
- `mindepth` is the minimum number of directories (shortest path)
- `maxdepth` is the maximum number of directories (longuest path)
- `minlength` is the shortest complete name (from the root of the data collection)
- `maxlength` is the longuest complete name (from the root of the data collection)

The `l*time` dates can be presented in human readable form with (in Python)
`time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime(latime))` or similar.

A similar structure is in `totals` for all users in the FiST file.
This structure has a `nusers` field which is the number of distinct users, other fields
are like the individual users values.


# JSON histogram output
The optional histogram looks like this:
```
% python3 fist-summary-fisidi.py -g fake-fist fist-basic-example.fist -o /dev/null -H -
{
    "name": "fake-fist",
    "file sizes": [
        {
            "range": "[16 B:32 B[",
            "bytes": 22,
            "files": 1,
            "posov": 0,
            "negov": 4074
        },
        {
            "range": "[256 B:512 B[",
            "bytes": 263,
            "files": 1,
            "posov": 0,
            "negov": 3833
        },
        {
            "range": "[512 B:1 KiB[",
            "bytes": 1321,
            "files": 2,
            "posov": 568,
            "negov": 3343
        },
        {
            "range": "[1 KiB:2 KiB[",
            "bytes": 1581,
            "files": 1,
            "posov": 0,
            "negov": 2515
        },
        {
            "range": "[8 KiB:16 KiB[",
            "bytes": 11750,
            "files": 1,
            "posov": 0,
            "negov": 538
        }
    ]
}
%
```

For each histogram bucket:
- `range` is the range of file sizes in the bucket, buckets are only present if there
  are files in the relevant range
- `bytes` is the number of bytes used by files content
- `files` is the number of files
- `posov` is the positive overhead (where relevant, due to sparse files, hardlinks,
  transparent compression, data in inode, ...)
- `negov` is the negative overhead ("physical" blocks allocated to hold files content)


# Other easy possible processings:
- select N largest files to provide a top-N list
- select files not accessed/modified for N days to archive, remove, etc.
# Less easy possible processings:
- use the object names to provide per-directory informations instead of per-user
- use the object names (filename "extensions") to provide usage breakdown (e.g. images,
  movies, executables, source files, etc.)
- use the object names & the user ID to provide per-user subtrees of user content,
  for instance to know which files to archive, remove or re-affect to someone else when
  a user account is closed
- ...
