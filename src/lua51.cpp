#include "lua_io.h"
#include "exceptions.h"
#include "opcodes51.h"
#include <sstream>
#include <iomanip>
using namespace std;

namespace Lua {
namespace Lua51 {

//
// Reading
//

// Header structure
#pragma pack(1)
struct Header
{
    char signature[4];
    unsigned char version;
    unsigned char format;
    unsigned char endianness;
    unsigned char sizeInt;
    unsigned char sizeSize_t;
    unsigned char sizeInstruction;
    unsigned char sizeNumber;
    unsigned char integral;
};
#pragma pack()

static Reader ReadHeader(istream& input)
{
    Header header;
	input.read( (char*)&header, sizeof header );
    if (input.fail()) {
        throw IOException("Unable to read file");
    }

    // Validate header
	if ((strncmp(header.signature, "\033Lua", 4) != 0) ||
		(header.version         != 0x51) ||
        (header.format          != 0) ||
		(header.endianness      != 1) ||
		(header.sizeInt			!= 4) ||
		(header.sizeSize_t		!= 4) ||
		(header.sizeInstruction != 4) ||
		(header.sizeNumber      != 4 && header.sizeNumber != 8) ||
		(header.integral        != 0))
	{
		throw BadFileException();
	}

    return Reader(input, header.sizeNumber);
}

static void ReadLines(Reader& reader, vector<Line>& lines)
{
	lines.resize( reader.ReadInt() );
	for (size_t i = 0; i < lines.size(); i++)
	{
		lines[i] = reader.ReadInt();
	}
}

static void ReadLocals(Reader& reader, vector<Local>& locals)
{
	locals.resize( reader.ReadInt() );
	for (size_t i = 0; i < locals.size(); i++)
	{
        Local& local = locals[i];
		local.name    = reader.ReadString();
		local.startPC = reader.ReadInt();
		local.endPC   = reader.ReadInt();
	}
}

static void ReadUpvalues(Reader& reader, vector<UpValue>& upvalues)
{
	upvalues.resize( reader.ReadInt() );
	for (size_t i = 0; i < upvalues.size(); i++)
	{
		upvalues[i] = reader.ReadString();
	}
}

static void ReadConstants(Reader& reader, vector<Constant>& constants)
{
	constants.resize( reader.ReadInt() );
	for (size_t i = 0; i < constants.size(); i++)
	{
		Constant& constant = constants[i];
		constant.type = (Type)reader.ReadByte();
		switch (constant.type)
		{
			case TNUMBER:  constant.number  = reader.ReadNumber(); break;
			case TSTRING:  constant.str     = reader.ReadString(); break;
            case TBOOLEAN: constant.boolean = reader.ReadByte() != 0; break;
			case TNIL:     break;
			default:
				throw BadFileException();
		}
	}
}

static void ReadFunction(Reader& reader, Function& function);

static void ReadFunctions(Reader& reader, vector<Function>& functions)
{
	functions.resize( reader.ReadInt() );
	for (size_t i = 0; i < functions.size(); i++)
	{
		ReadFunction(reader, functions[i]);
	}
}

static void ReadInstructions(Reader& reader, vector<Instruction>& instructions)
{
    instructions.resize( reader.ReadInt() );
	for (size_t i = 0; i < instructions.size(); i++)
	{
		instructions[i] = reader.ReadInt();
	}
}

static void ReadFunction(Reader& reader, Function& function)
{
	function.name            = reader.ReadString();
	function.lineDefined     = reader.ReadInt();
    function.lastLineDefined = reader.ReadInt();
	function.nUpvalues       = reader.ReadByte();
	function.nParameters     = reader.ReadByte();
	function.isVararg        = reader.ReadByte();
	function.maxStackSize    = reader.ReadByte();
	ReadInstructions(reader, function.instructions);
	ReadConstants   (reader, function.constants);
	ReadFunctions   (reader, function.functions);
    ReadLines       (reader, function.lines);
    ReadLocals      (reader, function.locals);
	ReadUpvalues    (reader, function.upvalues);
}

void ReadFile(istream& input, File& file)
{
    Reader reader = ReadHeader(input);
	ReadFunction(reader, file.function);
}

//
// Disassembling
//
static const char* OpcodeNames[NUM_OPCODES] = {
  "MOVE",       "LOADK",        "LOADBOOL",   "LOADNIL",
  "GETUPVAL",   "GETGLOBAL",    "GETTABLE",   "SETGLOBAL",
  "SETUPVAL",   "SETTABLE",     "NEWTABLE",   "SELF",
  "ADD",        "SUB",          "MUL",        "DIV",
  "MOD",        "POW",          "UNM",        "NOT",
  "LEN",        "CONCAT",       "JMP",        "EQ",
  "LT",         "LE",           "TEST",       "TESTSET",
  "CALL",       "TAILCALL",     "RETURN",     "FORLOOP",        
  "FORPREP",    "TFORLOOP",     "SETLIST",    "CLOSE",
  "CLOSURE",    "VARARG"
};

ostream& operator <<(ostream& stream, const Constant& constant)
{
	switch (constant.type)
	{
		case TSTRING:
			stream << '"';
			for (string::const_iterator i = constant.str.begin(); i != constant.str.end(); i++)
			{
				string::value_type ch = *i;
				switch (ch)
				{
					case '\a': stream << "\\a"; break;
					case '\b': stream << "\\b"; break;
					case '\f': stream << "\\f"; break;
					case '\n': stream << "\\n"; break;
					case '\r': stream << "\\r"; break;
					case '\t': stream << "\\t"; break;
					case '\v': stream << "\\v"; break;
					case '\\': case '\'': case '"':
						stream << '\\';
					default:
						stream << ch;
						break;
				}
			}
			stream << '"';
			break;

		case TNUMBER:
			stream << constant.number;
			break;

        case TBOOLEAN:
            stream << constant.boolean;
            break;

		case TNIL:
			stream << "nil";
			break;
	}
	return stream;
}

// PC is 0-based, but we print 1-based
static int PRINTPC(int pc) {
    return pc + 1;
}

static string CODE_RK(int reg)
{
    stringstream code;
    if (reg >= 0) {
        code << "R" << reg;
    } else {
        code << "K" << -(reg + 1);
    }
    return code.str();
}

static string COMMENT_RK(const DisassembleInfo& info, int reg)
{
    stringstream comment;
    if (reg >= 0) {
        comment << "R" << reg;
    } else {
        comment << info.function->constants[-(reg + 1)];
    }
    return comment.str();
}

void Disassemble(const DisassembleInfo& info, std::ostream& code, std::ostream& comment)
{
    int pc = info.pc;
    Instruction instr = info.function->instructions[pc];
    int opcode = GET_OPCODE(instr);
    code << left << setw(9) << OpcodeNames[ opcode ] << " ";

    switch (opcode)
    {
	case OP_MOVE:
		code    << "R" << GETARG_A(instr) << " R" << GETARG_B(instr);
		comment << "R" << GETARG_A(instr) << " := R" << GETARG_B(instr);
		break;

	case OP_LOADK:
		code    << "R" << GETARG_A(instr) << " K" << GETARG_Bx(instr);
		comment << "R" << GETARG_A(instr) << " := " << info.function->constants[GETARG_Bx(instr)];
		break;

	case OP_LOADBOOL:
		code << "R" << GETARG_A(instr) << " " << GETARG_B(instr) << " " << GETARG_C(instr);
		comment << "R" << GETARG_A(instr) << " := " << boolalpha << (GETARG_B(instr) != 0);
		if (GETARG_C(instr))
		{
			comment << "; PC := " << PRINTPC(pc + 2);
		}
		break;

	case OP_LOADNIL:
		code << "R" << GETARG_A(instr) << " R" << GETARG_B(instr);
		for (int i = GETARG_A(instr); i <= GETARG_B(instr); i++)
		{
			comment << "R" << i << " := ";
		}
		comment << "nil";
		break;

	case OP_GETUPVAL:
		code << "R" << GETARG_A(instr) << " " << GETARG_B(instr);
		comment << "R" << GETARG_A(instr) << " := " << "U" << GETARG_B(instr);
		break;

	case OP_GETGLOBAL:
		code    << "R" << GETARG_A(instr) << " K" << GETARG_Bx(instr);
		comment << "R" << GETARG_A(instr) << " := " << info.function->constants[GETARG_Bx(instr)].str;
		break;

	case OP_GETTABLE:
		code << "R" << GETARG_A(instr) << " R" << GETARG_B(instr) << " " << CODE_RK(GETARG_RKC(instr));
		comment << "R" << GETARG_A(instr) << " := R" << GETARG_B(instr) << "["
                << COMMENT_RK(info, GETARG_RKC(instr)) << "]";
		break;

	case OP_SETGLOBAL:
		code    << "R" << GETARG_A(instr) << " K" << GETARG_Bx(instr);
		comment << info.function->constants[GETARG_Bx(instr)].str << " := " << "R" << GETARG_A(instr);
		break;

	case OP_SETUPVAL:
		code << "R" << GETARG_A(instr) << " " << GETARG_B(instr);
		comment << "U" << GETARG_B(instr) << " := R" << GETARG_A(instr);
		break;

	case OP_SETTABLE:
		code << "R" << GETARG_A(instr) << " " << CODE_RK(GETARG_RKB(instr)) << " " << CODE_RK(GETARG_RKC(instr));
		comment << "R" << GETARG_A(instr) << "[" << COMMENT_RK(info, GETARG_RKB(instr)) << "] := "
                << COMMENT_RK(info, GETARG_RKC(instr));
        break;

	case OP_NEWTABLE:
		code << "R" << GETARG_A(instr) << " " << GETARG_B(instr) << " " << GETARG_C(instr);
		comment << "R" << GETARG_A(instr) << " := {}";
		break;

	case OP_SELF:
		code << "R" << GETARG_A(instr) << " R" << GETARG_B(instr) << " " << CODE_RK(GETARG_RKC(instr));
		comment << "R" << GETARG_A(instr) + 1 << " := R" << GETARG_B(instr) << "; "
		        << "R" << GETARG_A(instr) << " := R" << GETARG_B(instr) << "["
                << COMMENT_RK(info, GETARG_RKC(instr)) << "]";
		break;

	case OP_ADD:
	case OP_SUB:
	case OP_MUL:
	case OP_DIV:
    case OP_MOD:
	case OP_POW:
	{
		code << "R" << GETARG_A(instr) << " " << CODE_RK(GETARG_RKB(instr)) << " " << CODE_RK(GETARG_RKC(instr));
		
		comment << "R" << GETARG_A(instr) << " := " << COMMENT_RK(info, GETARG_RKB(instr));
		switch (opcode)
		{
			case OP_ADD: comment << " + "; break;
			case OP_SUB: comment << " - "; break;
			case OP_MUL: comment << " * "; break;
			case OP_DIV: comment << " / "; break;
            case OP_MOD: comment << " % "; break;
			case OP_POW: comment << " ^ "; break;
		}
        comment << COMMENT_RK(info, GETARG_RKC(instr));
		break;
	}

	case OP_UNM:
	case OP_NOT:
	case OP_LEN:
		code << "R" << GETARG_A(instr) << " R" << GETARG_B(instr);
		comment << "R" << GETARG_A(instr) << " := ";
		switch (opcode)
		{
			case OP_UNM: comment << "-"; break;
			case OP_NOT: comment << "not"; break;
            case OP_LEN: comment << "length of"; break;
		}
		comment << " R" << GETARG_B(instr);
		break;

	case OP_CONCAT:
	{
		code << "R" << GETARG_A(instr) << " R" << GETARG_B(instr) << " R" << GETARG_C(instr);
		comment << "R" << GETARG_A(instr) << " := R" << GETARG_B(instr);
		for (int i = GETARG_B(instr) + 1; i <= GETARG_C(instr); i++)
		{
			comment << " .. R" << i;
		}
		break;
	}

	case OP_JMP:
    {
        bool skip = false;
        if (pc > 0)
        {
            // Skip this instruction's meaning if it's preceded
            // by an instruction that jumps over it
            switch(GET_OPCODE(info.function->instructions[pc - 1]))
            {
            case OP_EQ:
	        case OP_LT:
	        case OP_LE:
            case OP_TEST:
            case OP_TESTSET:
            case OP_TFORLOOP:
                skip = true;
            }
        }
		
        code << PRINTPC(pc + 1 + GETARG_sBx(instr));
        if (!skip)
        {
            comment << "PC := " << PRINTPC(pc + 1 + GETARG_sBx(instr));
        }
		break;
    }

	case OP_EQ:
	case OP_LT:
	case OP_LE:
		code << GETARG_A(instr) << " " << CODE_RK(GETARG_RKB(instr)) << " " << CODE_RK(GETARG_RKC(instr));
		comment << "if " << COMMENT_RK(info, GETARG_RKB(instr));
		switch (opcode)
		{
			case OP_EQ: comment << (GETARG_A(instr) ? " == " : " != "); break;
			case OP_LT: comment << (GETARG_A(instr) ? " < "  : " >= "); break;
			case OP_LE: comment << (GETARG_A(instr) ? " <= " : " > ");  break;
		}
		comment << COMMENT_RK(info, GETARG_RKC(instr))
		        << " then PC := " << PRINTPC(pc + GETARG_sBx(info.function->instructions[pc+1]) + 2);
		break;

	case OP_TEST:
		code << "R" << GETARG_A(instr) << " " << GETARG_C(instr);
		comment << "if ";
		if (!GETARG_C(instr))
		{
			comment << "not ";
		}
		comment << "R" << GETARG_A(instr)
		        << " then PC := " << PRINTPC(pc + GETARG_sBx(info.function->instructions[pc+1]) + 2);
		break;

	case OP_TESTSET:
		code << "R" << GETARG_A(instr) << " R" << GETARG_B(instr) << " " << GETARG_C(instr);
		comment << "if ";
		if (!GETARG_C(instr))
		{
			comment << "not ";
		}
		comment << "R" << GETARG_B(instr) << " then begin "
	            << "R" << GETARG_A(instr) << " := R" << GETARG_B(instr) << "; "
                << "PC := " << PRINTPC(pc + GETARG_sBx(info.function->instructions[pc+1]) + 2)
                << " end";
		break;

	case OP_CALL:
		code << "R" << GETARG_A(instr) << " " << GETARG_B(instr) << " " << GETARG_C(instr);
		if (GETARG_C(instr) == 0)
		{
			comment << "R" << GETARG_A(instr) << ",... := ";
		}
		else if (GETARG_C(instr) > 1)
		{
			for (int c = 0; c < GETARG_C(instr) - 1; c++)
			{
				comment << "R" << GETARG_A(instr) + c;
				if (c < GETARG_C(instr) - 2) comment << ",";
			}
			comment << " := ";
		}
		comment << "R" << GETARG_A(instr) << "(";
		if (GETARG_B(instr) == 0)
		{
			comment << "R" << GETARG_A(instr) + 1 << ",...";
		}
		else if (GETARG_B(instr) > 1)
		{
			for (int b = 0; b < GETARG_B(instr) - 1; b++)
			{
				comment << "R" << GETARG_A(instr) + 1 + b;
				if (b < GETARG_B(instr) - 2) comment << ",";
			}
		}
		comment << ")";
		break;

	case OP_TAILCALL:
		code << "R" << GETARG_A(instr) << " " << GETARG_B(instr);

		comment << "return R" << GETARG_A(instr) << "(";
		if (GETARG_B(instr) == 0)
		{
			comment << "R" << GETARG_A(instr) + 1 << ",...";
		}
		else if (GETARG_B(instr) > 1)
		{
			for (int b = 0; b < GETARG_B(instr) - 1; b++)
			{
				comment << "R" << GETARG_A(instr) + 1 + b;
				if (b < GETARG_B(instr) - 2) comment << ",";
			}
		}
		comment << ")";
		break;

	case OP_RETURN:
		code << "R" << GETARG_A(instr) << " " << GETARG_B(instr);

		comment << "return ";
		if (GETARG_B(instr) == 0)
		{
			comment << "R" << GETARG_A(instr) << ",...";
		}
		else if (GETARG_B(instr) > 1)
		{
			for (int b = 0; b < GETARG_B(instr) - 1; b++)
			{
				comment << "R" << GETARG_A(instr) + b;
				if (b < GETARG_B(instr) - 2) comment << ",";
			}
		}
		break;

	case OP_FORLOOP:
		code << "R" << GETARG_A(instr) << " " << PRINTPC(pc + 1 + GETARG_sBx(instr));

		comment << "R" << GETARG_A(instr) << " += R" << GETARG_A(instr) + 2
		        << "; if (R" << GETARG_A(instr) << " <?= R" << GETARG_A(instr) + 1
		        << " then begin PC := " << PRINTPC(pc + 1 + GETARG_sBx(instr)) << "; "
                << "R" << GETARG_A(instr) + 3 << " = R" << GETARG_A(instr) << " end";
		break;

	case OP_FORPREP:
        code << "R" << GETARG_A(instr) << " R" << GETARG_A(instr) + 2 << " " << PRINTPC(pc + 1 + GETARG_sBx(instr));
		comment << "R" << GETARG_A(instr) << " -= R" << GETARG_A(instr) + 2
		        << "; PC := " << PRINTPC(pc + GETARG_sBx(instr) + 1);
		break;

    case OP_TFORLOOP:
		code << "R" << GETARG_A(instr) << " " << GETARG_C(instr);
		for (int c = 0; c < GETARG_C(instr); c++)
		{
			comment << "R" << (GETARG_A(instr) + 3 + c);
			if (c < GETARG_C(instr) - 1) comment << ",";
		}
		comment << " := R" << GETARG_A(instr) << "(R" << GETARG_A(instr) + 1 << ",R" << GETARG_A(instr) + 2 << ")"
                << "; if R" << GETARG_A(instr) + 3 << " ~= nil then begin PC = " << PRINTPC(pc + GETARG_sBx(info.function->instructions[pc+1]) + 2) << "; R" << GETARG_A(instr) + 2 << " := R" << GETARG_A(instr) + 3 << " end";
		break;

	case OP_SETLIST:
		code << "R" << GETARG_A(instr) << " " << GETARG_B(instr) << " " << GETARG_C(instr);

		comment << "R" << GETARG_A(instr) << "[";
        if (GETARG_B(instr) == 0) {
            comment << (GETARG_C(instr) - 1) * LFIELDS_PER_FLUSH + 1 << ",...";
        } else for (int i = 1; i <= GETARG_B(instr); i++)
	    {
		    comment << (GETARG_C(instr) - 1) * LFIELDS_PER_FLUSH + i;
		    if (i < GETARG_B(instr)) comment << ",";
	    }
	    comment << "] := ";
        if (GETARG_B(instr) == 0) {
            comment << "R" << GETARG_A(instr) + 1 << ",...";
        } else for (int i = 1; i <= GETARG_B(instr); i++)
	    {
		    comment << "R" << GETARG_A(instr) + i;
		    if (i < GETARG_B(instr)) comment << ",";
	    }
		break;

	case OP_CLOSE:
		code << "R" << GETARG_A(instr);
		comment << "close R" << GETARG_A(instr) << ",...";
		break;

	case OP_CLOSURE:
		code << "R" << GETARG_A(instr) << " " << GETARG_Bx(instr);
		comment << "R" << GETARG_A(instr) << " := closure("
		        << "Function #" << info.level << GETARG_Bx(instr);
        for (int i = 0; i < info.function->functions[GETARG_Bx(instr)].nUpvalues; i++)
		{
            Instruction next = info.function->instructions[pc + 1 + i];
            if (GET_OPCODE(next) == OP_GETUPVAL) {
    			comment << ", U" << GETARG_B(next);
            } else {
			    comment << ", R" << GETARG_B(next);
            }
		}
		comment << ")";
		break;

    case OP_VARARG:
		code << "R" << GETARG_A(instr) << " " << GETARG_B(instr);
        if (GETARG_B(instr) == 0) {
		    comment << "R" << GETARG_A(instr) << ",...";
        }
        for (int i = 0; i < GETARG_B(instr) - 1; i++)
        {
		    comment << "R" << GETARG_A(instr) + i;
            if (i + 1 < GETARG_B(instr) - 1) comment << ",";
        }
        comment << " := vararg";
		break;
    }

}

}
}