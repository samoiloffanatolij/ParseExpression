#pragma once

#include "ftree.h"

namespace pf {

using index_t = uint64_t;
using fn_rslt = std::expected<void, parse_exp::error>;   
using parse_rslt = std::expected<ftree::ftree, parse_exp::error>;

}

namespace parse_exp {

enum class error {
    couldnt_find_operator, couldnt_find_operand,
    couldnt_find_open_brace, couldnt_find_close_brace,
    couldnt_read_num_literal, couldnt_read_string_literal,
    couldnt_find_token, couldnt_find_func_ptr,
    piece_of_ternary_opr,
    semantics_inconsistency,
    incorrect_char, text_isnt_expr
};

std::string get_error_message(error code);
std::string get_error_message(error code, size_t pos);

pf::fn_rslt push_opr(opr::opr pushed, std::stack<ftree::tree_node>& opds);
pf::parse_rslt parse_exp(trims::ex_trim_str expr);

}
