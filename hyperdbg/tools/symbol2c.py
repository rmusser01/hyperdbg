"""
  Copyright notice
  ================
  
  Copyright (C) 2010
      Lorenzo  Martignoni <martignlo@gmail.com>
      Roberto  Paleari    <roberto.paleari@gmail.com>
      Aristide Fattori    <joystick@security.dico.unimi.it>
  
  This program is free software: you can redistribute it and/or modify it under
  the terms of the GNU General Public License as published by the Free Software
  Foundation, either version 3 of the License, or (at your option) any later
  version.
  
  HyperDbg is distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
  A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License along with
  this program. If not, see <http://www.gnu.org/licenses/>.
  
"""

import pdbparse
import sys

from pdbparse.pe import Sections
from pdbparse.omap import remap,OMAP_ENTRIES

# add prefixes that you wish to include 
filter = ["Ke", "Ki", "Mm", "Zw"]

class Symbol():
    def __init__(self, name, offset, size):
        self.name = name
        self.offset = offset
        self.size = 0;

    def __cmp__(self, other):
        if self.offset < other.offset:
            return -1
        elif self.offset == other.offset:
            return 0
        else:
            return 1

    def __str__(self):
        return "===\nName: %s\nOff: %x" % (self.name, self.offset)

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print "Usage: symbol2h.py in.pdb out"
        sys.exit(1)

    # load and parse pdb file
    pdb = pdbparse.parse(sys.argv[1])
    sects = Sections.parse(pdb.streams[10].data)
    gsyms = pdb.streams[pdb.streams[3].gsym_file]
    omap = OMAP_ENTRIES.parse(pdb.streams[12].data)
    # list to store Symbol objs
    syms = []
    i = 0
    remapped = 0
    # parse symbols
    for sym in gsyms.globals:
        off = sym.offset
        try:
            # let's remove some useless stuff
            if('?' in sym.name): continue 
            if('@' == sym.name[0]): continue
            if(sym.name.startswith("__imp__")):
                sym.name = sym.name[7:]
            elif(sym.name.startswith("_")):
                sym.name = sym.name[1:]
            else: continue

            # check if the symbol name starts with one of the desired prefixes
            for prefix in filter:
                if(sym.name.startswith(prefix)):
                    virt_base = sects[sym.segment-1].VirtualAddress
                    remapped = remap(off+virt_base, omap)
                    if remapped != 0:
                        syms.append(Symbol(sym.name, remapped, 0))
                    break

        except IndexError, e: # ignore this symbol
            print e.__str__()
            continue

    # sort symbols by offset
    syms.sort()

    # generate .c file
    out = open(sys.argv[2]+".c", "w")
    out.write("#include \"%s.h\"\n\n" % sys.argv[2])
    out.write("SYMBOL syms[] = {\n")
    for i in range(0, len(syms)):
        if(i == len(syms)-1):
            out.write("{\"%s\", 0x%08x}\n};\n" % (syms[i].name[0:256], syms[i].offset))
        else:
            out.write("{\"%s\", 0x%08x},\n" % (syms[i].name[0:256], syms[i].offset))
    out.write("\nconst ULONG NOS = sizeof(syms) / sizeof(SYMBOL);\n");
    out.close()
