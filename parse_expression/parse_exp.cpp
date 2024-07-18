#include <stack>

#include "include/parse_exp.h"

namespace parse_exp {

std::string get_error_message(error code) {
    if (code == error::couldnt_find_open_brace)
        return "Couldnt find open brace";
    if (code == error::couldnt_find_close_brace)
        return "Couldnt find close brace";
    if (code == error::couldnt_find_operator)
        return "Couldnt find operator";
    if (code == error::couldnt_find_operand)
        return "Couldnt find operand";
    if (code == error::couldnt_read_num_literal)
        return "Couldnt read number literal";
    if (code == error::couldnt_read_string_literal)
        return "Couldnt read string literal";
    if (code == error::couldnt_find_token)
        return "Couldnt find token";
    if (code == error::couldnt_find_func_ptr)
        return "Couldnt find func ptr";
    if (code == error::piece_of_ternary_opr)
        return "Only a piece of ternary operator in expression";
    if (code == error::semantics_inconsistency)
        return "Semantics inconsistency";
    if (code == error::incorrect_char)
        return "Incorrect char in expression";
    if (code == error::text_isnt_expr)
        return "Text isnt expression";
}

std::string get_error_message(error code, size_t pos) {
    if (code == error::couldnt_find_open_brace)
        return "Couldnt find open brace before " + std::to_string(pos);
    if (code == error::couldnt_find_close_brace)
        return "Couldnt find close brace after " + std::to_string(pos);
    if (code == error::couldnt_find_operator)
        return "Couldnt find operator for operand in " + std::to_string(pos);
    if (code == error::couldnt_find_operand)
        return "Couldnt find operand for operator in " + std::to_string(pos);
    if (code == error::couldnt_read_num_literal)
        return "Couldnt read number literal in " + std::to_string(pos);
    if (code == error::couldnt_read_string_literal)
        return "Couldnt read string literal in " + std::to_string(pos);
    if (code == error::couldnt_find_token)
        return "Couldnt find token before " + std::to_string(pos);
    if (code == error::couldnt_find_func_ptr)
        return "Couldnt find func ptr before " + std::to_string(pos);
    if (code == error::piece_of_ternary_opr)
        return "Only a piece of ternary operator in " + std::to_string(pos);
    if (code == error::semantics_inconsistency)
        return "Semantics inconsistency in " + std::to_string(pos);
    if (code == error::incorrect_char)
        return "Incorrect char in " + std::to_string(pos);
}

pf::fn_rslt push_opr(opr::opr pushed, std::stack<ftree::tree_node>& opds) {
    using t_opr = opr::ternary_opr;
    if (std::holds_alternative<opr::unary_opr>(pushed)) {
        opr::unary_opr u_opr = std::get<opr::unary_opr>(pushed);
        if (!opds.size())
            return std::unexpected(error::couldnt_find_operand);
        if (std::holds_alternative<ftree::_tree_node::ternary_node<t_opr::ways>>(opds.top()))
            return std::unexpected(error::piece_of_ternary_opr);
        ftree::_tree_node::unary_node u_node(u_opr, std::move(opds.top()));
        opds.pop();
        opds.push(std::move(u_node));
    } else if (std::holds_alternative<opr::binary_opr>(pushed)) {
        opr::binary_opr bin_opr = std::get<opr::binary_opr>(pushed);
        if (opds.size() < 2)
            return std::unexpected(error::couldnt_find_operand);
        if (std::holds_alternative<ftree::_tree_node::ternary_node<t_opr::ways>>(opds.top()))
            return std::unexpected(error::piece_of_ternary_opr);
        ftree::tree_node _node(std::move(opds.top()));
        opds.pop();
        if (std::holds_alternative<ftree::_tree_node::ternary_node<t_opr::ways>>(opds.top()))
            return std::unexpected(error::piece_of_ternary_opr);
        ftree::_tree_node::binary_node bin_node(bin_opr, std::move(opds.top()), std::move(_node));
        opds.pop();
        opds.push(std::move(bin_node));
    } else if (std::holds_alternative<t_opr>(pushed)) {
        t_opr tern_opr = std::get<t_opr>(pushed);
        if (opds.size() < 2)
            return std::unexpected(error::couldnt_find_operand);
        if (tern_opr == t_opr::ways) {
            if (std::holds_alternative<ftree::_tree_node::ternary_node<t_opr::ways>>(opds.top()))
                return std::unexpected(error::piece_of_ternary_opr);
            ftree::tree_node _node(std::move(opds.top()));
            opds.pop();
            if (std::holds_alternative<ftree::_tree_node::ternary_node<t_opr::ways>>(opds.top()))
                return std::unexpected(error::piece_of_ternary_opr);
            ftree::_tree_node::ternary_node<t_opr::ways> t_node(std::move(opds.top()), std::move(_node));
            opds.pop();
            opds.push(std::move(t_node));
        } else if (tern_opr == t_opr::condition) {
            if (!std::holds_alternative<ftree::_tree_node::ternary_node<t_opr::ways>>(opds.top()))
                return std::unexpected(error::piece_of_ternary_opr);
            ftree::_tree_node::ternary_node<t_opr::ways> _node(
                    std::move(std::get<ftree::_tree_node::ternary_node<t_opr::ways>>(opds.top())));
            opds.pop();
            if (std::holds_alternative<ftree::_tree_node::ternary_node<t_opr::ways>>(opds.top()))
                return std::unexpected(error::piece_of_ternary_opr);
            ftree::_tree_node::ternary_node<t_opr::condition> t_node(std::move(opds.top()), std::move(_node));
            opds.pop();
            opds.push(std::move(t_node));
        }
    }
}

pf::parse_rslt parse_exp(trims::ex_trim_str expr) {
    namespace tn = ftree::_tree_node;
    bool is_node = false;
    std::stack<ftree::tree_node> opds;
    std::stack<std::variant<opr::opr, char>> oprs;
    for (tf::index_t i = 0; i < expr.size(); ++i) {
        if (tf::ptf::is_space(expr[i])) {
            i = expr.apply(tf::trim_spaces).pos();
        } else if (tf::ptf::is_linebreak(expr[i])) {
            i = expr.apply(trim_while_true_t(tf::ptf::is_linebreak)).pos();
        } else if (tf::ptf::is_semicolon(expr[i])) {
            if (*tf::trim_while_true(expr, i + 1, [](int c) -> int {
                    return tf::ptf::is_semicolon(c) || tf::ptf::is_linebreak(c); }) != expr.size())
                return std::unexpected(error::text_isnt_expr);
            break;
        } else if (tf::ptf::is_alph_num(expr[i])) {
            expr.extract_next();
            if (expr.apply(tf::ptf::trim_num_literal).err())
                return std::unexpected(error::couldnt_read_num_literal);
            i = expr.pos();
            opds.push(tn::ftree_leaf(tn::leaf_type::num_literal, expr.pop_extracted()));
            is_node = true;
        } else if (tf::ptf::is_quote(expr[i])) {
            expr.extract_next();
            if (expr.apply(tf::ptf::trim_string_literal).err())
                return std::unexpected(error::couldnt_read_string_literal);
            i = expr.pos();
            opds.push(tn::ftree_leaf(tn::leaf_type::str_literal, expr.pop_extracted()));
            is_node = true;
        } else if (tf::ptf::is_alpha(expr[i])) {
            expr.extract_next();
            i = expr.apply(tf::ptf::trim_token).pos();
            opds.push(tn::ftree_leaf(tn::leaf_type::var, expr.pop_extracted()));
            is_node = true;
        } else if (tf::ptf::is_open_brace(expr[i])) {
            expr.save_pos(tf::tags::chain);
            i = (expr[i] == '(' ? expr.apply(trim_char_t('(')).pos() : expr.apply(trim_char_t('[')).pos());
            if (is_node) {
                ftree::tree_node _node = std::move(opds.top());
                opds.pop();
                expr.extract_next();
                if (expr.apply(expr[i - 1] == '('
                        ? trim_while_false_t([](int c) -> int { return c == ')'; })
                        : trim_while_false_t([](int c) -> int { return c == ']'; })
                        ).pos() == expr.size())
                    return std::unexpected(error::couldnt_find_close_brace);
                i = (expr[expr.pop_saved()] == '('
                        ? expr.apply(trim_char_t(')')).pos()
                        : expr.apply(trim_char_t(']')).pos()
                    );
                if (std::holds_alternative<tn::ftree_leaf>(_node)) {
                    tn::ftree_leaf _leaf = std::get<tn::ftree_leaf>(_node);
                    if (_leaf.tp == tn::leaf_type::var || _leaf.tp == tn::leaf_type::ctor_call) {
                        opds.push(tn::binary_node(opr::binary_opr::func_call, _leaf,
                            tn::ftree_leaf(tn::leaf_type::func_arg, expr.pop_extracted())));
                    } else {
                        return std::unexpected(error::semantics_inconsistency);
                    }
                } else {
                    opds.push(tn::binary_node(opr::binary_opr::func_call, std::move(_node),
                        tn::ftree_leaf(tn::leaf_type::func_arg, expr.pop_extracted())));
                }
            } else {
                if (expr[i - 1] == '(')
                    oprs.push('(');
                else
                    return std::unexpected(error::couldnt_find_func_ptr);
            }
            is_node = false;
        } else if (tf::ptf::is_special_open_brace(expr[i])) {
            if (is_node) {
                ftree::tree_node _node = std::move(opds.top());
                opds.pop();
                expr.extract_next();
                if (expr.apply(trim_while_false_t([](int c) -> int { return c == '}'; })).pos() == expr.size())
                    return std::unexpected(error::couldnt_find_close_brace);
                i = expr.apply(trim_char_t('}')).pos();
                if (std::holds_alternative<tn::ftree_leaf>(_node)) {
                    tn::ftree_leaf _leaf = std::get<tn::ftree_leaf>(_node);
                    if (_leaf.tp == tn::leaf_type::var) {
                        opds.push(tn::ftree_leaf(tn::leaf_type::ctor_call, 
                            static_cast<std::string>(_leaf.expr) + expr.pop_extracted() + '}'));
                    } else {
                        return std::unexpected(error::couldnt_find_token);
                    }
                } else {
                    return std::unexpected(error::couldnt_find_token);
                }
            } else {
                return std::unexpected(error::couldnt_find_token);
            }
            is_node = true;
        } else if (expr.invoke(tf::ptf::trim_operator)) {
            expr.extract_next();
            i = expr.apply(tf::ptf::trim_operator).pos();
            opr::opr _opr = *opr::opr_char(expr.pop_extracted());
            if (std::holds_alternative<opr::unary_opr>(_opr)) {
                opr::unary_opr u_opr = std::get<opr::unary_opr>(_opr);
                if (is_node) {
                    if (u_opr == opr::unary_opr::postf_inc) {
                        _opr = opr::unary_opr::pref_inc;
                    } else if (u_opr == opr::unary_opr::postf_dec) {
                        _opr = opr::unary_opr::pref_dec;
                    }
                }
            }
            while (oprs.size()) {
                if (std::holds_alternative<char>(oprs.top()) || !opr::__cmp(oprs.top(), _opr))
                    break;
                opr::opr pushed = std::get<opr::opr>(oprs.top());
                oprs.pop();
                auto rslt = push_opr(pushed, opds);
                if (!rslt)
                    return std::unexpected(rslt.error());
            }
            oprs.push(_opr);
            is_node = false;
        } else if (tf::ptf::is_close_brace(expr[i])) {
            i = expr.apply(trim_char_t(')')).pos();
            while (oprs.size()) {
                if (std::holds_alternative<char>(oprs.top()))
                    break;
                opr::opr pushed = std::get<opr::opr>(oprs.top());
                oprs.pop();
                auto rslt = push_opr(pushed, opds);
                if (!rslt)
                    return std::unexpected(rslt.error());
            }
            if (!oprs.size())
                return std::unexpected(error::couldnt_find_open_brace);
            oprs.pop();
            is_node = false;
        } else if (tf::ptf::is_special_close_brace(expr[i])) {
            return std::unexpected(error::couldnt_find_open_brace);
        } else {
            return std::unexpected(error::incorrect_char);
        }
    }
    while (oprs.size()) {
        if (std::holds_alternative<char>(oprs.top()))
            return std::unexpected(error::couldnt_find_close_brace);
        opr::opr pushed = std::get<opr::opr>(oprs.top());
        oprs.pop();
        auto rslt = push_opr(pushed, opds);
        if (!rslt)
            return std::unexpected(rslt.error());
    }
    if (opds.size() > 1)
        return std::unexpected(error::couldnt_find_operator);
    return ftree::ftree(std::move(opds.top()));
}

}
