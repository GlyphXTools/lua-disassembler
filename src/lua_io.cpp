#include "lua_io.h"
#include "exceptions.h"
using namespace std;

namespace Lua
{

Reader::Reader(std::istream& input, size_t sizeNumber)
    : m_input(input), m_sizeNumber(sizeNumber)
{
}

void Reader::Read(void* dest, size_t size)
{
    m_input.read((char*)dest, (streamsize)size);
    if (m_input.fail()) {
        throw IOException("Unable to read file");
    }
}

string Reader::ReadString()
{
	int size = ReadInt();
    
    string str;
    if (size > 0)
    {
	    char* data = new char[size + 1];
        try
        {
	        Read(data, size);
            data[size] = '\0';
	        str = data;
	        delete[] data;
        }
        catch (...)
        {
            delete[] data;
            throw;
        }
    }
	return str;
}

int Reader::ReadInt()
{
	int32_t value;
	m_input.read( (char*)&value, sizeof value );
    if (m_input.fail()) {
        throw IOException("Unable to read file");
    }
	return letohl(value);
}

int Reader::ReadByte()
{
	uint8_t value;
	m_input.read( (char*)&value, sizeof value );
    if (m_input.fail()) {
        throw IOException("Unable to read file");
    }
	return value;
}

double Reader::ReadNumber()
{
    if (m_sizeNumber == 4)
    {
	    float value;
        Read(&value, sizeof value);
        return value;
    }

    double value;
    Read(&value, sizeof value);
	return value;
}

static Version DetectFileVersion(Reader& reader)
{
    char signature[4];
    reader.Read(signature, 4);
    int version = reader.ReadByte();

    if (strncmp(signature, "\033Lua", 4) == 0) {
        if (version == 0x50) {
            return LUA_50;
        } else if (version == 0x51) {
            int format = reader.ReadByte();
            if (format == 0) {
                return LUA_51;
            }
        }
    }

    return LUA_UNKNOWN;
}

Version DetectFileVersion(istream& input)
{
    Reader reader(input, 4);
    Version version = DetectFileVersion(reader);
    
    // Reset stream
    input.seekg(0);
    return version;
}

}