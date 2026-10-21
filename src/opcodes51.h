#ifndef OPCODES51_H
#define OPCODES51_H

/*
 * size and position of opcode arguments.
 */
#define SIZE_C          9
#define SIZE_B          9
#define SIZE_Bx         (SIZE_C + SIZE_B)
#define SIZE_A          8

#define SIZE_OP         6

#define POS_OP          0
#define POS_A           (POS_OP + SIZE_OP)
#define POS_C           (POS_A + SIZE_A)
#define POS_B           (POS_C + SIZE_C)
#define POS_Bx          POS_C

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
#define GETARG_A(i)     ((int)(((i)>>POS_A) & MASK1(SIZE_A,0)))
#define GETARG_B(i)     ((int)(((i)>>POS_B) & MASK1(SIZE_B,0)))
#define GETARG_C(i)     ((int)(((i)>>POS_C) & MASK1(SIZE_C,0)))
#define GETARG_Bx(i)    ((int)(((i)>>POS_Bx) & MASK1(SIZE_Bx,0)))
#define GETARG_sBx(i)   (GETARG_Bx(i)-MAXARG_sBx)

/* this bit 1 means constant (0 means register) */
#define BITRK           (1 << (SIZE_B - 1))

/* test whether value is a constant */
#define ISK(x)          ((x) & BITRK)

/* gets the index of the constant */
#define INDEXK(r)       ((int)(r) & ~BITRK)

#define GETARG_RKB(i)	((ISK(GETARG_B(i))) ? -(int)INDEXK(GETARG_B(i))-1 : GETARG_B(i))
#define GETARG_RKC(i)	((ISK(GETARG_C(i))) ? -(int)INDEXK(GETARG_C(i))-1 : GETARG_C(i))

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
OP_LOADBOOL,/*  A B C   R(A) := (Bool)B; if (C) pc++                    */
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
OP_MOD,/*       A B C   R(A) := RK(B) % RK(C)                           */
OP_POW,/*       A B C   R(A) := RK(B) ^ RK(C)                           */
OP_UNM,/*       A B     R(A) := -R(B)                                   */
OP_NOT,/*       A B     R(A) := not R(B)                                */
OP_LEN,/*       A B     R(A) := length of R(B)                          */

OP_CONCAT,/*    A B C   R(A) := R(B).. ... ..R(C)                       */

OP_JMP,/*       sBx     pc+=sBx                                 */

OP_EQ,/*        A B C   if ((RK(B) == RK(C)) ~= A) then pc++            */
OP_LT,/*        A B C   if ((RK(B) <  RK(C)) ~= A) then pc++            */
OP_LE,/*        A B C   if ((RK(B) <= RK(C)) ~= A) then pc++            */

OP_TEST,/*      A C     if not (R(A) <=> C) then pc++                   */ 
OP_TESTSET,/*   A B C   if (R(B) <=> C) then R(A) := R(B) else pc++     */ 

OP_CALL,/*      A B C   R(A), ... ,R(A+C-2) := R(A)(R(A+1), ... ,R(A+B-1)) */
OP_TAILCALL,/*  A B C   return R(A)(R(A+1), ... ,R(A+B-1))              */
OP_RETURN,/*    A B     return R(A), ... ,R(A+B-2)      (see note)      */

OP_FORLOOP,/*   A sBx   R(A)+=R(A+2);
                        if R(A) <?= R(A+1) then { pc+=sBx; R(A+3)=R(A) }*/
OP_FORPREP,/*   A sBx   R(A)-=R(A+2); pc+=sBx                           */

OP_TFORLOOP,/*  A C     R(A+3), ... ,R(A+3+C) := R(A)(R(A+1), R(A+2)); 
                        if R(A+3) ~= nil then { pc++; R(A+2)=R(A+3); }  */ 
OP_SETLIST,/*   A B C   R(A)[(C-1)*FPF+i] := R(A+i), 1 <= i <= B        */

OP_CLOSE,/*     A       close all variables in the stack up to (>=) R(A)*/
OP_CLOSURE,/*   A Bx    R(A) := closure(KPROTO[Bx], R(A), ... ,R(A+n))  */

OP_VARARG/*     A B     R(A), R(A+1), ..., R(A+B-1) = vararg            */
} OpCode;


#define NUM_OPCODES ((int)OP_VARARG + 1)

/* number of list items to accumulate before a SETLIST instruction */
#define LFIELDS_PER_FLUSH       50

#endif