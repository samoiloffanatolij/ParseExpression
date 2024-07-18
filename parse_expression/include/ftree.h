#pragma once

#include "trims_fs.h"

namespace ftree {

namespace _tree_node {

enum class leaf_type {
    num_literal, str_literal,
    ctor_call, func_arg,
    var
};
struct ftree_leaf {
    leaf_type tp;
    std::string_view expr;
    ftree_leaf(leaf_type tp, std::string_view expr) : tp(tp), expr(expr) {}
};

#define node_ptr std::unique_ptr<ftree_node>

struct unary_node {
    opr::unary_opr tp;
    node_ptr opd;
    unary_node(opr::unary_opr tp, node_ptr opd) : tp(tp), opd(std::move(opd)) {}
    unary_node(opr::unary_opr tp, ftree_node opd) : tp(tp), opd(std::make_unique<ftree_node>(opd)) {}
};
struct binary_node {
    opr::binary_opr tp;
    node_ptr opd_1, opd_2;
    binary_node(opr::binary_opr tp, node_ptr opd_1, node_ptr opd_2)
        : tp(tp), opd_1(std::move(opd_1)), opd_2(std::move(opd_2)) {}
    binary_node(opr::binary_opr tp, ftree_node opd_1, ftree_node opd_2)
        : tp(tp), opd_1(std::make_unique<ftree_node>(opd_1)),
        opd_2(std::make_unique<ftree_node>(opd_2)) {}
};

template<opr::ternary_opr>
struct ternary_node {
};
template<>
struct ternary_node<opr::ternary_opr::ways> {
    node_ptr opd_1, opd_2;
    ternary_node(node_ptr opd_1, node_ptr opd_2) : opd_1(std::move(opd_1)), opd_2(std::move(opd_2)) {}
    ternary_node(ftree_node opd_1, ftree_node opd_2)
        : opd_1(std::make_unique<ftree_node>(opd_1)), opd_2(std::make_unique<ftree_node>(opd_2)) {}
};
template<>
struct ternary_node<opr::ternary_opr::condition> {
    std::unique_ptr<ftree_node> condition;
    std::unique_ptr<ternary_node<opr::ternary_opr::ways>> ways;
    ternary_node(std::unique_ptr<ftree_node> condition, std::unique_ptr<ternary_node<opr::ternary_opr::ways>> ways)
        : condition(std::move(condition)), ways(std::move(ways)) {}
    ternary_node(ftree_node condition, ternary_node<opr::ternary_opr::ways> ways)
        : condition(std::make_unique<ftree_node>(condition)),
        ways(std::make_unique<ternary_node<opr::ternary_opr::ways>>(ways)) {}
};

using ftree_node = std::variant<
                        ftree_leaf, unary_node, binary_node,
                        ternary_node<opr::ternary_opr::condition>,
                        ternary_node<opr::ternary_opr::ways>
                        >;

}

using tree_node = _tree_node::ftree_node;

class ftree {
private:
    std::unique_ptr<tree_node> _root;
public:
    ftree(std::unique_ptr<tree_node> root) : _root(std::move(root)) {}
    ftree(tree_node root) : _root(std::make_unique<tree_node>(root)) {}
    ftree(_tree_node::leaf_type tp, std::string_view expr)
        : _root(std::make_unique<tree_node>(tp, expr)) {}
};

}
