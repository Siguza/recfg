# recfg

Reconfig engine sequence parsing.

### Building

    make

### CLI

    recfg dump              # Parse dump as raw reconfig sequence
    recfg dump 0x4560       # Start parsing at offset 0x4560 to end of file
    recfg dump 0c4560 0x40  # Start parsing at offset 0x4560 0x45a0
    recfg -s iBoot          # Auto-find reconfig sequences in iBoot image
    recfg -s iBoot 0x1000   # Look for iBoot at offset 0x1000

### API

`recfg.c` can be used as standalone library for walking and/or patching reconfig sequences.  
See `recfg.h` for documentation and `main.c` for example implementation.

### License

[MPL2](https://github.com/Siguza/recfg/blob/master/LICENSE) with Exhibit B.
