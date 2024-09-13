#
# FiST file "analysis" to produce users oriented statistics (in JSON),
# with optionally a file sizes distribution histogram (JSON)
#
# Copyright (c) 2019-2024, CNRS/IN2P3 Computing Centre.
#
# Written by Loic Tortay <tortay@cc.in2p3.fr>.
#
# Requires Python 3.9+ (for the functools.cache decorator)
#
# Usage:
#	python3 fist-summary-fisidi.py -g $GROUP file.fist
#
# $GROUP is the expected group of the files/directories/etc.
# This is used to differentiate files of a single user in multiple groups (for
# instance, because billing is based on groups)
# $GROUP is also used as the name in the report, it does not technically need
# to be a group name
#
# The JSON output is by default printed to `stdout`, a filename can be provided
# with `-o` (`-` for `stdout`).
#
# An histogram of file sizes distribution (in powers of 2 buckets) can be computed
# and displayed with `-H`.
# This option requires a filename for the histogram JSON (`-` to use `stdout`).
# 
# Notes:
# . Dates are scrubbed to $NOW if they are more than 1 day in the future.
# . By default `root` owned filesystem objects are not included in the reports,
#   unless the group is `SPECIAL_GROUP` (`SPECIAL_GROUP` is defined with the `-S`
#   argument)
# . UIDs and GIDs are resolved, if the the resolution fails: IDs are printed as
#   `#ID` (`#` followed by the UID/GID), 
#
import argparse
import functools
import grp
import json
import math
import pwd
import re
import statistics
import time
import sys
from collections import defaultdict
from datetime import datetime
from datetime import timezone
from stat import S_ISREG, S_ISDIR, S_ISLNK

# Display a number with human-readable prefixes (powers of 2)
# following ISO/IEC 80000-13:2008 / EN 60027-2:2007
human_readable_prefixes = [
	{ 'prefix': "", 'scale': 2**0 },
	{ 'prefix': "Ki", 'scale': 2**10 },
	{ 'prefix': "Mi", 'scale': 2**20 },
	{ 'prefix': "Gi", 'scale': 2**30 },
	{ 'prefix': "Ti", 'scale': 2**40 },
	{ 'prefix': "Pi", 'scale': 2**50 }
]
def human_readable(value):
    if value > 0:
        idx = int(math.log2(value) / 10)
        return f'{int(value / human_readable_prefixes[idx]["scale"])} {human_readable_prefixes[idx]["prefix"]}B'
    else:
        return f'{value}'

#
# Handle object name length/depth accounting
def update_object_depth_and_length(name, users, user):
# Do not count '\n'
    l = len(name) - 1
# Number of filename components in the name
    d = name.count('/') + 1
 
    if l < users[user]['minlength']:
        users[user]['minlength'] = l
    if l > users[user]['maxlength']:
        users[user]['maxlength'] = l

    if d < users[user]['mindepth']:
        users[user]['mindepth'] = d
    if d > users[user]['maxdepth']:
        users[user]['maxdepth'] = d


#
# Resolve "uid" UID to a human readable name
#
@functools.cache
def resolve_uid(uid):
    try:
       username = pwd.getpwuid(uid).pw_name
    except KeyError:
       username = f'#{uid}'

    return f'{username}'

#
# Resolve "gid" GID to a human readable name
#
@functools.cache
def resolve_gid(gid):
    try:
       groupname = grp.getgrgid(gid).gr_name
    except KeyError:
       groupname = f'#{gid}'

    return f'{groupname}'

#
# Resolve "uid" + "gid" and returns a string.
# The return string is the whole user ID ("uid:gid"), iff the group is not the one expected
# ("tgtgroup"), otherwise the string is the resolved UID
#
@functools.cache
def resolve_uid_gid(uid, gid, tgtgroup, specialgroup):
    u = resolve_uid(uid)
    g = resolve_gid(gid)

    if g == tgtgroup or tgtgroup == specialgroup:
        return u
    else:
        return f'{u}:{g}'

#
# Process a FiST file to do users (+ optionally fisidi) accounting
#
# "filename" is the name for the FiST file to process
# "expgroup" is the group owning the storage space
# "outputfile" is the name of the JSON file to write or '-' to use "stdout"
# if set, "histofile" is the name of a JSON file which will contain the file
# sizes distribution (in the FiST file)
# 
def analyze_fist_file(filename, expgroup, outputfile, histofile, specialgroup):
    # Main data structures
    users = defaultdict(lambda: {'filesizes': [], 'bytes': 0, 'dirs': 0, 'files': 0, 'symlinks': 0, 'hlinked': 0, 'ondisk': 0, 'latime': 0, 'lmtime': 0, 'lctime': 0, 'mindepth': 9999, 'maxdepth': 0, 'minlength': 9999, 'maxlength': 0, 'ondisk': 0})
    sizes_histogram = defaultdict(lambda: {'bytes': 0, 'files': 0, 'posov': 0, 'negov': 0})
    # Totals
    total_files = total_bytes = total_dirs = total_symlinks = total_ondisk = total_hlinked = total_users = 0
    total_latime = total_lctime = total_lmtime = 0
    total_mindepth = total_minlength = 9999
    total_maxdepth = total_maxlength = 0
    # 
    rightnow = time.time()
    #
    with open(filename, 'r') as fp:
        for line in fp:
            # Ignore comments in the FiST file (optional headers)
            if line.startswith('#'):
                continue
            # All FiST fields
            sblocks, smode, shlinks, suid, sgid, ssize, smtime, satime, sctime, name = line.split(':')
            #
            mode = int(smode, 8)
            uid = int(suid)
            #
            if uid == 0 and expgroup != specialgroup:
                continue
            userid = resolve_uid_gid(uid, int(sgid), expgroup, specialgroup)
            #
            # Regular files
            if S_ISREG(mode):
                blocks = int(sblocks)
                hlinks = int(shlinks)
                size = int(ssize)
                mtime = int(smtime)
                atime = int(satime)
                ctime = int(sctime)
                # Store file sizes to compute the (file sizes) quartiles
                users[userid]['filesizes'].append(size)
                # Space & number of files
                users[userid]['bytes'] += size
                total_bytes += size
                users[userid]['files'] += 1
                total_files += 1
                # Number of files w/ hardlinks
                if hlinks > 1:
                    hlinks_norm = 1.0 / hlinks
                    total_hlinked += hlinks_norm
                    users[userid]['hlinked'] += hlinks_norm
                # Space used on disk (normalized w/ number of hardlinks)
                ondisk = (blocks << 10) / hlinks
                total_ondisk += ondisk 
                users[userid]['ondisk'] += ondisk
                # File times
                # Avoid dates too much in the future (client side bugs)
                if atime > (rightnow + 86400):
                    atime = rightnow
                if atime > users[userid]['latime']:
                    users[userid]['latime'] = atime
                if ctime > (rightnow + 86400):
                    ctime = rightnow
                if ctime > users[userid]['lctime']:
                    users[userid]['lctime'] = ctime
                if mtime > (rightnow + 86400):
                    mtime = rightnow
                if mtime > users[userid]['lmtime']:
                    users[userid]['lmtime'] = mtime
                # Histogram of file sizes distribution
                if histofile:
                    idx = int(math.log2(size)) if size > 1 else 0
                    sizes_histogram[idx]['files'] += 1
                    sizes_histogram[idx]['bytes'] += size
                    this_overhead = size - (blocks << 10)
                    if this_overhead > 0:
                        sizes_histogram[idx]['posov'] += this_overhead
                    else:
                        sizes_histogram[idx]['negov'] += -this_overhead
            # Directories
            elif S_ISDIR(mode):
                users[userid]['dirs'] += 1
                total_dirs += 1
            # Symlinks
            elif S_ISLNK(mode):
                users[userid]['symlinks'] += 1
                total_symlinks += 1
                # Remove target from symlink (name)
                name = re.sub('(?:%20-%3E%20| -> ).*$', '', name)
            else:
            # Ignore FS objects which are neither files/directories/symlinks
                continue
            # Name length & depth
            update_object_depth_and_length(name, users, userid)

    # List for global quartiles
    all_files_sizes = []
    summary = {'name': expgroup, 'date': datetime.now(timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ'), 'users': []}
    # Sort users based on number of bytes (largest first)
    for userid, data in sorted(users.items(), key=lambda item: item[1]['bytes'], reverse=True):
        all_files_sizes.extend(data['filesizes'])
        if data['files'] > 1:
            # statistics.quantiles() needs at least two data points
            avg_file_size = data['bytes'] / data['files']
            min_file_size = min(data['filesizes'])
            q1_file_size, median_file_size, q3_file_size = statistics.quantiles(data['filesizes'], n=4)
            max_file_size = max(data['filesizes'])
            data['hlinked'] = data['hlinked']
            if data['latime'] > total_latime:
                total_latime = data['latime']
            if data['lctime'] > total_lctime:
                total_lctime = data['lctime']
            if data['lmtime'] > total_lmtime:
                total_lmtime = data['lmtime']
        # statistics.quantiles() needs at least two data points, handle the single file case explicitely:
        elif data['files'] == 1:
            avg_file_size = min_file_size = q1_file_size = median_file_size = q3_file_size = max_file_size = data['filesizes'][0]
        else:
            avg_file_size = min_file_size = q1_file_size = median_file_size = q3_file_size = max_file_size = 0
            if data['dirs'] == 0 and data['symlinks'] == 0:
                data['mindepth'] = data['minlength'] = 0
        if data['mindepth'] < total_mindepth:
             total_mindepth = data['mindepth']
        if data['maxdepth'] > total_maxdepth:
             total_maxdepth = data['maxdepth']
        if data['minlength'] < total_minlength:
             total_minlength = data['minlength']
        if data['maxlength'] > total_maxlength:
             total_maxlength = data['maxlength']

        summary['users'].append({
            'name': userid,
            'nfiles': data['files'],
            'hlinkedfiles': int(round(data['hlinked'], 0)),
            'ndirs': data['dirs'],
            'nlinks': data['symlinks'],
            'bytes': data['bytes'],
            'overhead': int(round(data['ondisk'] - data['bytes'], 0)),
            'minfsize': min_file_size,
            'q1fsize': int(round(q1_file_size, 0)),
            'medfsize': int(round(median_file_size, 0)),
            'avgfsize': int(round(avg_file_size, 0)),
            'q3fsize': int(round(q3_file_size, 0)),
            'maxfsize': max_file_size,
            'latime': data['latime'],
            'lmtime': data['lmtime'],
            'lctime': data['lctime'],
            'mindepth': data['mindepth'],
            'maxdepth': data['maxdepth'],
            'minlength': data['minlength'],
            'maxlength': data['maxlength']
        })
        total_users += 1
    # Global average & quartiles
    if total_files > 0:
        total_avg_file_size = total_bytes / total_files
        total_min_file_size = min(all_files_sizes)
        total_q1_file_size, total_median_file_size, total_q3_file_size = statistics.quantiles(all_files_sizes, n=4)
        total_max_file_size = max(all_files_sizes)
    else:
        total_avg_file_size = total_min_file_size = total_q1_file_size = total_median_file_size = total_q3_file_size = total_max_file_size = 0
        if total_dirs == 0 and total_symlinks == 0:
            total_mindepth = total_minlength = 0
    # Totals
    summary['totals'] = {
        'nusers': total_users,
        'nfiles': total_files,
        'hlinkedfiles': int(round(total_hlinked, 0)),
        'ndirs': total_dirs,
        'nlinks': total_symlinks,
        'bytes': total_bytes,
        'overhead': int(round(total_ondisk - total_bytes, 0)),
        'minfsize': total_min_file_size,
        'q1fsize': int(round(total_q1_file_size, 0)),
        'medfsize': int(round(total_median_file_size, 0)),
        'avgfsize': int(round(total_avg_file_size, 0)),
        'q3fsize': int(round(total_q3_file_size, 0)),
        'maxfsize': total_max_file_size,
        'latime': total_latime,
        'lmtime': total_lmtime,
        'lctime': total_lctime,
        'mindepth': total_mindepth,
        'maxdepth': total_maxdepth,
        'minlength': total_minlength,
        'maxlength': total_maxlength
    }
    if outputfile and outputfile != "-":
        with open(outputfile, 'w') as of:
            json.dump(summary, of, indent=4)
    else:
        print(json.dumps(summary, indent=4))

    # File sizes distribution
    if histofile:
        histo = {'name': expgroup, 'file sizes': []}
        for s in sorted(sizes_histogram.keys()):
            lower = 0 if s == 0 else 2**s
            histo['file sizes'].append({
                'range': f'[{human_readable(lower)}:{human_readable(2**(s+1))}[',
                'bytes': sizes_histogram[s]['bytes'],
                'files': sizes_histogram[s]['files'],
                'posov': sizes_histogram[s]['posov'],
                'negov': sizes_histogram[s]['negov']
            })
        if histofile != "-":
            with open(histofile, 'w') as hf:
                json.dump(histo, hf, indent=4)
        else:
            print(json.dumps(histo, indent=4))
    #

# Main
if __name__ == "__main__":
    # Parse script arguments
    parser = argparse.ArgumentParser(description="Process a FiST file to produce a JSON users report & optionally a FiSiDi report")
    parser.add_argument('filename', help="FiST file to process")
    parser.add_argument('-H', '--histofile', help="Output file sizes distribution in this JSON file ('-' for 'stdout')")
    parser.add_argument('-g', '--group', required=True, help="Expected group name")
    parser.add_argument('-o', '--outputfile', help="JSON Output filename")
    parser.add_argument('-S', '--specialgroup', help="Group with special treatment (root-owned objects and multiple groups allowed)")
    args = parser.parse_args()
    analyze_fist_file(args.filename, args.group, args.outputfile, args.histofile, args.specialgroup)

