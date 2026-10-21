#include "lua.h"
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <ctime>
#include <fstream>
#include <sstream>
#include <stdexcept>
using namespace std;

static const struct {
    const char* name;
    void (*Load)(istream& input, Lua::File& file);
    void (*Disassemble)(const Lua::DisassembleInfo& info, ostream& code, ostream& meaning);
} LuaFormats[] = {
    {"Lua 5.0", Lua::Lua50::ReadFile, Lua::Lua50::Disassemble},
    {"Lua 5.1", Lua::Lua51::ReadFile, Lua::Lua51::Disassemble},
    {NULL}
};

static void DisassembleFunction(const Lua::Function& function, Lua::Version version, const string& level = "")
{
    // Print function information
	cout << "; Name:            " << function.name << endl;
	cout << "; Defined at line: " << function.lineDefined << endl;
	cout << "; #Upvalues:       " << (int)function.nUpvalues << endl;
	cout << "; #Parameters:     " << (int)function.nParameters;
	if (function.nParameters > 0)
	{
		cout << " (";
		for (int i = 0; i < function.nParameters; i++)
		{
			cout << "R" << i;
			if (i < function.nParameters - 1) cout << ", ";
		}
		cout << ")";
	}
	cout << endl;
	cout << "; Is_vararg:       " << (int)function.isVararg << endl;
	cout << "; Max Stack Size:  " << (int)function.maxStackSize << endl;
	cout << ";" << endl;

	if (function.locals.size() == 0)
	{
		cout << "; No locals information" << endl;
	}
	else
	{
		cout << "; Locals:" << endl;
		cout << "; Start PC |  End PC  | Name" << endl;
		for (size_t i = 0; i < function.locals.size(); i++)
		{
			cout << "; " << hex << setfill('0') ;
			cout << setw(8) << function.locals[i].startPC << " | ";
			cout << setw(8) << function.locals[i].endPC   << " | ";
			cout << function.locals[i].name << endl;
		}
		cout << ";" << endl;
	}

	if (function.upvalues.size() == 0)
	{
		cout << "; No upvalues" << endl;
	}
	else
	{
		cout << "; Upvalues:" << endl;
		for (size_t i = 0; i < function.upvalues.size(); i++)
		{
			cout << "; " << function.upvalues[i] << endl;
		}
		cout << ";" << endl;
	}
	cout << endl;

    // Get line number width from total #lines
    int lineNumberWidth = -1;
    if (!function.lines.empty())
    {
        lineNumberWidth = (int)log10((float)*max_element(function.lines.begin(), function.lines.end())) + 1;
    }

    // Disassemble instructions
    Lua::DisassembleInfo info;
    info.function = &function;
    info.level    = level;
	for (info.pc = 0; info.pc < function.instructions.size(); info.pc++)
	{
        stringstream code;
		stringstream comment;

        comment << boolalpha;

        // Disassemble instruction
        LuaFormats[version].Disassemble(info, code, comment);

        // Print PC and line no.
		cout << dec << right << setfill(' ') << setw(3) << info.pc + 1 << " [";
        if (lineNumberWidth < 1) {
            cout << "-";
        } else {
            cout << setw(lineNumberWidth) << function.lines[info.pc];
        }
        cout << "]: ";

        // Print code
        string codestr = code.str();
		cout << codestr;

        // Print comment
        string commentstr = comment.str();
        if (!commentstr.empty())
		{
			cout << right << setw(25 - (int)codestr.length()) << "; " << commentstr;
		}

		cout << endl;
	}

	for (size_t i = 0; i < function.functions.size(); i++)
	{
		stringstream newlevel;
		newlevel << level << (int)i;
		cout << endl << endl << "; Function #" << newlevel.str() << ":" << endl << ";" << endl;
		DisassembleFunction(function.functions[i], version, newlevel.str() + ".");
	}
}

static void Disassemble(const Lua::File& file, Lua::Version version)
{
    DisassembleFunction(file.function, version);
}

int main(int argc, char* argv[])
{
    // Parse the arguments
	if (argc != 2)
	{
		cerr << "Lua 5 disassembler 1.2, by Mike Lankamp." << endl
		     << "Syntax: luadisasm <src-file>" << endl
             << endl
		     << "The program will read a Lua 5.0 or 5.1 object file and disassemble it to" << endl
		     << "stdout." << endl;
        return 1;
	}

    const char* src  = argv[1];

#ifdef NDEBUG
	try
#endif
	{
		ifstream input(src, ios_base::binary | ios_base::in);
		if (!input.is_open())
		{
			cerr << "Unable to open input file \"" << src << "\"" << endl;
            return 1;
		}

        Lua::Version version = Lua::DetectFileVersion(input);
        if (version == Lua::LUA_UNKNOWN)
        {
            cerr << "Input file is not recognized as a supported Lua file" << endl;
            return 1;
        }

        Lua::File file;
        LuaFormats[version].Load(input, file);
        input.close();

        time_t now = time(NULL);

        cout << "; This file has been disassembled by the Lua 5 disassembler by Mike Lankamp" << endl
             << ";" << endl
             << "; Original file: " << src << endl
             << "; File format:   " << LuaFormats[version].name << endl
             << "; Disassembled:  " << ctime(&now)
             << ";" << endl
             << endl;

        Disassemble(file, version);
	}
#ifdef NDEBUG
	catch (exception& e)
	{
		cerr << e.what() << endl;
        return 1;
	}
#endif
	return 0;
}