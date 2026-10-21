# Lua disassembler
[![build](https://github.com/GlyphXTools/lua-disassembler/actions/workflows/build.yml/badge.svg)](https://github.com/GlyphXTools/lua-disassembler/actions/workflows/build.yml?query=branch%3Amain)

luadisasm is a lua disassembler for Lua 5.0 and 5.1 object files. It produces semi-human readable lua assembly.

## Usage

Syntax: `luadisasm <src-file>`

This program will read a Lua 5.0 or 5.1 object file and disassemble it to stdout.