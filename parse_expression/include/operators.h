#pragma once

namespace operators {
    
enum class unary_opr {
    exclam,
    postf_inc, postf_dec,
    pref_inc, pref_dec,
    plus, minus,
    logic_not, bit_not,
    ref, deref, rref,
    _count = 12
};
enum class binary_opr {
    scope, arrow, dot, func_call,
    mult, div, mod,
    add, diff,
    bit_shift_l, bit_shift_r,
    less, less_eq, 
    greater, greater_eq,
    equal, not_equal,
    bit_and, bit_xor, bit_or,
    logic_and, logic_or,
    asgmt, asgmt_sum, asgmt_diff,
    asgmt_mult, asgmt_div, asgmt_mod,
    asgmt_shift_l, asgmt_shift_r,
    asgmt_and, asgmt_or, asgmt_xor,
    comma,
    _count = 34
};
enum class ternary_opr {
    condition,
    ways,
    _count = 2
};

template<class T>
concept type_ok =
    std::is_same_v<unary_opr, T> || std::is_same_v<binary_opr, T> || std::is_same_v<ternary_opr, T>;

template<type_ok T>
struct priority {
};

template<>
struct priority<unary_opr> {
    const static constexpr std::array<size_t, std::to_underlying(unary_opr::_count)> arr = {
        1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2
    };
    constexpr size_t operator()(unary_opr opr) const noexcept {
        return arr[std::to_underlying(opr)];
    }
};

template<>
struct priority<binary_opr> {
    const static constexpr std::array<size_t, std::to_underlying(binary_opr::_count)> arr = {
        0, 0, 0, 0, 4, 4, 4, 5, 5, 6, 6, 7, 7, 7, 7, 8, 8, 9, 10, 11, 12, 13, 14, 14, 14, 14,
        14, 14, 14, 14, 14, 14, 14, 15
    };
    constexpr size_t operator()(binary_opr opr) const noexcept {
        return arr[std::to_underlying(opr)];
    }
};

template<>
struct priority<ternary_opr> {
    const static constexpr std::array<size_t, std::to_underlying(ternary_opr::_count)> arr = { 14, 14 };
    constexpr size_t operator()(ternary_opr opr) const noexcept {
        return arr[std::to_underlying(opr)];
    }
};

inline size_t opr_priority(opr _opr) noexcept {
    if (std::holds_alternative<unary_opr>(_opr))
        return priority<unary_opr>()(std::get<unary_opr>(_opr));
    if (std::holds_alternative<binary_opr>(_opr))
        return priority<binary_opr>()(std::get<binary_opr>(_opr));
    if (std::holds_alternative<ternary_opr>(_opr))
        return priority<ternary_opr>()(std::get<ternary_opr>(_opr));
}

inline bool __cmp(opr opr_1, opr opr_2) {
    return (opr_assoc(opr_2) == _ltr
                ? opr_priority(opr_1) >= opr_priority(opr_2)
                : opr_priority(opr_1) > opr_priority(opr_2));
}

enum class associativity {
    left_to_right, right_to_left, none
};

constexpr associativity _ltr = associativity::left_to_right;
constexpr associativity _rtl = associativity::right_to_left;
constexpr associativity _none = associativity::none;

template<type_ok T>
struct associative {
};

template<>
struct associative<unary_opr> {
    const static constexpr std::array<associativity, std::to_underlying(unary_opr::_count)> arr = {
       _ltr, _ltr, _ltr, _rtl, _rtl, _rtl, _rtl, _rtl, _rtl, _rtl, _rtl, _rtl
    };
    constexpr associativity operator()(unary_opr opr) const noexcept {
        return arr[std::to_underlying(opr)];
    }
};

template<>
struct associative<binary_opr> {
    const static constexpr std::array<associativity, std::to_underlying(binary_opr::_count)> arr = {
        _none, _ltr, _ltr, _ltr, _ltr, _ltr, _ltr, _ltr, _ltr, _ltr, _ltr, _ltr, _ltr, _ltr, _ltr,
        _ltr, _ltr, _ltr, _ltr, _ltr, _ltr, _ltr, _rtl, _rtl, _rtl, _rtl, _rtl, _rtl, _rtl, _rtl,
        _rtl, _rtl, _rtl, _ltr
    };
    constexpr associativity operator()(binary_opr opr) const noexcept {
        return arr[std::to_underlying(opr)];
    }
};

template<>
struct associative<ternary_opr> {
    const static constexpr std::array<associativity, std::to_underlying(ternary_opr::_count)> arr = { _rtl, _rtl };
    constexpr associativity operator()(ternary_opr opr) const noexcept {
        return arr[std::to_underlying(opr)];
    }
};

template<type_ok T>
using assoc = associative<T>;

inline associativity opr_assoc(opr _opr) noexcept {
    if (std::holds_alternative<unary_opr>(_opr))
        return assoc<unary_opr>()(std::get<unary_opr>(_opr));
    if (std::holds_alternative<binary_opr>(_opr))
        return assoc<binary_opr>()(std::get<binary_opr>(_opr));
    if (std::holds_alternative<ternary_opr>(_opr))
        return assoc<ternary_opr>()(std::get<ternary_opr>(_opr));
}

template<type_ok T>
struct opr_design {
};

template<>
struct opr_design<unary_opr> {
    static constexpr std::array<char*, std::to_underlying(unary_opr::_count)> arr = {
        "!", "++", "--", "++", "--", "+", "-", "!", "~", "&", "*", "&&"
    };
    unary_opr operator()(std::string_view opr_design) const noexcept {
        return static_cast<unary_opr>(std::find(arr.begin(), arr.end(), opr_design) - arr.begin());
    }
};

template<>
struct opr_design<binary_opr> {
    static constexpr std::array<char*, std::to_underlying(binary_opr::_count)> arr = {
        "::", "->", ".", "()", "*", "/", "%", "+", "-", "<<", ">>", "<", "<=", ">", ">=", "==",
        "!=", "&", "^", "|", "&&", "||", "=", "+=", "-=", "*=", "/=", "%=", "<<=", ">>=",
        "&=", "|=", "^=", ","
    };
    binary_opr operator()(std::string_view opr_design) const noexcept {
        return static_cast<binary_opr>(std::find(arr.begin(), arr.end(), opr_design) - arr.begin());
    }
};

template<>
struct opr_design<ternary_opr> {
    static constexpr std::array<char*, std::to_underlying(ternary_opr::_count)> arr = { "?", ":" };
    ternary_opr operator()(std::string_view opr_design) const noexcept {
        return static_cast<ternary_opr>(std::find(arr.begin(), arr.end(), opr_design) - arr.begin());
    }
};

using opr = std::variant<unary_opr, binary_opr, ternary_opr>;

std::expected<opr, std::string_view> opr_char(std::string_view opr_char) noexcept {
    opr rslt = opr_design<unary_opr>()(opr_char);
    if (std::get<unary_opr>(rslt) != unary_opr::_count) return rslt;
    rslt = opr_design<binary_opr>()(opr_char);
    if (std::get<binary_opr>(rslt) != binary_opr::_count) return rslt;
    rslt = opr_design<ternary_opr>()(opr_char);
    if (std::get<ternary_opr>(rslt) != ternary_opr::_count) return rslt;
    return std::unexpected(opr_char);
}

}

namespace opr = operators;
