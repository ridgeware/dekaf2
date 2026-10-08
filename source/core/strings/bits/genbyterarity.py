#!/usr/bin/env python3
"""
genbyterarity - computes the table kByteRarity in kbyterarity.h: the rank of each
byte value by its frequency in common text, 0 for the rarest.

The corpus consists of parts with equal weight. A part is either a list of files,
or translated program messages of a list of languages, read from the gettext
catalogs (.mo) of a locale directory - only the translations (msgstr), each
language with equal weight in its part. A part of files is cut to 1.5 MB, the
messages of a language to 200 kB.

The UTF-8 lead bytes 0xC2 to 0xF4 rank above all other bytes, among themselves by
their frequency (see the comment of kByteRarity for the reason).

usage: genbyterarity.py [--write HEADER] PART...

  PART is one of
    --part NAME [--tail BYTES] FILE...     the files, of each only the last BYTES
    --messages LOCALEDIR LANGUAGE...       the messages of the languages

  --write HEADER   replaces the initializer of kByteRarity in HEADER, without it
                   the table goes to stdout

The table in kbyterarity.h was computed with these parts, from the files of a
macOS system with Homebrew:

  genbyterarity.py --write kbyterarity.h \\
    --part logs --tail 400000 <system log files> <log files of an IDE> \\
    --part code <every third .cpp and .h file of dekaf2's source tree> \\
    --part prose <man pages of bash zsh git ssh curl make, as text> <Markdown files> \\
    --part json <JSON files> \\
    --part html <HTML files> \\
    --messages /opt/homebrew/share/locale de fr es it pt pt_BR nl sv da nb fi pl cs sk hu ro hr sl tr el ru uk bg sr \\
    --messages /opt/homebrew/share/locale ja ko zh_CN zh_TW ar he hi th vi

The man pages were written as text with "man -P cat bash | col -b". Log files
grow, so a later run gives slightly different ranks in the middle of the table.
That changes no results, only the speed of a few searches, and marginally.
"""

import collections
import glob
import os
import re
import struct
import sys

PART_LIMIT     = 1_500_000
LANGUAGE_LIMIT = 200_000
LEAD_BYTES     = range(0xC2, 0xF5)


def read_files(sFiles, iTail):
    """the files of a part, each cut to its last iTail bytes, together cut to PART_LIMIT"""
    Data = bytearray()

    for sFile in sFiles:
        try:
            with open(sFile, 'rb') as File:
                Bytes = File.read()
        except OSError as Error:
            print(f'skipping {sFile}: {Error}', file=sys.stderr)
            continue

        if iTail:
            Bytes = Bytes[-iTail:]

        Data += Bytes

        if len(Data) >= PART_LIMIT:
            break

    return bytes(Data[:PART_LIMIT])


def read_catalog(sFile):
    """the translations of a gettext catalog in UTF-8, or none for another charset"""
    with open(sFile, 'rb') as File:
        Data = File.read()

    if len(Data) < 20:
        return []

    iMagic = struct.unpack('<I', Data[:4])[0]

    if iMagic == 0x950412de:
        sOrder = '<'
    elif iMagic == 0xde120495:
        sOrder = '>'
    else:
        return []

    _, iCount, _, iOffset = struct.unpack(sOrder + '4I', Data[4:20])
    Translations = []

    for i in range(iCount):
        iLength, iStart = struct.unpack(sOrder + '2I', Data[iOffset + 8 * i : iOffset + 8 * i + 8])
        Translations.append(Data[iStart : iStart + iLength])

    # the translation of the empty message is the header with the charset
    if not Translations or not re.search(rb'charset=utf-8', Translations[0], re.IGNORECASE):
        return []

    return Translations[1:]


def read_messages(sLocaleDir, sLanguage):
    """the translated messages of a language, cut to LANGUAGE_LIMIT"""
    Data = bytearray()

    for sFile in sorted(glob.glob(os.path.join(sLocaleDir, sLanguage, 'LC_MESSAGES', '*.mo'))):
        for Translation in read_catalog(sFile):
            Data += Translation.replace(b'\0', b'\n') + b'\n'

        if len(Data) >= LANGUAGE_LIMIT:
            break

    if not Data:
        print(f'no messages for {sLanguage} in {sLocaleDir}', file=sys.stderr)

    return bytes(Data[:LANGUAGE_LIMIT])


def add_frequencies(Frequency, Data, dWeight):
    """adds the relative frequencies of the bytes of Data, multiplied by dWeight"""
    if not Data:
        return

    for iByte, iCount in collections.Counter(Data).items():
        Frequency[iByte] += iCount / len(Data) * dWeight


def parse_parts(Args):
    """the parts of the command line, as ('files', name, tail, files) or ('messages', dir, languages)"""
    Parts = []
    sHeader = None
    i = 0

    while i < len(Args):
        sArg = Args[i]

        if sArg == '--write':
            sHeader = Args[i + 1]
            i += 2
        elif sArg == '--part':
            Parts.append(['files', Args[i + 1], 0, []])
            i += 2
        elif sArg == '--tail' and Parts and Parts[-1][0] == 'files':
            Parts[-1][2] = int(Args[i + 1])
            i += 2
        elif sArg == '--messages':
            Parts.append(['messages', Args[i + 1], []])
            i += 2
        elif sArg.startswith('--'):
            sys.exit(f'unknown option {sArg}')
        elif not Parts:
            sys.exit(f'{sArg} belongs to no part')
        else:
            Parts[-1][-1].append(sArg)
            i += 1

    if not Parts:
        sys.exit(__doc__)

    return Parts, sHeader


def compute_ranks(Parts):
    """the rank of each byte value, 0 for the rarest"""
    Frequency = [0.0] * 256

    for Part in Parts:
        if Part[0] == 'files':
            add_frequencies(Frequency, read_files(Part[3], Part[2]), 1.0)
        else:
            _, sLocaleDir, Languages = Part

            for sLanguage in Languages:
                add_frequencies(Frequency, read_messages(sLocaleDir, sLanguage), 1.0 / len(Languages))

    Other = [b for b in range(256) if b not in LEAD_BYTES]
    Order = sorted(Other, key=lambda b: (Frequency[b], b)) + sorted(LEAD_BYTES, key=lambda b: (Frequency[b], b))
    Rank  = [0] * 256

    for iRank, iByte in enumerate(Order):
        Rank[iByte] = iRank

    return Rank


def format_table(Rank):
    """the initializer rows of kByteRarity"""
    return '\n'.join('\t/* 0x{:02X} */ {},'.format(iRow * 16, ', '.join('{:3d}'.format(Rank[iRow * 16 + iCol]) for iCol in range(16)))
                     for iRow in range(16))


def main():
    Parts, sHeader = parse_parts(sys.argv[1:])
    sTable = format_table(compute_ranks(Parts))

    if not sHeader:
        print(sTable)
        return

    with open(sHeader, newline='') as File:
        sText = File.read()

    Match = re.search(r'(constexpr uint8_t kByteRarity\[256\]\n\{\n)(.*?)(\n\};)', sText, re.DOTALL)

    if not Match:
        sys.exit(f'no initializer of kByteRarity in {sHeader}')

    with open(sHeader, 'w', newline='') as File:
        File.write(sText[:Match.start(2)] + sTable + sText[Match.end(2):])


if __name__ == '__main__':
    main()
