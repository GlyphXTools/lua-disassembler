#ifndef OPCODES50_H
#define OPCODES50_H

/*
** size and position of opcode arguments.
*/
#define SIZE_C          9
#define SIZE_B          9
#define SIZE_Bx         (SIZE_C + SIZE_B)
#define SIZE_A          8

#define SIZE_OP         6

#define POS_C           SIZE_OP
#define POS_B           (POS_C + SIZE_C)
#define POS_Bx          POS_C
#define POS_A           (POS_B + SIZE_B)


/*
** limits for opcode arguments.
** we use (signed) int to manipulate most arguments,
** so they must fit in BITS_INT-1 bits (-1 for sign)
*/
#define MAXARG_Bx        ((1<<SIZE_Bx)-1)
#define MAXARG_sBx        (MAXARG_Bx>>1)         /* `sBx' is signed */


#define MAXARG_A        ((1<<SIZE_A)-1)
#define MAXARG_B        ((1<<SIZE_B)-1)
#define MAXARG_C        ((1<<SIZE_C)-1)


/* creates a mask with `n' 1 bits at position `p' */
#define MASK1(n,p)      ((~((~(Instruction)0)<<n))<<p)

/* creates a mask with `n' 0 bits at position `p' */
#define MASK0(n,p)      (~MASK1(n,p))

/*
** the following macros help to manipulate instructions
*/

#define GET_OPCODE(i)   ((i)&MASK1(SIZE_OP,0))
#define GETARG_A(i)     ((int)((i)>>POS_A))
#define GETARG_B(i)     ((int)(((i)>>POS_B) & MASK1(SIZE_B,0)))
#define GETARG_C(i)     ((int)(((i)>>POS_C) & MASK1(SIZE_C,0)))
#define GETARG_Bx(i)    ((int)(((i)>>POS_Bx) & MASK1(SIZE_Bx,0)))
#define GETARG_sBx(i)   (GETARG_Bx(i)-MAXARG_sBx)

#define MAXSTACK        250
#define GETARG_RKB(i)	((GETARG_B(i) >= MAXSTACK) ? -(int)GETARG_B(i)+MAXSTACK-1 : GETARG_B(i))
#define GETARG_RKC(i)	((GETARG_C(i) >= MAXSTACK) ? -(int)GETARG_C(i)+MAXSTACK-1 : GETARG_C(i))

/*
** R(x) - register
** Kst(x) - constant (in constant table)
** RK(x) == if x < MAXSTACK then R(x) else Kst(x-MAXSTACK)
*/

typedef enum {
/*----------------------------------------------------------------------
name            args    description
------------------------------------------------------------------------*/
OP_MOVE,/*      A B     R(A) := R(B)                                    */
OP_LOADK,/*     A Bx    R(A) := Kst(Bx)                                 */
OP_LOADBOOL,/*  A B C   R(A) := (Bool)B; if (C) PC++                    */
OP_LOADNIL,/*   A B     R(A) := ... := R(B) := nil                      */
OP_GETUPVAL,/*  A B     R(A) := UpValue[B]                              */

OP_GETGLOBAL,/* A Bx    R(A) := Gbl[Kst(Bx)]                            */
OP_GETTABLE,/*  A B C   R(A) := R(B)[RK(C)]                             */

OP_SETGLOBAL,/* A Bx    Gbl[Kst(Bx)] := R(A)                            */
OP_SETUPVAL,/*  A B     UpValue[B] := R(A)                              */
OP_SETTABLE,/*  A B C   R(A)[RK(B)] := RK(C)                            */

OP_NEWTABLE,/*  A B C   R(A) := {} (size = B,C)                         */

OP_SELF,/*      A B C   R(A+1) := R(B); R(A) := R(B)[RK(C)]             */

OP_ADD,/*       A B C   R(A) := RK(B) + RK(C)                           */
OP_SUB,/*       A B C   R(A) := RK(B) - RK(C)                           */
OP_MUL,/*       A B C   R(A) := RK(B) * RK(C)                           */
OP_DIV,/*       A B C   R(A) := RK(B) / RK(C)                           */
OP_POW,/*       A B C   R(A) := RK(B) ^ RK(C)                           */
OP_UNM,/*       A B     R(A) := -R(B)                                   */
OP_NOT,/*       A B     R(A) := not R(B)                                */

OP_CONCAT,/*    A B C   R(A) := R(B).. ... ..R(C)                       */

OP_JMP,/*       sBx     PC += sBx                                       */

OP_EQ,/*        A B C   if ((RK(B) == RK(C)) ~= A) then pc++            */
OP_LT,/*        A B C   if ((RK(B) <  RK(C)) ~= A) then pc++            */
OP_LE,/*        A B C   if ((RK(B) <= RK(C)) ~= A) then pc++            */

OP_TEST,/*      A B C   if (R(B) <=> C) then R(A) := R(B) else pc++     */ 

OP_CALL,/*      A B C   R(A), ... ,R(A+C-2) := R(A)(R(A+1), ... ,R(A+B-1)) */
OP_TAILCALL,/*  A B C   return R(A)(R(A+1), ... ,R(A+B-1))              */
OP_RETURN,/*    A B     return R(A), ... ,R(A+B-2)      (see note)      */

OP_FORLOOP,/*   A sBx   R(A)+=R(A+2); if R(A) <?= R(A+1) then PC+= sBx  */

OP_TFORLOOP,/*  A C     R(A+2), ... ,R(A+2+C) := R(A)(R(A+1), R(A+2)); 
                        if R(A+2) ~= nil then pc++                      */
OP_TFORPREP,/*  A sBx   if type(R(A)) == table then R(A+1):=R(A), R(A):=next;
                        PC += sBx                                       */

OP_SETLIST,/*   A Bx    R(A)[Bx-Bx%FPF+i] := R(A+i), 1 <= i <= Bx%FPF+1 */
OP_SETLISTO,/*  A Bx                                                    */

OP_CLOSE,/*     A       close all variables in the stack up to (>=) R(A)*/
OP_CLOSURE/*    A Bx    R(A) := closure(KPROTO[Bx], R(A), ... ,R(A+n))  */
} OpCode;

#define NUM_OPCODES     ((int)OP_CLOSURE + 1)

/* number of list items to accumulate before a SETLIST instruction */
/* (must be a power of 2) */
#define LFIELDS_PER_FLUSH       32

#endif