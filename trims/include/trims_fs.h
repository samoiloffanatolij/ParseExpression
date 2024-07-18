#pragma once

#include "trims.h"
#include "operators.h"

namespace tf {

inline trim_fn_rslt trim_char(const trim_str_like& s, index_t pos, char c) {
    if (pos < s.size() && s[pos] == c)
        return pos + 1;
    return std::unexpected(pos);
}

#define trim_char_t(c) \
    [](const tf::trim_str_like& str, tf::index_t pos) { return tf::trim_char(str, pos, c); }

inline trim_fn_rslt trim_char_if(const trim_str_like& s, index_t pos, int (*f)(int)) {
    if (pos < s.size() && f(s[pos]))
        return pos + 1;
    return std::unexpected(pos);
}

#define trim_char_if_t(f) \
    [](const tf::trim_str_like& str, tf::index_t pos) { return tf::trim_char_if(str, pos, f); }

inline trim_fn_rslt trim_chars(const trim_str_like& s, index_t pos, std::string_view chars) {
    index_t i = 0;
    for (; i < chars.size() && pos < s.size(); ++pos, ++i) {
        if (chars[i] != s[pos])
            return std::unexpected(pos);
    }
    if (i != chars.size())
        return std::unexpected(pos);
    return pos;
}

#define trim_chars_t(chars) \
    [](const tf::trim_str_like& str, tf::index_t pos) { return tf::trim_chars(str, pos, chars); }

inline trim_fn_rslt trim_word(const trim_str_like& s, index_t pos, std::string_view word) {
    auto rslt = trim_chars(s, pos, word);
    if (!rslt || (*rslt == s.size()) || (*rslt < s.size() && std::isspace(s[*rslt])))
        return rslt;
    return std::unexpected(pos);
}

#define trim_word_t(word) \
    [](const tf::trim_str_like& str, tf::index_t pos) { return tf::trim_word(str, pos, word); }

inline trim_fn_rslt trim_while_true(const trim_str_like& s, index_t pos, int (*f)(int)) {
    while (pos < s.size() && f(s[pos])) ++pos;
    return pos;
}

#define trim_while_true_t(f) \
    [](const tf::trim_str_like& str, tf::index_t pos) { return tf::trim_while_true(str, pos, f); }

inline trim_fn_rslt trim_while_false(const trim_str_like& s, index_t pos, int (*f)(int)) {
    while (pos < s.size() && !f(s[pos])) ++pos;
    return pos;
}

#define trim_while_false_t(f) \
    [](const tf::trim_str_like& str, tf::index_t pos) { return tf::trim_while_false(str, pos, f); }

inline trim_fn_rslt trim_while_start(const trim_str_like& s, index_t pos) {
    for (index_t i = pos + 1; i < s.size(); ++i) {
        if (s[pos] != s[i])
            return i;
    }
    return s.size();
}

inline trim_fn_rslt trim_spaces(const trim_str_like& s, index_t pos) {
    return trim_while_true(s, pos, std::isspace);
}

inline trim_fn_rslt trim_spaces_require(const trim_str_like& s, index_t pos) {
    index_t _pos = *trim_spaces(s, pos);
    if (pos == _pos)
        return std::unexpected(pos);
    return _pos;
}

inline trim_fn_rslt trim_until_spacing(const trim_str_like& s, index_t pos) {
    return trim_while_false(s, pos, std::isspace);
}

inline trim_fn_rslt trim_line(const trim_str_like& s, index_t pos) {
    return *trim_while_false(s, pos, [](int c) -> int { return c == '\n'; }) + 1;
}

inline trim_fn_rslt trim_any_word(const trim_str_like& s, index_t pos) {
    index_t _pos = *trim_while_false(s, pos, [](int c) -> int { return !std::isalpha(c); });
    if (pos == _pos)
        return std::unexpected(pos);
    return _pos;
}

inline trim_fn_rslt trim_until_balance(const trim_str_like& s, index_t pos, char inc, char dec, int cnt = 0) {
    bool flag = false;
    while (pos < s.size()) {
        if (!flag && cnt)
            flag = true;
        if (s[pos] == inc)
            ++cnt;
        else if (s[pos] == dec)
            --cnt;
        if (flag && !cnt)
            return pos + 1;
        ++pos;
    }
    return std::unexpected(pos);
}

#define trim_until_balance_t(inc, dec, cnt) \
    [](const tf::trim_str_like& str, tf::index_t pos) { return tf::trim_until_balance(str, pos, inc, dec, cnt); }

namespace program_trims {

inline int is_alph_num(int c) {
    return std::isalnum(c);
}
inline int is_alpha(int c) {
    return std::isalpha(c);
}
inline int is_semicolon(int c) {
    return c == ';';
}
inline int is_open_brace(int c) {
    return c == '(' || c == '[';
}
inline int is_close_brace(int c) {
    return c == ')';
}
inline int is_special_open_brace(int c) {
    return c == '{';
}
inline int is_special_close_brace(int c) {
    return c == ']' || c == '}';
}
inline int is_linebreak(int c) {
    return c == '\n';
}
inline int is_space(int c) {
    return std::isspace(c);
}
inline int is_quote(int c) {
    return c == '\"' || c == '\'';
}
inline int is_colon(int c) {
    return c == ':';
}
inline int is_quest_mark(int c) {
    return c == '?';
}

inline trim_fn_rslt trim_num_literal(const trim_str_like& s, index_t pos) {
    if (pos + 2 < s.size() && s.substr(pos, 2) == "0x") {
        index_t _pos = *trim_while_false(s, pos + 2, [](int c) -> int {
            return !std::isalnum(c) && (!std::isalpha(c) || c > 'f');
        });
        if (_pos == pos + 2)
            return std::unexpected(pos);
        return _pos;
    } else {
        index_t _pos = *trim_while_false(s, pos, [](int c) -> int { return !std::isalnum(c); });
        if (_pos == pos)
            return std::unexpected(pos);
        return _pos;
    }
}

inline trim_fn_rslt trim_string_literal(const trim_str_like& s, index_t pos) {
    index_t _pos = pos;
    if (s[pos] == '\"' || s[pos] == '\'') {
        for (++pos; pos < s.size(); ++pos) {
            if (s[pos] == '\n')
                return std::unexpected(pos);
            if ((s[pos] == '\"' || s[pos] == '\'') && s[pos - 1] != '\\') {
                if (s[pos] == s[_pos])
                    return pos + 1;
                return std::unexpected(_pos);
            }
        }
    }
    return std::unexpected(_pos);
}

inline trim_fn_rslt trim_token(const trim_str_like& s, index_t pos) {
    if (pos >= s.size() || std::isalnum(s[pos]))
        return std::unexpected(pos);
    index_t _pos = *trim_while_false(s, pos, [](int c) -> int {
        return !std::isalpha(c) && !std::isalnum(c) && c != '_';
    });
    if (pos == _pos)
        return std::unexpected(pos);
    return _pos;
}

inline trim_fn_rslt trim_operator(const trim_str_like& s, index_t pos) {
    for (size_t _size = 3; _size; --_size) {
        if (opr::opr_char(s.substr(pos, _size)))
            return pos + _size;
    }
    return std::unexpected(pos);
}

}

namespace ptf = program_trims;

}
