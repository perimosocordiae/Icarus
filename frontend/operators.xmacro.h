OPERATOR_MACRO(Hashtag,        "",        hashtag,   0,    non_assoc)
OPERATOR_MACRO(When,           "when",    op_b,      0,    non_assoc)
OPERATOR_MACRO(Import,         "import",  op_l,      1,    non_assoc)
OPERATOR_MACRO(Return,         "return",  op_lt,     1,    non_assoc)
OPERATOR_MACRO(Yield,          "yield",   op_lt,     1,    non_assoc)
OPERATOR_MACRO(Jump,           "jump",    op_l,      1,    non_assoc)
OPERATOR_MACRO(Needs,          "needs",   op_l,      1,    non_assoc)
OPERATOR_MACRO(Ensure,         "ensure",  op_l,      1,    non_assoc)
OPERATOR_MACRO(Print,          "print",   op_l,      1,    non_assoc)
OPERATOR_MACRO(Eval,           "$",       op_l,      1,    right_assoc)
OPERATOR_MACRO(Comma,          ",",       comma,     2,    chain_assoc)
OPERATOR_MACRO(Expand,         "<<",      op_l,      3,    right_assoc)
OPERATOR_MACRO(Assign,         "=",       eq,        4,    non_assoc)
OPERATOR_MACRO(ColonEq,        ":=",      op_b,      4,    non_assoc)
OPERATOR_MACRO(DoubleColonEq,  "::=",     op_b,      4,    non_assoc)
OPERATOR_MACRO(Colon,          ":",       colon,     5,    non_assoc)
OPERATOR_MACRO(DoubleColon,    "::",      colon,     5,    non_assoc)
OPERATOR_MACRO(OrEq,           "|=",      op_b,      6,    non_assoc)
OPERATOR_MACRO(XorEq,          "^=",      op_b,      7,    non_assoc)
OPERATOR_MACRO(AndEq,          "&=",      op_b,      8,    non_assoc)
OPERATOR_MACRO(AddEq,          "+=",      op_b,      9,    non_assoc)
OPERATOR_MACRO(SubEq,          "-=",      op_b,      9,    non_assoc)
OPERATOR_MACRO(MulEq,          "*=",      op_b,      10,   non_assoc)
OPERATOR_MACRO(DivEq,          "/=",      op_b,      10,   non_assoc)
OPERATOR_MACRO(ModEq,          "%=",      op_b,      10,   non_assoc)
OPERATOR_MACRO(Or,             "|",       op_b,      11,   chain_assoc)
OPERATOR_MACRO(Rocket,         "=>",      op_b,      12,   right_assoc)
OPERATOR_MACRO(Arrow,          "->",      fn_arrow,  12,   right_assoc)
OPERATOR_MACRO(Xor,            "^",       op_b,      13,   chain_assoc)
OPERATOR_MACRO(And,            "&",       op_bl,     14,   chain_assoc)
OPERATOR_MACRO(Lt,             "<",       op_b,      15,   chain_assoc)
OPERATOR_MACRO(Le,             "<=",      op_b,      15,   chain_assoc)
OPERATOR_MACRO(Eq,             "==",      op_b,      15,   chain_assoc)
OPERATOR_MACRO(Ne,             "!=",      op_b,      15,   chain_assoc)
OPERATOR_MACRO(Ge,             ">=",      op_b,      15,   chain_assoc)
OPERATOR_MACRO(Gt,             ">",       op_b,      15,   chain_assoc)
OPERATOR_MACRO(Add,            "+",       op_b,      16,   left_assoc)
OPERATOR_MACRO(Sub,            "-",       op_bl,     16,   left_assoc)
OPERATOR_MACRO(Mul,            "*",       op_bl,     17,   left_assoc)
OPERATOR_MACRO(Div,            "/",       op_b,      17,   left_assoc)
OPERATOR_MACRO(Mod,            "%",       op_b,      17,   left_assoc)
OPERATOR_MACRO(As,             "as",      op_b,      18,   left_assoc)
OPERATOR_MACRO(Copy,           "copy",    op_l,      18,   non_assoc)
OPERATOR_MACRO(Move,           "move",    op_l,      18,   non_assoc)
OPERATOR_MACRO(MatchDecl,      "`",       op_b,      19,   non_assoc)
OPERATOR_MACRO(Which,          "which",   op_l,      20,   non_assoc)
OPERATOR_MACRO(Not,            "!",       op_l,      21,   right_assoc)
OPERATOR_MACRO(TypeOf,         ":?",      op_r,      21,   right_assoc)
OPERATOR_MACRO(BufPtr,         "[*]",     op_l,      21,   right_assoc)
OPERATOR_MACRO(At,             "@",       op_l,      21,   right_assoc)
OPERATOR_MACRO(Index,          "[]",      expr,      22,   chain_assoc)
OPERATOR_MACRO(Call,           "'",       op_bl,     23,   left_assoc)
OPERATOR_MACRO(NotAnOperator,  "_",       eof,      100,  chain_assoc)
