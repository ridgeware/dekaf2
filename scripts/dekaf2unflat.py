#!/usr/bin/env python3
"""
dekaf2unflat - migrate flat dekaf2 #includes to the new hierarchical layout.

Usage:
    dekaf2unflat.py [--dry-run] [--include-dir <path>] <path>...

Arguments:
    <path>...       Files or directories to process. Directories are
                    walked recursively for .h and .cpp files.

Options:
    --dry-run            Show what would be changed without modifying any files.
    --include-dir <path> Path to the dekaf2 include directory containing the
                         hierarchical headers. Default: /usr/local/include/dekaf2/dekaf2

The flat-to-hierarchical mapping is built automatically by scanning the
installed dekaf2 header directory structure.

Examples:
    dekaf2unflat.py /path/to/myproject
    dekaf2unflat.py *.cpp *.h
    dekaf2unflat.py --dry-run src/
    dekaf2unflat.py --include-dir /opt/dekaf2/include/dekaf2/dekaf2 src/
"""
import os, sys

def usage():
    print()
    print(__doc__.strip())
    print()
    sys.exit(0)

def build_flat_map(include_dir):
    """Scan the installed hierarchical header directory and build a mapping
    from flat include paths to their new hierarchical locations."""
    flat_map = {}
    for root, dirs, files in os.walk(include_dir):
        for fname in files:
            if not fname.endswith('.h'):
                continue
            full = os.path.join(root, fname)
            rel = os.path.relpath(full, include_dir)
            # skip files in the root directory (flat stubs or non-restructured)
            if os.sep not in rel and '/' not in rel:
                continue
            rel = rel.replace(os.sep, '/')
            # derive what the flat include path was:
            #   core/strings/bits/simd/kfindfirstof.h -> bits/simd/kfindfirstof.h
            #   core/strings/bits/kfindsetofchars.h   -> bits/kfindsetofchars.h
            #   core/strings/kstring.h                -> kstring.h
            parts = rel.split('/')
            bits_idx = None
            for j, p in enumerate(parts):
                if p == 'bits':
                    bits_idx = j
                    break
            if bits_idx is not None:
                flat_name = '/'.join(parts[bits_idx:])
            else:
                flat_name = parts[-1]
            if flat_name != rel:
                flat_map[flat_name] = rel
    return flat_map

# parse arguments
targets = []
include_dir_override = None
dry_run = False
args = sys.argv[1:]
i = 0
while i < len(args):
    if args[i] in ('-h', '--help'):
        usage()
    elif args[i] == '--dry-run':
        dry_run = True
    elif args[i] == '--include-dir':
        i += 1
        if i >= len(args):
            print('Error: --include-dir requires an argument', file=sys.stderr)
            sys.exit(1)
        include_dir_override = args[i]
    elif args[i].startswith('-'):
        print(f'Error: unknown option: {args[i]}', file=sys.stderr)
        sys.exit(1)
    else:
        targets.append(args[i])
    i += 1

if not targets:
    usage()

# locate the dekaf2 include directory
include_dir = include_dir_override or '/usr/local/include/dekaf2/dekaf2'
if not os.path.isdir(include_dir):
    print(f'Error: dekaf2 include directory not found: {include_dir}', file=sys.stderr)
    print('Use --include-dir to specify the correct path.', file=sys.stderr)
    sys.exit(1)

flat_map = build_flat_map(include_dir)
def process_file(filepath):
    try:
        with open(filepath, encoding='utf-8', errors='strict') as f:
            content = f.read()
    except (UnicodeDecodeError, OSError) as e:
        print(f'Skipping {filepath}: {e}', file=sys.stderr)
        return 0
    new_content = content
    for old, new in flat_map.items():
        new_content = new_content.replace(
            '#include <dekaf2/' + old + '>',
            '#include <dekaf2/' + new + '>'
        )
        new_content = new_content.replace(
            'DEKAF2_HAS_INCLUDE(<dekaf2/' + old + '>)',
            'DEKAF2_HAS_INCLUDE(<dekaf2/' + new + '>)'
        )
        # also fix quoted includes to angle bracket includes:
        # #include "dekaf2/kstring.h" or #include "dekaf2/bits/kunique_deleter.h"
        new_content = new_content.replace(
            '#include "dekaf2/' + old + '"',
            '#include <dekaf2/' + new + '>'
        )
        # bare quoted includes without dekaf2/ prefix - disabled, too risky
        # for external projects (could match unrelated local headers)
        # if '/' not in old:
        #     new_content = new_content.replace(
        #         '#include "' + old + '"',
        #         '#include <dekaf2/' + new + '>'
        #     )
    if new_content != content:
        if dry_run:
            print('Would update:', filepath)
        else:
            with open(filepath, 'w') as f:
                f.write(new_content)
            print('Updated:', filepath)
        return 1
    return 0

count = 0
for target in targets:
    if os.path.isfile(target):
        count += process_file(target)
    elif os.path.isdir(target):
        for root, dirs, files in os.walk(target):
            for fname in files:
                if fname.endswith('.h') or fname.endswith('.cpp'):
                    count += process_file(os.path.join(root, fname))
    else:
        print(f'Warning: {target}: no such file or directory', file=sys.stderr)
if dry_run:
    print(f'Total: {count} files would be updated')
else:
    print(f'Total: {count} files updated')
