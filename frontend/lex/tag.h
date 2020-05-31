#ifndef ICARUS_FRONTEND_LEX_TAG_H
#define ICARUS_FRONTEND_LEX_TAG_H

namespace frontend {

// TODO debug stringify.
enum Tag : uint64_t {
  bof     = 1ull << 0,
  eof     = 1ull << 1,
  newline = 1ull << 2,

  braced_stmts = 1ull << 3,
  scope_expr   = 1ull << 4,
  block_expr   = 1ull << 5,
  fn_call_expr = 1ull << 6,

  stmts         = 1ull << 7,
  expr          = 1ull << 8,
  fn_expr       = 1ull << 9,
  l_paren       = 1ull << 10,
  r_paren       = 1ull << 11,
  l_bracket     = 1ull << 12,
  r_bracket     = 1ull << 13,
  l_brace       = 1ull << 14,
  r_brace       = 1ull << 15,
  l_ref         = 1ull << 16,
  semicolon     = 1ull << 17,
  kw_block_head = 1ull << 18,
  kw_struct     = 1ull << 19,
  hashtag       = 1ull << 20,
  op_r          = 1ull << 21,
  op_l          = 1ull << 22,
  op_b          = 1ull << 23,
  colon         = 1ull << 24,
  eq            = 1ull << 25,
  comma         = 1ull << 26,
  op_bl         = 1ull << 27,
  op_lt         = 1ull << 28,
  fn_arrow      = 1ull << 29,
  kw_block      = 1ull << 30,
  yield         = 1ull << 32,
  label         = 1ull << 33,
  sop_l         = 1ull << 34,
  sop_lt        = 1ull << 35,
};

inline std::string stringify(Tag t) { return std::to_string(t); }

}  // namespace frontend

#endif  // ICARUS_FRONTEND_LEX_TAG_H
