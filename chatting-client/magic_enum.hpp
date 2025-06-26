#ifndef _MAGIC_ENUM_HPP_
#define _MAGIC_ENUM_HPP_
#include <cstring>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace reflect {
template<typename _Ty, _Ty value>
static std::string get_type() {
#if  _MSC_VER
    std::string tmp = __FUNCSIG__;
    std::string function_name = __FUNCTION__;

    auto start = tmp.find(function_name + '<') + function_name.length() + 1;
    auto reflect = tmp.find(",", start) + 1;
    start = reflect;
    auto end = tmp.find(">(void)", reflect);

#else
    constexpr const char* prefix = "N = ";
    std::string tmp = __PRETTY_FUNCTION__;

    auto start = tmp.find(prefix) + strlen(prefix);
    auto end = tmp.find_first_of(";]", start);

#endif //  _MSC_VER
    return tmp.substr(start, end - start);
}

template<std::size_t _Beg, std::size_t _End, typename _Callable>
static void static_for(_Callable&& func) {
    if constexpr (_Beg != _End) {
        func(std::integral_constant<std::size_t, _Beg>());
        static_for<_Beg + 1, _End>(std::forward<_Callable>(func));
    }
}

template<typename _Ty, std::size_t _Beg = 0, std::size_t _End = 256>
static std::string get_enum_name(_Ty n) {
    std::string ret;
    static_for<_Beg, _End>([&](auto _constexpr_value) {
        constexpr auto value = _constexpr_value.value;
        if (static_cast<_Ty>(value) == n)
            ret = get_type<_Ty, static_cast<_Ty>(value)>();
    });
    if (ret.empty())
        return std::to_string(std::size_t(n));
    return ret;
}

template<typename _Ret, std::size_t _Beg = 0, std::size_t _End = 256>
static _Ret name_to_enum(const std::string& name) {
    for (std::size_t start = _Beg; start < _End; ++start) {
        if (get_enum_name(static_cast<_Ret>(start)) == name)
            return static_cast<_Ret>(start);
    }
    //exception
    throw std::runtime_error("No Enum Found!!");
}
}
#endif //_MAGIC_ENUM_HPP_
