// TODO Combine these if they're on the same line.
// TODO if you stop calling it a string-literal, these error messages will have
// to change too.
MAKE_LOG_ERROR(InvalidEscapedCharacterInStringLiteral,
               "Found an invalid escape sequence in string-literal.")
MAKE_LOG_ERROR(RunawayStringLiteral, "Reached end of line before finding the "
                                     "end of a string-literal. Did you forget "
                                     "a quotation mark?")
MAKE_LOG_ERROR(EscapedDoubleQuoteInCharacterLiteral,
               "The double quotation mark character (\") does not need to be "
               "esacped in a character-literal.")
MAKE_LOG_ERROR(InvalidEscapedCharacterInCharacterLiteral,
               "Encounterd an invalid escape sequence in character-literal. "
               "The valid escape sequences in a character-literal are \\\\, "
               "\\a, \\b, \\f, \\n, \\r, \\s, \\t, and \\v.")
MAKE_LOG_ERROR(RunawayCharacterLiteral,
               "Found a backtick (`), but did not see a character literal.")
MAKE_LOG_ERROR(SpaceInCharacterLiteral, "Found a backtick (`) followed by a "
                                        "space character. Space character "
                                        "literals are written as `\\s.")
MAKE_LOG_ERROR(TabInCharacterLiteral, "Founda tab in your character-literal. "
                                      "Tab charcater literals are written as "
                                      "`\t.")
// TODO lexing things like 1..2 (as a range, but not 1...2 as 1..(.2))
MAKE_LOG_ERROR(TooManyDots, "There are too many consecutive period (.) "
                            "characters. Did you mean just \"..\"?")
MAKE_LOG_ERROR(InvalidCharacterQuestionMark,
               "Question marks (?) are not valid Icarus syntax.")
MAKE_LOG_ERROR(InvalidCharacterTilde, "Tildes (~) are not valid Icarus syntax.")
MAKE_LOG_ERROR(
    NonWhitespaceAfterNewlineEscape,
    "Found a non-whitespace character following a line-continuation ('\\').")
MAKE_LOG_ERROR(
    NotInMultilineComment,
    "Found a token representing the end of a "
    "multi-line comment (*/), but it was not part of a comment block.")

// TODO this is a crappy error message
MAKE_LOG_ERROR(DeclarationUsedAsOrdinaryExpression,
               "Declarations cannot be used as ordinary expressions.")

// TODO handle case where a value is repeated multiple times. build a set and
// highlight them accordingly.
MAKE_LOG_ERROR(RepeatedEnumName, "Repeated enum member.")
MAKE_LOG_ERROR(EnumNeedsIds, "Enum members must be identifiers.")
MAKE_LOG_ERROR(CallingDeclaration, "Declarations cannot be called.")
MAKE_LOG_ERROR(IndexingDeclaration, "Declaration cannot be indexed")
MAKE_LOG_ERROR(DeclarationInIndex, "Declarations cannot appear inside an index")
MAKE_LOG_ERROR(NonDeclarationInStructDeclaration,
               "Each struct member must be defined using a declaration.")
MAKE_LOG_ERROR(CommaListStatement,
               "Comma-separated lists are not allowed as statements")
MAKE_LOG_ERROR(NonInDeclInForLoop, "Expected 'in' declaration in for-loop.")
MAKE_LOG_ERROR(InvalidRequirement, "Require statements must take a string "
                                   "literal as the name of the file to be "
                                   "imported.")
MAKE_LOG_ERROR(RHSNonIdInAccess, "Right-hand side must be an identifier")
MAKE_LOG_ERROR(DeclarationInAccess,
               "Declaration not allowed on left-hand side of dot (.) operator.")
MAKE_LOG_ERROR(UninferrableType,
               "Unable to infer the type of the following expression:")
MAKE_LOG_ERROR(
    NonConstantBindingToConstantDeclaration,
    "Attempting to declare a constant with a non-constant initial value.")

// TODO better text here
MAKE_LOG_ERROR(InferringHole,
               "Attempting to infer the type of an uninitialized value")
