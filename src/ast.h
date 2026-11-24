/* ast.h - syntax tree, semantic types and symbols */
#ifndef PPC_AST_H
#define PPC_AST_H

#include "util.h"

typedef struct Type Type;
typedef struct Struct Struct;
typedef struct Routine Routine;
typedef struct Var Var;
typedef struct Expr Expr;
typedef struct Stmt Stmt;
typedef struct TypeExpr TypeExpr;
typedef struct EnumInfo EnumInfo;
typedef struct Block Block;
typedef struct Field Field;

typedef VEC(Expr *) ExprVec;
typedef VEC(Stmt *) StmtVec;

/* identifiers are case-sensitive: name is the spelling from the source */
typedef struct {
    char *name;
    Pos pos;
} Ident;
typedef VEC(Ident) IdentVec;

/* ------------------------------------------------------------------ syntax */

enum { PM_VALUE, PM_VAR, PM_CONST, PM_OUT, PM_OWNED, PM_CONSTREF };
enum { RK_PROC, RK_FUNC, RK_CTOR, RK_DTOR };
enum { VIRT_NONE, VIRT_VIRTUAL, VIRT_OVERRIDE };
enum { SK_RECORD, SK_CLASS };

typedef struct {
    IdentVec names;
    TypeExpr *ty;
    int borrowed;
} FieldDecl;
typedef VEC(FieldDecl) FieldDeclVec;

typedef struct {
    int mode;
    IdentVec names;
    TypeExpr *ty;
} ParamGroup;
typedef VEC(ParamGroup) ParamGroupVec;

typedef struct {
    int kind;
    Ident name;
    ParamGroupVec params;
    TypeExpr *ret;
    int ret_borrowed;
    int virt;
    int is_abstract;
    int is_const;  /* '; const;' */
    int is_classm; /* class function / class procedure */
} MethodDecl;

typedef struct {
    Ident name;
    TypeExpr *ty;
    Ident read, write;
    int has_read, has_write;
} PropDecl;

typedef struct {
    int kind; /* SK_CLASS */
    Ident parent;
    int has_parent;
    FieldDeclVec fields;
    VEC(MethodDecl) methods;
    VEC(PropDecl) props;
} StructDecl;

typedef enum { TE_NAMED, TE_PTR, TE_ARRAY, TE_SUBRANGE, TE_ENUM, TE_RECORD, TE_STRUCT, TE_CLASSFWD, TE_STR } TEKind;

struct TypeExpr {
    TEKind kind;
    Pos pos;
    Ident name;               /* TE_NAMED, TE_PTR target */
    VEC(TypeExpr *) ranges;   /* TE_ARRAY */
    TypeExpr *elem;           /* TE_ARRAY */
    Expr *lo, *hi;            /* TE_SUBRANGE */
    IdentVec enums;           /* TE_ENUM */
    FieldDeclVec fields;      /* TE_RECORD */
    StructDecl *sd;           /* TE_STRUCT */
    int nilable;              /* 'optional' modifier */
    int readonly;             /* 'const T': read-only object reference */
};

typedef struct {
    Ident name;
    TypeExpr *ty;
    Expr *value;
} ConstDecl;
typedef struct {
    Ident name;
    TypeExpr *ty;
} TypeDecl;
typedef struct {
    IdentVec names;
    TypeExpr *ty;
    Expr *init;
    int borrowed;
    int is_heap; /* declared in a 'var' section */
    Expr *ctor;  /* 'x: TFoo.Create(args)': created/constructed at the declaration */
} VarDecl;

typedef struct {
    int kind;
    Ident cls;
    int has_cls;
    Ident name;
    int has_params;
    ParamGroupVec params;
    TypeExpr *ret;
    int ret_borrowed;
    int forward;
    int is_const;  /* '; const;' */
    int is_classm; /* class function / class procedure */
    Block *body;
    Pos pos;
} RoutineDecl;

typedef enum { D_CONST, D_TYPE, D_VAR, D_ROUTINE } DeclKind;
typedef struct {
    DeclKind kind;
    VEC(ConstDecl) consts;
    VEC(TypeDecl) types;
    VEC(VarDecl) vars;
    RoutineDecl *rt;
} Decl;

struct Block {
    VEC(Decl *) decls;
    StmtVec body;
};

/* ------------------------------------------------------------ expressions */

typedef enum {
    /* parsed forms */
    E_INT, E_REAL, E_STR, E_NIL, E_IDENT, E_MEMBER, E_INDEX, E_CALL, E_DEREF, E_ADDR,
    E_UNARY, E_BINARY, E_IN, E_IS, E_AS, E_SET, E_INHERITED, E_FMT, E_LIST,
    /* resolved forms (produced by sema) */
    E_VAR,     /* variable reference: var */
    E_FIELD,   /* a.field */
    E_RCALL,   /* routine call: rt, a = receiver (or NULL), args */
    E_BUILTIN, /* builtin: bi, args */
    E_CONV,    /* implicit conversion of a to type */
    E_CAST,    /* explicit typecast T(a) */
    E_TYPEREF, /* a type used in expression position: ctype */
    E_ISSOME,  /* a is some id: unpacks an optional into the new variable var */
    E_SOMEPAT, /* 'some id' as a case label */
    E_RECLIT,  /* record literal (F: v; G: w): fnames, args */
    E_DEFAULT  /* 'default': the zero value of the expected type */
} ExprKind;

enum { OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_IDIV, OP_MOD, OP_AND, OP_OR, OP_XOR, OP_SHL, OP_SHR,
       OP_EQ, OP_NE, OP_LT, OP_LE, OP_GT, OP_GE, OP_NEG, OP_PLUS, OP_NOT };

/* call modes for E_RCALL */
enum { CM_NORMAL, CM_STATIC, CM_VIRTUAL, CM_INHERITED, CM_NEWCLASS, CM_CTOR_INST };

struct Expr {
    ExprKind kind;
    Pos pos;
    long long ival;
    double rval;
    char *sval;
    int slen;
    Ident id;
    int has_id;
    Expr *a, *b, *c;
    ExprVec args;
    ExprVec args2; /* set literal: upper bounds (NULL entries for single values) */
    IdentVec fnames; /* record literal: field names */
    int op;
    int has_args;
    /* annotations */
    Type *type;
    Var *var;
    Field *field;
    Routine *rt;
    int cmode;
    Type *ctype;
    int bi;
};

/* ------------------------------------------------------------- statements */

typedef enum {
    S_EMPTY, S_ASSIGN, S_CALL, S_COMPOUND, S_IF, S_WHILE, S_REPEAT, S_FOR, S_CASE, S_WITH,
    S_BREAK, S_CONTINUE, S_EXIT, S_BLOCK
} StmtKind;

typedef struct {
    ExprVec lo, hi; /* hi entries may be NULL */
    Stmt *body;
} CaseArm;

struct Stmt {
    StmtKind kind;
    Pos pos;
    Expr *a, *b, *c; /* assign: a := b; if/while: a cond; for: c var, a from, b to; exit: a */
    Stmt *s1, *s2;
    int bind; /* S_ASSIGN: bind a reference variable (var x: Integer := y) instead of assigning */
    StmtVec body;
    Ident var;
    int down;
    int decl_var; /* for val i := ...: the loop declares its counter */
    VEC(CaseArm) arms;
    StmtVec else_;
    int has_else;
    ExprVec exprs;    /* with */
    VEC(Var *) wvars; /* with temporaries; S_BLOCK: the block's variables */
    Block *blk;       /* S_BLOCK: declarations and body */
};

/* ------------------------------------------------------------- semantics */

typedef enum { T_VOID, T_INT, T_BOOL, T_CHAR, T_REAL, T_STR, T_PTR, T_ARRAY, T_RECORD, T_CLASS, T_OPT, T_LIT, T_ENUM, T_NIL } TypeKind;
enum { IK_I8, IK_U8, IK_I16, IK_U16, IK_I32, IK_U32, IK_I64 };

struct Type {
    TypeKind kind;
    int ik;
    Type *base;  /* T_PTR target (NULL = untyped Pointer), T_ARRAY element */
    long long lo, hi;
    Type *index; /* T_ARRAY index type */
    Struct *st;
    EnumInfo *en;
    char *name;
    int nilable; /* optional class type: may be none (represented as NULL) */
    int readonly; /* class type 'const T': the object can only be read through this reference */
};

struct Field {
    char *name;
    char *cname;
    Type *type;
    int borrowed;
    Struct *owner;
};

typedef struct {
    char *name;
    Type *type;
    char *read, *write;
    Pos pos;
} Prop;

typedef struct {
    char *name;
    Routine *intro;
    Routine *impl;
} Slot;

struct Struct {
    int id;
    char *name;
    int kind;
    Struct *parent;
    VEC(Field *) fields; /* flattened, inherited first */
    VEC(Routine *) methods; /* own methods */
    VEC(Prop *) props;
    VEC(Slot) slots;
    int has_vmt;
    int defined;
    int emitted;
    Type *type;
    Pos pos;
};

struct EnumInfo {
    int id;
    char *name;
    VEC(char *) names;
};

enum { VK_LOCAL, VK_PARAM, VK_SELF, VK_RESULT, VK_WITH, VK_GLOBAL };

struct Var {
    char *name;
    char *cname;
    Type *type;
    int level;
    int in_frame;
    int is_ref;   /* C variable holds a pointer to the value */
    int is_global;
    int borrowed; /* borrow checker: non-owning reference */
    int vkind;
    int mode;     /* parameter mode */
    int is_const; /* const parameter / for-loop var */
    int is_stack; /* class variable in a 'val' section: object lives in the stack frame */
    int is_box;   /* value variable in a 'var' section: value lives on the heap (like Box<T>) */
    int pending;  /* declared without a type: the first assignment decides it */
    int decl_heap; /* declared in a 'vao' section (owning) */
    int is_alias;  /* 'var x: T := y' for a value type: x refers to y */
    Routine *owner;
    int id;
    Pos pos;
    int bslot; /* borrow checker: slot index (+1) in the current unit, 0 = untracked */
};

typedef struct {
    char *name;
    Type *type;
    int mode;
    Var *var;
} Param;

struct Routine {
    int id;
    char *name;
    char *cname;
    int kind;
    VEC(Param) params;
    Type *ret;
    int ret_borrowed;
    int level;
    Routine *parent;
    Struct *owner;
    int virt;
    int slot;
    int is_abstract;
    int is_constm; /* '; const;': method does not change Self, routine has no side effects */
    int is_classm; /* class method: no Self, called as TClass.Name(...) */
    int has_frame;
    int defined;
    int forward;
    int prelude;
    Var *result;
    Var *self;
    VEC(Var *) locals; /* locals declared in the routine (not params) */
    StmtVec body;
    Pos pos;
    VEC(Var *) nl_writes;   /* non-local variables written by this routine */
    VEC(Routine *) callees; /* routines called directly */
};

typedef struct {
    VEC(Routine *) routines;
    VEC(Var *) globals;
    StmtVec init;
    StmtVec body;
    VEC(Struct *) structs;
    VEC(EnumInfo *) enums;
    Struct *tobject;
    char *name;
    Pos end_pos;
} Program;

/* builtins */
enum {
    BI_NONE, BI_WRITE, BI_WRITELN, BI_READ, BI_READLN, BI_LENGTH, BI_ORD, BI_CHR, BI_SUCC, BI_PRED,
    BI_INC, BI_DEC, BI_ABS, BI_SQR, BI_SQRT, BI_SIN, BI_COS, BI_ARCTAN, BI_EXP, BI_LN, BI_TRUNC,
    BI_ROUND, BI_FRAC, BI_INT, BI_ODD, BI_HIGH, BI_LOW, BI_SIZEOF, BI_COPY, BI_POS, BI_DELETE,
    BI_INSERT, BI_INTTOSTR, BI_STRTOINT, BI_FLOATTOSTR, BI_STR, BI_UPCASE, BI_UPPERCASE,
    BI_LOWERCASE, BI_CONCAT, BI_HALT, BI_RANDOM, BI_RANDOMIZE, BI_ASSIGNED, BI_NEW, BI_DISPOSE,
    BI_FREEANDNIL, BI_TAKE, BI_SETLENGTH, BI_CLASSNAME, BI_PI, BI_EXIT, BI_BREAK, BI_CONTINUE,
    BI_HASVALUE, BI_VALUEOR, BI_BOXNEW, BI_BOXFREE
};

/* parser.c */
Block *parse_program(const char *src, char **name, Pos *end_pos);
Block *parse_decls_only(const char *src);
Expr *new_expr(ExprKind k, Pos p);
Stmt *new_stmt(StmtKind k, Pos p);

/* sema.c */
Program *sema_program(Block *prelude, Block *prog, const char *name);
int is_owning_type(Type *t);
int struct_descends(Struct *d, Struct *b);
const char *type_name(Type *t);

/* borrow.c */
int borrow_check(Program *p);
extern int g_borrow_check, g_nil_check;
int is_ref_type(Type *t);
int is_owning_temp(Expr *e);

/* codegen.c */
void codegen(Program *p, FILE *out);

#endif
