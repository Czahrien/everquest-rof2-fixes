#!/usr/bin/env python3
"""Compare two builds of dinput8.dll, ignoring fields that identify the build.

Builds from different directories or hosts differ only in the PE header's
TimeDateStamp (a content hash, not a time), the checksum, and the debug
directory's timestamps and build-ID payload. Everything else should match.

Usage: python tools/compare-dll.py a.dll b.dll
Exit status: 0 if equivalent, 1 if not, 2 on bad input.
"""

import struct
import sys

IMAGE_DIRECTORY_ENTRY_DEBUG = 6
DEBUG_DIRECTORY_ENTRY_SIZE = 28


def identity_ranges(data):
    """Returns (start, end, description) byte ranges that identify the build."""
    (pe_offset,) = struct.unpack_from("<I", data, 0x3C)
    if data[pe_offset:pe_offset + 4] != b"PE\0\0":
        raise ValueError("not a PE file")
    file_header = pe_offset + 4
    optional_header = file_header + 20
    (num_sections,) = struct.unpack_from("<H", data, file_header + 2)
    (optional_size,) = struct.unpack_from("<H", data, file_header + 16)
    (magic,) = struct.unpack_from("<H", data, optional_header)
    if magic != 0x10B:
        raise ValueError("not a 32-bit PE file")

    ranges = [
        (file_header + 4, file_header + 8, "PE header TimeDateStamp"),
        (optional_header + 64, optional_header + 68, "PE checksum"),
    ]

    sections = []
    for i in range(num_sections):
        entry = optional_header + optional_size + i * 40
        virtual_size, virtual_address, raw_size, raw_pointer = struct.unpack_from("<IIII", data, entry + 8)
        sections.append((virtual_address, max(virtual_size, raw_size), raw_pointer))

    def rva_to_offset(rva):
        for virtual_address, size, raw_pointer in sections:
            if virtual_address <= rva < virtual_address + size:
                return raw_pointer + (rva - virtual_address)
        return None

    debug_rva, debug_size = struct.unpack_from("<II", data, optional_header + 96 + IMAGE_DIRECTORY_ENTRY_DEBUG * 8)
    debug_offset = rva_to_offset(debug_rva) if debug_rva else None
    if debug_offset is not None:
        for i in range(debug_size // DEBUG_DIRECTORY_ENTRY_SIZE):
            entry = debug_offset + i * DEBUG_DIRECTORY_ENTRY_SIZE
            ranges.append((entry + 4, entry + 8, f"debug directory entry {i} TimeDateStamp"))
            (payload_size,) = struct.unpack_from("<I", data, entry + 16)
            (payload_offset,) = struct.unpack_from("<I", data, entry + 24)
            if payload_size:
                ranges.append((payload_offset, payload_offset + payload_size, f"debug directory entry {i} payload"))
    return ranges


def masked(data, ranges):
    out = bytearray(data)
    for start, end, _ in ranges:
        out[start:end] = bytes(end - start)
    return out


def main(argv):
    if len(argv) != 3:
        print(__doc__.strip().splitlines()[-2], file=sys.stderr)
        return 2
    try:
        a, b = (open(path, "rb").read() for path in argv[1:])
        ranges_a, ranges_b = identity_ranges(a), identity_ranges(b)
    except (OSError, ValueError, struct.error) as e:
        print(f"error: {e}", file=sys.stderr)
        return 2

    if a == b:
        print("identical")
        return 0
    if len(a) != len(b):
        print(f"different sizes: {len(a)} vs {len(b)} bytes")
        return 1
    if [r[:2] for r in ranges_a] != [r[:2] for r in ranges_b]:
        print("different PE layout")
        return 1

    ignored = {desc for start, end, desc in ranges_a if a[start:end] != b[start:end]}
    diffs = [i for i, (x, y) in enumerate(zip(masked(a, ranges_a), masked(b, ranges_a))) if x != y]
    if diffs:
        print(f"different: {len(diffs)} bytes differ outside the build-identity fields, first at 0x{diffs[0]:X}")
        return 1
    print("equivalent: only build-identity fields differ")
    for desc in sorted(ignored):
        print(f"  ignored: {desc}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
