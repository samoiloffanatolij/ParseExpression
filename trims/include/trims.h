#pragma once

#include <expected>
#include <istream>
#include <memory>
#include <string>
#include <string_view>
#include <deque>
#include <stdexcept>
#include <concepts>
#include <type_traits>
#include <variant>
#include <array>
#include <utility>

#include "io.h"


namespace tf {

using index_t = uint64_t;    
using trim_fn_rslt = std::expected<index_t, index_t>;

}

namespace trims {

using tf::index_t;

enum class count_lines { no, yes };
template<count_lines> class _trim_str_buf {};

template<>
class _trim_str_buf<count_lines::no> {
protected:
    std::istream* _src;
    index_t _size;
    bool _eos;
    std::string _data;
    index_t _start, _end;
public:
    _trim_str_buf(std::istream* src) : _src(src), _eos(false), _start(0), _end(0) {
        auto size = io::get_file_size(*src);
        if (!size)
            throw std::runtime_error(io::get_error_message(size.error()));
        _size = *size;
    }

    index_t size() const noexcept { return _size; }
    void set_start(index_t pos) {
        constexpr index_t min_distance = 1024;

        if (pos - _start < min_distance)
            return;
        _start = pos, _data = std::string(_data.begin() + (pos - _start), _data.end());
    }
    void underflow() {
        constexpr index_t read_chunk_size = 1024;

        if (_eos) return;
        _data.reserve(_data.size() + read_chunk_size);
        auto read = io::read_bytes(*_src, _size, read_chunk_size);
        if (!read)
            throw std::runtime_error(io::get_error_message(read.error()));
        std::copy(read->begin(), read->end(), std::back_inserter(_data));
        _end += read->size(), _eos = (read->size() != read_chunk_size);
    }
    char at(index_t pos) {
        while (pos >= _end && !_eos)
            underflow();
        return _data.at(pos - _start);
    }
    char operator[](index_t pos) {
        while (pos >= _end && !_eos)
            underflow();
        return _data[pos - _start];
    }
    std::string_view substr(index_t pos, index_t n) {
        if (n != -1)
            this->at(pos + n - 1);
        return std::string_view(_data).substr(pos - _start, n);
    }
};

template<>
class _trim_str_buf<count_lines::yes> : public _trim_str_buf<count_lines::no> {
public:
    using base = _trim_str_buf<count_lines::no>;
protected:
    std::deque<index_t>* _newlines;
public:
    _trim_str_buf(std::istream* src, std::deque<index_t>* newlines) : base(src), _newlines(newlines) {
        this->_newlines->push_back(0);
    }
    
    std::deque<index_t>* const& newlines() const noexcept { return _newlines; }
    std::deque<index_t>* newlines(std::deque<index_t>& newlines) noexcept { 
        return std::exchange(_newlines, &newlines); 
    }

    char at(index_t pos) {
        while (pos >= _end && !_eos)
            underflow();
        if (_data.at(pos - _start) == '\n') {
            auto it = std::lower_bound(_newlines->begin(), _newlines->end(), pos + 1);
            if (it == _newlines->end() || *it != pos + 1)
                _newlines->insert(it, pos + 1);
        }
        return _data[pos - _start];
    }
    char operator[](index_t pos) {
        while (pos >= _end && !_eos)
            underflow();
        if (_data[pos - _start] == '\n') {
            auto it = std::lower_bound(_newlines->begin(), _newlines->end(), pos + 1);
            if (it == _newlines->end() || *it != pos + 1)
                _newlines->insert(it, pos + 1);
        }
        return _data[pos - _start];
    }
    index_t linepos(size_t line) const { return (*_newlines)[line - 1]; } 
    std::pair<size_t, size_t> linecol(index_t pos) const {
        auto it = std::next(std::upper_bound(_newlines->begin(), _newlines->end(), pos), -1);
        return { std::distance(_newlines->begin(), it) + 1, pos - *it + 1 };
    }
};

struct trim_str_like {
    virtual index_t pos() const noexcept = 0;
    virtual index_t size() const noexcept = 0;
    virtual char at(index_t pos) const = 0;
    virtual char operator[](index_t pos) const = 0;
    virtual std::string_view substr(index_t pos, index_t n) const = 0;
    virtual bool exhausted() const noexcept = 0;
    virtual bool ok() const noexcept = 0;
    virtual bool err() const noexcept { return !ok(); }
};

template<count_lines CountLines>
class _trim_str_base : public trim_str_like {
public:
    static constexpr bool counts_lines = (CountLines == count_lines::yes);
protected:
    mutable _trim_str_buf<CountLines> _buf;
    index_t _pos;
public:
    _trim_str_base(std::istream& src) requires(!counts_lines) : _buf(&src), _pos(0) {}
    _trim_str_base(std::istream& src, std::deque<index_t>& newlines) requires(counts_lines) 
        : _buf(&src, &newlines), _pos(0) {}

    auto& newlines() const noexcept requires(counts_lines) { return _buf.newlines(); }
    auto newlines(std::deque<index_t>& newlines) noexcept requires(counts_lines) { 
        return _buf.newlines(newlines); 
    }

    auto linepos(size_t line) const requires(counts_lines) { return _buf.linepos(line); }
    auto linecol(index_t pos) const requires(counts_lines) { return _buf.linecol(pos); }

    index_t pos() const noexcept override { return _pos; }
    index_t size() const noexcept override { return _buf.size(); }
    char at(index_t pos) const override { return _buf.at(pos); }
    char operator[](index_t pos) const override { return _buf[pos]; }
    std::string_view substr(index_t pos, index_t n) const override { return _buf.substr(pos, n); }

    bool exhausted() const noexcept override { return _pos >= _buf.size(); }
    bool ok() const noexcept override { return _pos != -1; }
};

enum class use_saves { no, yes };
template<count_lines, use_saves> class _saving_trim_str_base {};

template<count_lines CountLines>
class _saving_trim_str_base<CountLines, use_saves::no> : public _trim_str_base<CountLines> {
public:
    using base = _trim_str_base<CountLines>;
protected:
    void _upd_buf_start() { this->_buf.set_start(this->_pos); }
public:
    _saving_trim_str_base(std::istream& src) requires(CountLines == count_lines::no) : base(src) {}
    _saving_trim_str_base(std::istream& src, std::deque<index_t>& newlines) 
        requires(CountLines == count_lines::yes) : base(src, newlines) {}
};

template<count_lines CountLines>
class _saving_trim_str_base<CountLines, use_saves::yes> : public _trim_str_base<CountLines> {
public:
    using base = _trim_str_base<CountLines>;
protected:
    std::deque<index_t>* _saved;

    void _upd_buf_start() { 
        this->_buf.set_start(this->_saved->size() ? std::min(this->_saved->front(), this->_pos) : this->_pos); 
    }
public:
    _saving_trim_str_base(std::istream& src, std::deque<index_t>& saved) requires(CountLines == count_lines::no)
        : base(src), _saved(&saved) {}
    _saving_trim_str_base(std::istream& src, std::deque<index_t>& newlines, std::deque<index_t>& saved) 
        requires(CountLines == count_lines::yes) : base(src, newlines), _saved(&saved) {}
    
    std::deque<index_t>* const& saved() const noexcept { return _saved; }
    std::deque<index_t>* saved(std::deque<index_t>& saved) { return std::exchange(_saved, &saved); }
    
    void save_pos() { this->_saved->push_back(this->_pos); }
    index_t load_saved() { return this->_pos = this->_saved->back(), this->_saved->pop_back(), this->_pos; }
    index_t pop_saved() { auto _ = _saved->back(); return _saved->pop_back(), _; }
};

}

namespace tf {

namespace tags {
    // Use function chaining overload 
    struct chain_t {};
    constexpr chain_t chain {};

    // Use function chaining overload with out parameter
    struct out_t {};
    constexpr out_t out {};
}

using trims::trim_str_like;

template<class... Args>
using trim_fn_t = trim_fn_rslt(*)(const trim_str_like&, index_t, Args...);

using trim_fn = trim_fn_t<>;

struct save_pos_t {};
constexpr save_pos_t save_pos {};

struct extract_next_t {};
constexpr extract_next_t extract_next {};

inline trim_fn_rslt nop(const trim_str_like& s, index_t pos) { return pos; }

template<class F, class... Args>
concept trim_fn_c = requires {
    std::is_invocable_v<F, const trim_str_like&, index_t, Args...>;
    std::is_same_v<trim_fn_rslt, std::invoke_result_t<F, const trim_str_like&, index_t, Args...>>;
};

template<class F>
concept trim_seq_el = trim_fn_c<F>;

template<class... Fs>
    requires (sizeof...(Fs) > 1) && (trim_seq_el<Fs> && ...)
struct trim_seq {
    using element_t = trim_fn;

    static constexpr auto size = sizeof...(Fs);
    const std::array<element_t, size> funcs;

    constexpr trim_seq(Fs&&... fs) : funcs({ std::forward<Fs>(fs)... }) {}
};

template<class... Fs>
trim_seq(Fs&&... fs) -> trim_seq<Fs...>;

template<class F>
concept saving_trim_seq_el = trim_fn_c<F> || std::is_same_v<std::remove_cvref_t<F>, save_pos_t>;

template<class... Fs>
    requires (sizeof...(Fs) > 1) && (saving_trim_seq_el<Fs> && ...)
struct saving_trim_seq {
    using element_t = std::variant<trim_fn, save_pos_t>;

    static constexpr size_t size = sizeof...(Fs),
        saves = (std::is_same_v<std::remove_cvref_t<Fs>, save_pos_t> + ...);
    const std::array<element_t, size> funcs;

    constexpr saving_trim_seq(Fs&&... fs) : funcs({ std::forward<Fs>(fs)... }) {}
};

template<class... Fs>
saving_trim_seq(Fs&&... fs) -> saving_trim_seq<Fs...>;

template<class F>
concept ex_trim_seq_el = trim_seq_el<F> || std::is_same_v<std::remove_cvref_t<F>, extract_next_t>;

template<class... Fs>
    requires (sizeof...(Fs) > 1) && (ex_trim_seq_el<Fs> && ...)
struct ex_trim_seq {
    using element_t = std::variant<trim_fn, extract_next_t>;

    static constexpr size_t size = sizeof...(Fs),
        extracts = (std::is_same_v<std::remove_cvref_t<Fs>, extract_next_t> + ...);
    const std::array<element_t, size> funcs;

    constexpr ex_trim_seq(Fs&&... fs) : funcs({ std::forward<Fs>(fs)... }) {}
};

template<class... Fs>
ex_trim_seq(Fs&&... fs) -> ex_trim_seq<Fs...>;

}

namespace trims {

template<count_lines CountLines, use_saves Saves> 
class trim_str_base : public _saving_trim_str_base<CountLines, Saves> {
public:
    static constexpr auto saves = Saves;
    using base = _saving_trim_str_base<CountLines, Saves>;
private:
    template<class... Fs> requires(Saves == use_saves::yes)
    tf::trim_fn_rslt _apply_saving_seq_base(const tf::saving_trim_seq<Fs...>& seq) {
        tf::trim_fn_rslt rslt = this->_pos;
        std::vector<tf::index_t> saves;
        saves.reserve(seq.saves);
        for (auto it = seq.funcs.begin(); it != seq.funcs.end() && rslt.has_value(); ++it) {
            if (it->index() == 0) {
                rslt = std::get<0>(*it)(*this, *rslt);
            } else {
                saves.push_back(*rslt);
            }
        }
        if (rslt)
            std::copy(saves.begin(), saves.end(), std::back_inserter(*this->_saved));
        return rslt;
    }
public:
    using base::_saving_trim_str_base;

    using base::pos;
    auto& pos(tf::tags::out_t, tf::index_t& out) { return out = this->_pos, *this; }
    using base::linecol;
    auto& linecol(tf::tags::out_t, size_t& line, size_t& col) { 
        return std::tie(line, col) = linecol(this->_pos), *this; 
    }

    auto& save_pos(tf::tags::chain_t) requires(Saves == use_saves::yes) { return base::save_pos(), *this; }
    auto& save_pos(tf::tags::out_t, tf::index_t& out) requires(Saves == use_saves::yes) { 
        return out = this->_pos, base::save_pos(), *this; 
    }
    tf::index_t load_saved() requires(Saves == use_saves::yes) { return base::load_saved(); }
    auto& load_saved(tf::tags::chain_t) requires(Saves == use_saves::yes) { 
        return base::load_saved(), *this; 
    }
    auto& load_saved(tf::tags::out_t, tf::index_t& out) requires(Saves == use_saves::yes) { 
        return out = base::load_saved(), *this; 
    }
    tf::index_t pop_saved() requires(Saves == use_saves::yes) {
        return base::pop_saved();
    }
    auto& pop_saved(tf::tags::chain_t) requires(Saves == use_saves::yes) {
        return base::pop_saved(), *this;
    }
    auto& pop_saved(tf::tags::out_t, tf::index_t& out) requires(Saves == use_saves::yes) {
        return out = base::pop_saved(), *this;
    }

    template<class F, class... Args> requires(tf::trim_fn_c<F, Args...>)
    tf::trim_fn_rslt invoke(F&& f, Args&&... args) const {
        return f(*this, this->_pos, std::forward<Args>(args)...);
    }

    template<class... Fs>
    tf::trim_fn_rslt invoke(const tf::trim_seq<Fs...>& seq) const {
        tf::trim_fn_rslt rslt = this->_pos;
        for (auto it = seq.funcs.begin(); it != seq.funcs.end() && rslt.has_value(); ++it) {
            rslt = (*it)(*this, *rslt);
        }
        return rslt;
    }

    template<class... Fs>
    tf::trim_fn_rslt invoke(const tf::saving_trim_seq<Fs...>& seq) const {
        tf::trim_fn_rslt rslt = this->_pos;
        for (auto it = seq.funcs.begin(); it != seq.funcs.end() && rslt.has_value(); ++it) {
            if (it->index() == 0)
                rslt = std::get<0>(*it)(*this, *rslt);
        }
        return rslt;
    }

    template<class F, class... Args> requires(tf::trim_fn_c<F, Args...>)
    tf::trim_fn_rslt apply_if_ok(F&& f, Args&&... args) {
        auto rslt = invoke(std::forward<F>(f), std::forward<Args>(args)...);
        if (rslt)
            this->_pos = *rslt, this->_upd_buf_start();
        return rslt;
    }

    template<class... Fs>
    tf::trim_fn_rslt apply_if_ok(const tf::trim_seq<Fs...>& seq) {
        auto rslt = invoke(seq);
        if (rslt)
            this->_pos = *rslt, this->_upd_buf_start();
        return rslt;
    }

    template<class... Fs> requires(Saves == use_saves::yes)
    tf::trim_fn_rslt apply_if_ok(const tf::saving_trim_seq<Fs...>& seq) {
        auto rslt = _apply_saving_seq_base(seq);
        if (rslt)
            this->_pos = *rslt, this->_upd_buf_start();
        return rslt;
    }

    template<class F, class... Args> requires(tf::trim_fn_c<F, Args...>)
    auto& apply(F&& f, Args&&... args) {
        auto rslt = invoke(std::forward<F>(f), std::forward<Args>(args)...);
        this->_pos = rslt.value_or(-1);
        if (rslt)
            this->_upd_buf_start();
        return *this;
    }

    template<class... Fs>
    auto& apply(const tf::trim_seq<Fs...>& seq) {
        auto rslt = invoke(seq);
        this->_pos = rslt.value_or(-1);
        if (rslt)
            this->_upd_buf_start();
        return *this;
    }

    template<class... Fs> requires(Saves == use_saves::yes)
    auto& apply(const tf::saving_trim_seq<Fs...>& seq) {
        auto rslt = _apply_saving_seq_base(seq);
        this->_pos = rslt.value_or(-1);
        if (rslt)
            this->_upd_buf_start();
        return *this;
    }
};

using trim_str = trim_str_base<count_lines::yes, use_saves::yes>;

template<count_lines CountLines, use_saves Saves>
class _ex_trim_str_base : public _saving_trim_str_base<CountLines, Saves> {
public:
    using base = _saving_trim_str_base<CountLines, Saves>;
protected:
    std::deque<std::string>* _extracted;
    bool _extract_next;
public:
    _ex_trim_str_base(std::istream& src, std::deque<std::string>& extracted) 
        requires(CountLines == count_lines::no && Saves == use_saves::no) 
        : base(src), _extracted(&extracted), _extract_next(false) {}
    
    _ex_trim_str_base(std::istream& src, std::deque<index_t>& newlines, std::deque<std::string>& extracted) 
        requires(CountLines == count_lines::yes && Saves == use_saves::no) 
        : base(src, newlines), _extracted(&extracted), _extract_next(false) {}
    
    _ex_trim_str_base(std::istream& src, std::deque<index_t>& saved, std::deque<std::string>& extracted) 
        requires(CountLines == count_lines::no && Saves == use_saves::yes) 
        : base(src, saved), _extracted(&extracted), _extract_next(false) {}
    
    _ex_trim_str_base(std::istream& src, std::deque<index_t>& newlines, 
                      std::deque<tf::index_t>& saved, std::deque<std::string>& extracted) 
        requires(CountLines == count_lines::yes && Saves == use_saves::yes) 
        : base(src, newlines, saved), _extracted(&extracted), _extract_next(false) {}
    
    std::deque<std::string>* const& extracted() const noexcept { 
        return _extracted; 
    }
    std::deque<std::string>* extracted(std::deque<std::string>& extracted) { 
        return std::exchange(_extracted, &_extracted); 
    }

    void extract_next() noexcept { _extract_next = true; }
    std::string pop_extracted() {
        auto ret = std::move(_extracted->back());
        return _extracted->pop_back(), ret;
    }
};

template<count_lines CountLines, use_saves Saves>
class ex_trim_str_base : public _ex_trim_str_base<CountLines, Saves> {
public:
    static constexpr auto saves = Saves;
    using base = _ex_trim_str_base<CountLines, Saves>;
private:
    template<class... Fs> requires(Saves == use_saves::yes)
    tf::trim_fn_rslt _apply_saving_seq_base(const tf::saving_trim_seq<Fs...>& seq) {
        tf::trim_fn_rslt rslt = this->_pos;
        std::vector<tf::index_t> saves;
        saves.reserve(seq.saves);
        for (auto it = seq.funcs.begin(); it != seq.funcs.end() && rslt.has_value(); ++it) {
            if (it->index() == 0) {
                rslt = std::get<0>(*it)(*this, *rslt);
            } else {
                saves.push_back(*rslt);
            }
        }
        if (rslt)
            std::copy(saves.begin(), saves.end(), std::back_inserter(*this->_saved));
        return rslt;
    }

    template<class... Fs>
    tf::trim_fn_rslt _apply_ex_seq_base(const tf::ex_trim_seq<Fs...>& seq) {
        tf::trim_fn_rslt rslt = this->_pos;
        std::vector<std::string> extracted;
        extracted.reserve(seq.extracts);
        bool extract_next = false;
        for (auto it = seq.funcs.begin(); it != seq.funcs.end() && rslt.has_value(); ++it) {
            if (it->index() == 1) {
                extract_next = true;
            } else {
                auto n_rslt = std::get<0>(*it)(*this, *rslt);
                if (extract_next && n_rslt)
                    extracted.emplace_back(this->substr(*rslt, *n_rslt - *rslt));
                rslt = n_rslt, extract_next = false;
            }
        }
        if (rslt)
            std::move(extracted.begin(), extracted.end(), std::back_inserter(*this->_extracted));
        return rslt;
    }
public:
    using base::_ex_trim_str_base;

    using base::pos;
    auto& pos(tf::tags::out_t, tf::index_t& out) { return out = this->_pos, *this; }
    using base::linecol;
    auto& linecol(tf::tags::out_t, size_t& line, size_t& col) { 
        return std::tie(line, col) = linecol(this->_pos), *this; 
    }
    
    auto& save_pos(tf::tags::chain_t) requires(Saves == use_saves::yes) { return base::save_pos(), *this; }
    auto& save_pos(tf::tags::out_t, tf::index_t& out) requires(Saves == use_saves::yes) { 
        return out = this->_pos, base::save_pos(), *this; 
    }
    tf::index_t load_saved() requires(Saves == use_saves::yes) { return base::load_saved(); }
    auto& load_saved(tf::tags::chain_t) requires(Saves == use_saves::yes) { 
        return base::load_saved(), *this; 
    }
    auto& load_saved(tf::tags::out_t, tf::index_t& out) requires(Saves == use_saves::yes) { 
        return out = base::load_saved(), *this; 
    }
    tf::index_t pop_saved() requires(Saves == use_saves::yes) {
        return base::pop_saved();
    }
    auto& pop_saved(tf::tags::chain_t) requires(Saves == use_saves::yes) {
        return base::pop_saved(), *this;
    }
    auto& pop_saved(tf::tags::out_t, tf::index_t& out) requires(Saves == use_saves::yes) {
        return out = base::pop_saved(), *this;
    }

    auto& extract_next() { return base::extract_next(), *this; }

    template<class F, class... Args> requires(tf::trim_fn_c<F, Args...>)
    tf::trim_fn_rslt invoke(F&& f, Args&&... args) const {
        return f(*this, this->_pos, std::forward<Args>(args)...);
    }

    template<class... Fs>
    tf::trim_fn_rslt invoke(const tf::trim_seq<Fs...>& seq) const {
        tf::trim_fn_rslt rslt = this->_pos;
        for (auto it = seq.funcs.begin(); it != seq.funcs.end() && rslt.has_value(); ++it) {
            rslt = (*it)(*this, *rslt);
        }
        return rslt;
    }

    template<class... Fs>
    tf::trim_fn_rslt invoke(const tf::saving_trim_seq<Fs...>& seq) const {
        tf::trim_fn_rslt rslt = this->_pos;
        for (auto it = seq.funcs.begin(); it != seq.funcs.end() && rslt.has_value(); ++it) {
            if (it->index() == 0)
                rslt = std::get<0>(*it)(*this, *rslt);
        }
        return rslt;
    }

    template<class... Fs>
    tf::trim_fn_rslt invoke(const tf::ex_trim_seq<Fs...>& seq) const {
        tf::trim_fn_rslt rslt = this->_pos;
        for (auto it = seq.funcs.begin(); it != seq.funcs.end() && rslt.has_value(); ++it) {
            if (it->index() == 0)
                rslt = std::get<0>(*it)(*this, *rslt);
        }
        return rslt;
    }

    template<class F, class... Args> requires(tf::trim_fn_c<F, Args...>)
    tf::trim_fn_rslt apply_if_ok(F&& f, Args&&... args) {
        auto rslt = invoke(std::forward<F>(f), std::forward<Args>(args)...);
        if (rslt) {
            if (this->_extract_next)
                this->_extracted->emplace_back(this->substr(this->_pos, *rslt - this->_pos));
            this->_pos = *rslt, this->_upd_buf_start();
        }
        this->_extract_next = false;
        return rslt;
    }

    template<class... Fs>
    tf::trim_fn_rslt apply_if_ok(const tf::trim_seq<Fs...>& seq) {
        auto rslt = invoke(seq);
        if (rslt) {
            if (this->_extract_next)
                this->_extracted->emplace_back(this->substr(this->_pos, *rslt - this->_pos));
            this->_pos = *rslt, this->_upd_buf_start();
        }
        this->_extract_next = false;
        return rslt;
    }

    template<class... Fs> requires(Saves == use_saves::yes)
    tf::trim_fn_rslt apply_if_ok(const tf::saving_trim_seq<Fs...>& seq) {
        auto rslt = _apply_saving_seq_base(seq);
        if (rslt) {
            if (this->_extract_next)
                this->_extracted->emplace_back(this->substr(this->_pos, *rslt - this->_pos));
            this->_pos = *rslt, this->_upd_buf_start();
        }
        this->_extract_next = false;
        return rslt;
    }

    template<class... Fs>
    tf::trim_fn_rslt apply_if_ok(const tf::ex_trim_seq<Fs...>& seq) {
        auto rslt = _apply_ex_seq_base(seq);
        if (rslt) {
            if (this->_extract_next)
                this->_extracted->emplace_back(this->substr(this->_pos, *rslt - this->_pos));
            this->_pos = *rslt, this->_upd_buf_start();
        }
        this->_extract_next = false;
        return rslt;
    }

    template<class F, class... Args> requires(tf::trim_fn_c<F, Args...>)
    auto& apply(F&& f, Args&&... args) {
        auto rslt = invoke(std::forward<F>(f), std::forward<Args>(args)...);
        if (rslt && this->_extract_next)
            this->_extracted->emplace_back(this->substr(this->_pos, *rslt - this->_pos));
        if (this->_pos = rslt.value_or(-1); rslt) 
            this->_upd_buf_start();
        this->_extract_next = false;
        return *this;
    }

    template<class... Fs>
    auto& apply(const tf::trim_seq<Fs...>& seq) {
        auto rslt = invoke(seq);
        if (rslt && this->_extract_next)
            this->_extracted->emplace_back(this->substr(this->_pos, *rslt - this->_pos));
        if (this->_pos = rslt.value_or(-1); rslt) 
            this->_upd_buf_start();
        this->_extract_next = false;
        return *this;
    }

    template<class... Fs> requires(Saves == use_saves::yes)
    auto& apply(const tf::saving_trim_seq<Fs...>& seq) {
        auto rslt = _apply_saving_seq_base(seq);
        if (rslt && this->_extract_next)
            this->_extracted->emplace_back(this->substr(this->_pos, *rslt - this->_pos));
        if (this->_pos = rslt.value_or(-1); rslt) 
            this->_upd_buf_start();
        this->_extract_next = false;
        return *this;
    }

    template<class... Fs>
    auto& apply(const tf::ex_trim_seq<Fs...>& seq) {
        auto rslt = _apply_ex_seq_base(seq);
        if (rslt && this->_extract_next)
            this->_extracted->emplace_back(this->substr(this->_pos, *rslt - this->_pos));
        if (this->_pos = rslt.value_or(-1); rslt) 
            this->_upd_buf_start();
        this->_extract_next = false;
        return *this;
    }
};

using ex_trim_str = ex_trim_str_base<count_lines::yes, use_saves::yes>;

}
