#!/usr/bin/env python3
"""Pack a folder of game files into a minimal ISO 9660 image, for testing.

    make_test_iso.py <USRDIR folder> <out.iso>

The files appear as /PSP_GAME/USRDIR/... in the image, which is where the game
looks for them as disc0:/PSP_GAME/USRDIR/... . This is not a UMD image: there is
no PSP_GAME/SYSDIR, no raw sector data and no copy protection. It exists so the
bring-up host, which reads the disc from an ISO, can be pointed at a folder.
"""
import os
import struct
import sys

SECTOR = 2048


def both32(v):
    return struct.pack("<I", v) + struct.pack(">I", v)


def both16(v):
    return struct.pack("<H", v) + struct.pack(">H", v)


def dir_record(name, lba, size, is_dir):
    ident = name
    rec = struct.pack("<BB", 0, 0) + both32(lba) + both32(size)
    rec += bytes([100, 1, 1, 0, 0, 0, 0])  # 2000-01-01 00:00:00, UTC
    rec += bytes([2 if is_dir else 0, 0, 0]) + both16(1)
    rec += bytes([len(ident)]) + ident
    if len(rec) % 2:
        rec += b"\0"
    return bytes([len(rec)]) + rec[1:]


class Node:
    def __init__(self, name, path, is_dir):
        self.name, self.path, self.is_dir = name, path, is_dir
        self.children, self.parent = [], None
        self.lba = self.size = 0


def scan(path, name, parent=None):
    node = Node(name, path, os.path.isdir(path))
    node.parent = parent
    if node.is_dir:
        for entry in sorted(os.listdir(path)):
            node.children.append(scan(os.path.join(path, entry), entry.upper(), node))
    else:
        node.size = os.path.getsize(path)
    return node


def iso_name(node):
    return node.name.encode() if node.is_dir else node.name.encode() + b";1"


def records_for(node):
    own = dir_record(b"\0", node.lba, node.size, True)
    up = dir_record(b"\1", (node.parent or node).lba, (node.parent or node).size, True)
    return [own, up] + [dir_record(iso_name(c), c.lba, c.size, c.is_dir) for c in node.children]


def dir_size(node):
    size, used = 0, 0
    for rec in records_for(node):
        if used + len(rec) > SECTOR:
            size, used = size + SECTOR, 0
        used += len(rec)
    return size + SECTOR


def main(src, out):
    usrdir = scan(src, "USRDIR")
    psp_game = Node("PSP_GAME", "", True)
    psp_game.children = [usrdir]
    usrdir.parent = psp_game
    root = Node("", "", True)
    root.children = [psp_game]
    psp_game.parent = root

    dirs, files, queue = [], [], [root]
    while queue:
        node = queue.pop(0)
        dirs.append(node)
        for c in node.children:
            if c.is_dir:
                queue.append(c)
            else:
                files.append(c)

    next_lba = 20
    for d in dirs:  # sizes are fixed by the directory contents, so assign in two passes
        d.lba = next_lba
        d.size = SECTOR  # provisional
        next_lba += 1
    for d in dirs:
        d.size = dir_size(d)
    next_lba = 20
    for d in dirs:
        d.lba = next_lba
        next_lba += d.size // SECTOR
    for f in files:
        f.lba = next_lba
        next_lba += max(1, (f.size + SECTOR - 1) // SECTOR)

    def path_table(big):
        enc = (lambda v: struct.pack(">I", v)) if big else (lambda v: struct.pack("<I", v))
        enc16 = (lambda v: struct.pack(">H", v)) if big else (lambda v: struct.pack("<H", v))
        index = {id(d): i + 1 for i, d in enumerate(dirs)}
        table = b""
        for d in dirs:
            ident = b"\0" if d is root else d.name.encode()
            entry = bytes([len(ident), 0]) + enc(d.lba) + enc16(index[id(d.parent or d)]) + ident
            if len(ident) % 2:
                entry += b"\0"
            table += entry
        return table

    ptable_l, ptable_m = path_table(False), path_table(True)

    pvd = bytearray(SECTOR)
    pvd[0:8] = b"\x01CD001\x01\x00"
    pvd[8:40] = b"PSP GAME".ljust(32)
    pvd[40:72] = b"TEKKEN6TEST".ljust(32)
    pvd[80:88] = both32(next_lba)
    pvd[120:124] = both16(1)
    pvd[124:128] = both16(1)
    pvd[128:132] = both16(SECTOR)
    pvd[132:140] = both32(len(ptable_l))
    pvd[140:144] = struct.pack("<I", 18)
    pvd[148:152] = struct.pack(">I", 19)
    pvd[156:190] = dir_record(b"\0", root.lba, root.size, True)
    pvd[881] = 1

    with open(out, "wb") as image:
        image.write(b"\0" * (16 * SECTOR))
        image.write(pvd)
        term = bytearray(SECTOR)
        term[0:8] = b"\xffCD001\x01\x00"
        image.write(term)
        image.write(ptable_l.ljust(SECTOR, b"\0"))
        image.write(ptable_m.ljust(SECTOR, b"\0"))
        for d in dirs:
            sector, used = bytearray(), 0
            blob = bytearray()
            for rec in records_for(d):
                if used + len(rec) > SECTOR:
                    blob += bytes(SECTOR - used)
                    used = 0
                blob += rec
                used += len(rec)
            blob += bytes(SECTOR - used)
            assert len(blob) == d.size, (d.name, len(blob), d.size)
            image.write(blob)
        for f in files:
            with open(f.path, "rb") as source:
                data = source.read()
            image.write(data + bytes(-len(data) % SECTOR if data else SECTOR))
    print(f"wrote {out}: {next_lba} sectors, {len(files)} files")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2])
