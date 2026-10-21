#ifndef LUA_IO_H
#define LUA_IO_H

#include "lua.h"

namespace Lua
{

class Reader
{
    std::istream& m_input;
    size_t        m_sizeNumber;

public:
    void        Read(void* dest, size_t size);
    int         ReadByte();
    int         ReadInt();
    double      ReadNumber();
    std::string ReadString();

    Reader(std::istream& input, size_t sizeNumber);
};

}

#endif