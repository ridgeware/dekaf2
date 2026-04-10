#!/usr/bin/env python3
"""Fix flat includes in a given directory using restructure-map.txt."""
import re, os, sys

flat_map = {}
with open('/Users/jschurig/src/dekaf2/restructure-map.txt') as f:
    for line in f:
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        parts = line.split(' -> ')
        if len(parts) == 2:
            old, new = parts[0].strip(), parts[1].strip()
            flat_map[old] = new

target_dir = sys.argv[1] if len(sys.argv) > 1 else '/Users/jschurig/src/dekaf2/utests'
count = 0
for root, dirs, files in os.walk(target_dir):
    for fname in files:
        if not (fname.endswith('.h') or fname.endswith('.cpp')):
            continue
        filepath = os.path.join(root, fname)
        with open(filepath) as f:
            content = f.read()
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
            # #include "kstring.h" (only for top-level headers without /)
            if '/' not in old:
                new_content = new_content.replace(
                    '#include "' + old + '"',
                    '#include <dekaf2/' + new + '>'
                )
        if new_content != content:
            with open(filepath, 'w') as f:
                f.write(new_content)
            print('Updated:', os.path.relpath(filepath, target_dir))
            count += 1
print(f'Total: {count} files updated')
