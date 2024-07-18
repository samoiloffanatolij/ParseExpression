#pragma once

#include <iostream>

#include <istream>
#include <sstream>
#include <string>
#include <string_view>
#include <functional>
#include <memory>
#include <stdexcept>
#include <deque>
#include <expected>
#include <variant>
#include <list>
#include <type_traits>

#include "io.h"

// defines :(
#define trim_char_t(c) [](const trims::trim_str_like& str, size_t pos) noexcept { return tf::trim_char(str, pos, c); }
#define skip_chars_t(n) [](const trims::trim_str_like& str, size_t pos) noexcept { return tf::skip_chars(str, pos, n); }
#define trim_word_t(word) [](const trims::trim_str_like& str, size_t pos) noexcept { return tf::trim_word(str, pos, word); }
#define trim_a_or_b_t(a, b) [](const trims::trim_str_like& str, size_t pos) noexcept { return tf::trim_a_or_b(str, pos, a, b); }
#define trim_while_true_t(f) [](const trims::trim_str_like& str, size_t pos) noexcept { return tf::trim_while_true(str, pos, f); }
#define trim_while_false_t(f) [](const trims::trim_str_like& str, size_t pos) noexcept { return tf::trim_while_false(str, pos, f); }
#define trim_until_balance_t(inc, dec) [](const trims::trim_str_like& str, size_t pos) noexcept { return tf::trim_until_balance(str, pos, inc, dec); }


namespace tf {
    
using trim_fn_rslt = std::expected<uint64_t, uint64_t>;

}

namespace trims {

enum class error_policy {
    immediate_throw, retry_throw,
};

template<error_policy err_policy>
class _trim_str_buf {
public:
    using size_t = uint64_t;
protected:
    std::unique_ptr<std::istream> __src;
    size_t __streamsize;
    std::string __data;
    size_t __start, __end;
    std::vector<size_t> __newlines;
    bool __eos;
public:
    _trim_str_buf(std::istream* src) : __src(src), __start(0), __end(0), __eos(false), __newlines{ 0 } {
        auto filesize = io::get_file_size(*__src);
        if (!filesize) {
            if constexpr (err_policy == error_policy::retry_throw) {
                filesize = io::get_file_size(*__src);
                if (filesize) goto end;
            }
            throw std::runtime_error(io::get_error_message(filesize.error()));
        }
        end: 
        __streamsize = *filesize;
    }

    std::istream* src() && noexcept { return __src.release(); }
    size_t size() const noexcept { return __streamsize; } 
    size_t line_pos(size_t line) const noexcept { return __newlines[line]; } 
    std::pair<size_t, size_t> linecol(size_t pos) const noexcept {
        auto it = std::next(std::upper_bound(__newlines.begin(), __newlines.end(), pos), -1);
        return { std::distance(__newlines.begin(), it) + 1, pos - *it + 1 };
    }
    void set_start(size_t pos) noexcept {
        constexpr size_t min_distance = 1024;

        if (pos - __start < min_distance)
            return;
        auto n_data = std::string(__data.begin() + (pos - __start), __data.end());
        // std::cout << "Removed " << (pos - __start) << " bytes from buffer\n";
        __start = pos;
        __data = std::move(n_data);
    }
    void underflow() {
        constexpr size_t read_chunk_size = 1024;

        if (__eos) return;
        __data.reserve(__data.size() + read_chunk_size);
        auto read = io::read_bytes(*__src, __streamsize, read_chunk_size);
        if (!read) {
            if constexpr (err_policy == error_policy::retry_throw) {
                read = io::read_bytes(*__src, __streamsize, read_chunk_size);
                if (read) goto success;
            }
            throw std::runtime_error(io::get_error_message(read.error()));
        }
        success:
        std::copy(read->begin(), read->end(), std::back_inserter(__data));
        // std::cout << "Read " << read->size() << " bytes to buffer\n";
        __end += read->size();
        __eos = (read->size() != read_chunk_size);
    }
    char at(size_t pos) {
        while (pos >= __end && !__eos)
            underflow();
        if (pos >= __end)
            throw std::runtime_error("bad string access");
        if (__data[pos - __start] == '\n') {
            auto it = std::lower_bound(__newlines.begin(), __newlines.end(), pos + 1);
            if (it == __newlines.end() || *it != pos + 1)
                __newlines.insert(it, pos + 1);
        }
        return __data[pos - __start];
    }
    std::string_view substr(size_t off, size_t n) {
        if (n != -1)
            this->at(__start + off + n - 1);
        return std::string_view(__data).substr(off, n);
    }
};

template<error_policy err_policy>
class _trim_str_base {
public:
    using size_t = uint64_t;
protected:
    mutable _trim_str_buf<err_policy> _buf;
    std::deque<size_t> _saved;
    size_t _it;

    void _update_buf_start() noexcept {
        _buf.set_start(_saved.size() ? std::min(_it, _saved.front()) : _it);
    }
public:
    constexpr _trim_str_base(std::istream* src, size_t pos = 0) : _buf(src), _it(pos) {}
    constexpr _trim_str_base(std::string s, size_t pos = 0) : _it(pos),
        _buf(new std::istringstream(std::move(s), std::ios_base::binary)) {}

    constexpr size_t pos() const noexcept { return _it; }
    constexpr size_t size() const noexcept { return _buf.size(); }
    constexpr auto linecol(size_t pos) const noexcept { return _buf.linecol(pos); }
    constexpr auto saved_linecol() const noexcept { return _buf.linecol(_saved.back()); }
    constexpr auto line_pos(size_t line) const noexcept { return _buf.line_pos(line); } 
    constexpr char at(size_t pos) const { return _buf.at(pos); }
    constexpr std::string_view substr(size_t off, size_t n) const {
        return _buf.substr(off, n);
    }

    constexpr bool exhausted() const noexcept { return _it == _buf.size(); }
    constexpr bool ok() const noexcept { return _it != -1u; }
    constexpr bool err() const noexcept { return _it == -1u; }
    constexpr void force_err() noexcept { _it = -1u; }

    void save() {
        _saved.push_back(_it);
    }
    void load() {
        _it = _saved.back();
        _saved.pop_back();
    }
    void del() {
        _saved.pop_back();
    }
};

using trim_str_like = _trim_str_base<error_policy::retry_throw>;

}

namespace tf {

template<class... Args>
using trim_fn_t = trim_fn_rslt(*)(const trims::trim_str_like&, size_t, Args...) noexcept;
using trim_fn = trim_fn_t<>;

enum class special {
    extract_next,
};

constexpr trim_fn_rslt nop(const trims::trim_str_like& str, size_t pos) noexcept { return pos; }

template<class F, class... Args>
concept trim_fn_c = requires {
    std::is_nothrow_invocable_v<F, const trims::trim_str_like&, size_t, Args...>;
    std::is_same_v<trim_fn_rslt, std::invoke_result_t<F, const trims::trim_str_like&, size_t, Args...>>;
};

template<auto... Fs>
    requires (sizeof...(Fs) > 1) && (trim_fn_c<decltype(Fs)> && ...)
struct trim_seq {
    static constexpr size_t size = sizeof...(Fs);
    static constexpr trim_fn funcs[size] = { Fs... };
};

template<auto F>
concept ex_trim_fn_c = trim_fn_c<decltype(F)> || requires {
    std::is_same_v<decltype(F), special>;
    F == special::extract_next;
};

template<auto... Fs>
    requires (sizeof...(Fs) > 1) && (ex_trim_fn_c<Fs> && ...)
struct ex_trim_seq {
    static constexpr size_t size = sizeof...(Fs);
    static constexpr std::variant<trim_fn, special> funcs[size] = { Fs... };
};

}

namespace trims {

class trim_str : public trim_str_like {
private:
    using base = trim_str_like;
public:
    using _trim_str_base::_trim_str_base;

    auto& save() {
        base::save();
        return *this;
    }
    auto& load() {
        base::load();
        return *this;
    }
    auto& del() {
        base::del();
        return *this;
    }

    auto& linecol(size_t& line, size_t& col) const noexcept {
        std::tie(line, col) = base::linecol(_it);
        return *this;
    }
    auto& linecol(size_t& line, size_t& col) noexcept {
        std::tie(line, col) = base::linecol(_it);
        return *this;
    }
    auto& saved_linecol(size_t& line, size_t& col) const noexcept {
        std::tie(line, col) = base::saved_linecol();
        return *this;
    }
    auto& saved_linecol(size_t& line, size_t& col) noexcept {
        std::tie(line, col) = base::saved_linecol();
        return *this;
    }

    template<class F, class... Args>
        requires (tf::trim_fn_c<F, Args...>)
    constexpr tf::trim_fn_rslt invoke(F&& f, Args&&... args) const {
        return f(*this, _it, std::forward<Args>(args)...);
    }

    template<auto... Fs>
    constexpr tf::trim_fn_rslt invoke(tf::trim_seq<Fs...> seq) const {
        size_t it = _it;
        tf::trim_fn_rslt rslt;
        for (size_t i = 0; i < seq.size && it != -1u; ++i) {
            rslt = seq.funcs[i](*this, it);
            if (!rslt)
                break;
            it = *rslt;
        }
        return rslt;
    }

    template<class F, class... Args>
        requires (tf::trim_fn_c<F, Args...>)
    constexpr tf::trim_fn_rslt apply_if_ok(F&& f, Args&&... args) {
        tf::trim_fn_rslt rslt = invoke(std::forward<F>(f), std::forward<Args>(args)...);
        if (!rslt)
            return rslt;
        _it = *rslt;
        _update_buf_start();
        return rslt;
    }

    template<auto... Fs>
    constexpr tf::trim_fn_rslt apply_if_ok(tf::trim_seq<Fs...> seq) {
        tf::trim_fn_rslt rslt = invoke(seq);
        if (!rslt)
            return rslt;
        _it = *rslt;
        _update_buf_start();
        return rslt;
    }

    template<class F, class... Args>
        requires (tf::trim_fn_c<F, Args...>)
    constexpr trim_str& apply(F&& f, Args&&... args) {
        tf::trim_fn_rslt rslt = invoke(std::forward<F>(f), std::forward<Args>(args)...);
        if (!rslt) {
            _it = -1;
            return *this;
        }
        _it = *rslt;
        _update_buf_start();
        return *this;
    }

    template<auto... Fs>
    constexpr trim_str& apply(tf::trim_seq<Fs...> seq) {
        tf::trim_fn_rslt rslt = invoke(seq);
        if (!rslt) {
            _it = -1;
            return *this;
        }
        _it = *rslt;
        _update_buf_start();
        return *this;
    }
};

class _ex_trim_str_base : public trim_str_like {
protected:
    std::deque<std::string> _extracted;
    bool _extract_next;
public:
    _ex_trim_str_base(std::istream* src, size_t pos = 0) 
        : trim_str_like(src, pos), _extract_next(false) {}
    _ex_trim_str_base(std::string s, size_t pos = 0) 
        : trim_str_like(std::move(s), pos), _extract_next(false) {}

    void extract_next() noexcept { _extract_next = true; }
    std::string pop_extracted() noexcept {
        std::string ret = std::move(_extracted.back());
        _extracted.pop_back();
        return ret;
    }
};

class ex_trim_str : public _ex_trim_str_base {
private:
    using base = _ex_trim_str_base;

    template<auto... Fs>
    constexpr tf::trim_fn_rslt __apply_seq_base(tf::ex_trim_seq<Fs...> seq) {
        std::deque<std::string> extracted;
        bool extract_next = false;
        size_t it = _it;
        tf::trim_fn_rslt rslt;
        for (size_t i = 0; i < seq.size && it != -1; ++i) {
            if (seq.funcs[i].index() == 1) {
                extract_next = true;
            } else {
                rslt = std::get<0>(seq.funcs[i])(*this, it);
                if (extract_next && rslt) {
                    extracted.emplace_back(substr(it, *rslt - it));
                    extract_next = false;
                }
                it = rslt.value_or(-1);
            }
        }
        if (it != -1)
            std::move(extracted.begin(), extracted.end(), std::back_inserter(_extracted));
        return rslt;
    }
public:
    using _ex_trim_str_base::_ex_trim_str_base;

    auto& save() {
        base::save();
        return *this;
    }
    auto& load() {
        base::load();
        return *this;
    }
    auto& del() {
        base::del();
        return *this;
    }

    auto& linecol(size_t& line, size_t& col) const noexcept {
        std::tie(line, col) = base::linecol(_it);
        return *this;
    }
    auto& linecol(size_t& line, size_t& col) noexcept {
        std::tie(line, col) = base::linecol(_it);
        return *this;
    }
    auto& saved_linecol(size_t& line, size_t& col) const noexcept {
        std::tie(line, col) = base::saved_linecol();
        return *this;
    }
    auto& saved_linecol(size_t& line, size_t& col) noexcept {
        std::tie(line, col) = base::saved_linecol();
        return *this;
    }

    template<class F, class... Args>
        requires (tf::trim_fn_c<F, Args...>)
    constexpr tf::trim_fn_rslt invoke(F&& f, Args&&... args) const {
        return f(*this, _it, std::forward<Args>(args)...);
    }

    template<auto... Fs>
    constexpr tf::trim_fn_rslt invoke(tf::trim_seq<Fs...> seq) const {
        size_t it = _it;
        tf::trim_fn_rslt rslt;
        for (size_t i = 0; i < seq.size && it != -1u; ++i) {
            rslt = seq.funcs[i](*this, it);
            if (!rslt)
                break;
            it = *rslt;
        }
        return rslt;
    }

    template<auto... Fs>
    constexpr tf::trim_fn_rslt invoke(tf::ex_trim_seq<Fs...> seq) const {
        size_t it = _it;
        tf::trim_fn_rslt rslt;
        for (size_t i = 0; i < seq.size && it != -1u; ++i) {
            if (seq.funcs[i].index() == 0u) {
                rslt = seq.funcs[i](*this, it);
                it = rslt.value_or(-1);
            }
        }
        return rslt;
    }

    template<class F, class... Args>
        requires (tf::trim_fn_c<F, Args...>)
    constexpr tf::trim_fn_rslt apply_if_ok(F&& f, Args&&... args) {
        tf::trim_fn_rslt rslt = invoke(std::forward<F>(f), std::forward<Args>(args)...);
        if (!rslt)
            return _extract_next = false, rslt;
        if (_extract_next) {
            _extracted.emplace_back(substr(_it, *rslt - _it));
            _extract_next = false;
        }
        _it = *rslt;
        _update_buf_start();
        return rslt;
    }

    template<auto... Fs>
    constexpr tf::trim_fn_rslt apply_if_ok(tf::trim_seq<Fs...> seq) {
        tf::trim_fn_rslt rslt = invoke(seq);
        if (!rslt)
            return _extract_next = false, rslt;
        if (_extract_next) {
            _extracted.emplace_back(substr(_it, *rslt - _it));
            _extract_next = false;
        }
        _it = *rslt;
        _update_buf_start();
        return rslt;
    }

    template<auto... Fs>
    constexpr tf::trim_fn_rslt apply_if_ok(tf::ex_trim_seq<Fs...> seq) {
        tf::trim_fn_rslt rslt = __apply_seq_base(seq);
        if (!rslt)
            return _extract_next = false, rslt;
        if (_extract_next) {
            _extracted.emplace_back(substr(_it, *rslt - _it));
            _extract_next = false;
        }
        _it = *rslt;
        _update_buf_start();
        return rslt;
    }

    constexpr ex_trim_str& apply(tf::special f) {
        if (f == tf::special::extract_next)
            _extract_next = true;
        return *this;
    }

    template<class F, class... Args>
        requires (tf::trim_fn_c<F, Args...>)
    constexpr ex_trim_str& apply(F&& f, Args&&... args) {
        tf::trim_fn_rslt rslt = invoke(std::forward<F>(f), std::forward<Args>(args)...);
        if (!rslt) {
            _it = -1;
            return *this;
        }
        if (_extract_next) {
            _extracted.emplace_back(substr(_it, *rslt - _it));
            _extract_next = false;
        }
        _it = *rslt;
        _update_buf_start();
        return *this;
    }

    template<auto... Fs>
    constexpr ex_trim_str& apply(tf::trim_seq<Fs...> seq) {
        tf::trim_fn_rslt rslt = invoke(seq);
        if (!rslt) {
            _it = -1;
            return *this;
        }
        if (_extract_next) {
            _extracted.emplace_back(substr(_it, *rslt - _it));
            _extract_next = false;
        }
        _it = *rslt;
        _update_buf_start();
        return *this;
    }

    template<auto... Fs>
    constexpr ex_trim_str& apply(tf::ex_trim_seq<Fs...> seq) {
        tf::trim_fn_rslt rslt = __apply_seq_base(seq);
        if (!rslt) {
            _it = -1;
            return *this;
        }
        if (_extract_next) {
            _extracted.emplace_back(substr(_it, *rslt - _it));
            _extract_next = false;
        }
        _it = *rslt;
        _update_buf_start();
        return *this;
    }
};

}

namespace tf {

constexpr int is_linebreak(int c) {
    return c == '\n';
}
constexpr int is_separator(int c) {
    return !std::isalnum(c);
}

constexpr trim_fn_rslt trim_spaces(const trims::trim_str_like& str, size_t pos) noexcept {
    for (; pos < str.size(); ++pos) {
        if (!std::isspace(str.at(pos)))
            break;
    }
    return pos;
}

constexpr trim_fn_rslt trim_spaces_require(const trims::trim_str_like& str, size_t pos) noexcept {
    size_t _pos = trim_spaces(str, pos).value();
    if (_pos == pos)
        return std::unexpected(pos);
    return _pos;
}

constexpr trim_fn_rslt trim_char(const trims::trim_str_like& str, size_t pos, char c) noexcept {
    if (pos < str.size() && str.at(pos) == c)
        return pos + 1;
    return std::unexpected(pos);
}

constexpr trim_fn_rslt trim_word(const trims::trim_str_like& str, size_t pos, std::string_view word) noexcept {
    size_t i = 0;
    for (; i < word.size() && pos < str.size(); ++pos, ++i) {
        if (word[i] != str.at(pos))
            return std::unexpected(pos);
    }
    if (i == word.size())
        return pos;
    return std::unexpected(pos);
}

constexpr trim_fn_rslt trim_a_or_b(const trims::trim_str_like& str, size_t pos, std::string_view a, std::string_view b) noexcept {
    if (pos >= str.size())
        return std::unexpected(pos);
    std::string_view sub = str.substr(pos, std::max(a.size(), b.size()));
    bool A = sub.starts_with(a), B = sub.starts_with(b);
    if (!(A ^ B))
        return std::unexpected(pos);
    if (A)
        return pos + a.size();
    return pos + b.size();
}

constexpr trim_fn_rslt trim_while_true(const trims::trim_str_like& str, size_t pos, int (*f)(int c)) noexcept {
    for (; pos < str.size(); ++pos) {
        if (!f(str.at(pos)))
            break;
    }
    return pos;
}

constexpr trim_fn_rslt trim_while_false(const trims::trim_str_like& str, size_t pos, int (*f)(int c)) noexcept {
    for (; pos < str.size(); ++pos) {
        if (f(str.at(pos)))
            break;
    }
    return pos;
}

constexpr trim_fn_rslt trim_until_spacing(const trims::trim_str_like& str, size_t pos) noexcept {
    return trim_while_false(str, pos, std::isspace);
}

constexpr trim_fn_rslt trim_line(const trims::trim_str_like& str, size_t pos) noexcept {
    return trim_while_false(str, pos, is_linebreak).value() + 1;
}

constexpr trim_fn_rslt trim_any_word(const trims::trim_str_like& str, size_t pos) noexcept {
    size_t _pos = trim_while_false(str, pos, is_separator).value();
    if (_pos == pos)
        return std::unexpected(pos);
    return _pos;
}

constexpr trim_fn_rslt trim_until_balance(const trims::trim_str_like& str, size_t pos, char inc, char dec, int cnt) noexcept {
    bool flag = false;
    for (; pos < str.size(); ++pos) {
        if (!flag && cnt) 
            flag = true;
        if (str.at(pos) == inc)
            ++cnt;
        else if (str.at(pos) == dec)
            --cnt;
        else if (flag && !cnt)
            return pos + 1;
    }
    return std::unexpected(pos);
}

namespace prog {
    constexpr int is_semicolon(int c) {
        return c == ';';
    }
    constexpr int is_open_brace(int c) {
        return c == '(' || c == '{' || c == '[';
    }
    constexpr int is_close_brace(int c) {
        return c == ')' || c == '}' || c == ']';
    }
    constexpr int is_semicolon_or_spacing(int c) {
        return is_semicolon(c) || std::isspace(c);
    }
    constexpr int is_semicolon_or_linebreak(int c) {
        return c == ';' || c == '\n';
    }
    constexpr int is_spacing_or_brace(int c) {
        return std::isspace(c) || is_open_brace(c) || is_close_brace(c);
    }

    constexpr trim_fn_rslt trim_token(const trims::trim_str_like& str, size_t pos) noexcept {
        bool first = true;
        for (; pos < str.size(); ++pos) {
            if (first) {
                if (!(std::isalpha(str.at(pos)) || str.at(pos) == '_'))
                    break;
                first = false;
            } else {
                if (!(std::isalnum(str.at(pos)) || str.at(pos) == '_'))
                    break;
            }
        }
        if (first)
            return std::unexpected(pos);
        return pos;
    }

    constexpr trim_fn_rslt trim_until_semicolon(const trims::trim_str_like& str, size_t pos) noexcept {
        return trim_while_false(str, pos, is_semicolon);
    }

    constexpr trim_fn_rslt trim_until_spacing_or_semicolon(const trims::trim_str_like& str, size_t pos) noexcept {
        return trim_while_false(str, pos, is_semicolon_or_spacing);
    }

    constexpr trim_fn_rslt trim_until_linebreak_or_semicolon(const trims::trim_str_like& str, size_t pos) noexcept {
        return trim_while_false(str, pos, is_semicolon_or_linebreak);
    }

    constexpr trim_fn_rslt trim_until_spacing_or_brace(const trims::trim_str_like& str, size_t pos) noexcept {
        return trim_while_false(str, pos, is_spacing_or_brace);
    }

    constexpr trim_fn_rslt trim_string_literal(const trims::trim_str_like& str, size_t pos) noexcept {
        if (str.at(pos) == '\"') {
            bool backslash = false;
            for (size_t i = pos + 1; i < str.size(); ++i) {
                if (str.at(i) == '\\') {
                    backslash = true;
                    continue;
                }
                if (str.at(i) == '\n')
                    return std::unexpected(i);
                if (!backslash && str.at(i) == '\"')
                    return i + 1;
                backslash = false;
            }
        }
        return std::unexpected(pos);
    }

    constexpr trim_fn_rslt trim_decimal_literal(const trims::trim_str_like& str, size_t pos) noexcept {
        bool first = true;
        for (; pos < str.size(); ++pos) {
            if (!std::isdigit(str.at(pos)))
                break;
            first = false;
        }
        if (first)
            return std::unexpected(pos);
        return pos;
    }
}

}
