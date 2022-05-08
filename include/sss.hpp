#pragma once

#include <algorithm>
#include <complex>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <bitset>
#include <chrono>
#include <filesystem>

#if defined __has_include
#   if __has_include (<boost/pfr.hpp>)
#       include <boost/pfr.hpp>
#       define SSS_PFR_NAMESPACE namespace pfr = boost::pfr
#   elif __has_include (<pfr.hpp>)
#       include <pfr.hpp>
#       define SSS_PFR_NAMESPACE
#   endif
#endif

#define SSS_STATIC_FAIL(msg) []<bool _ = false>(){ static_assert(_, msg); }();

namespace sss {

SSS_PFR_NAMESPACE;

namespace detail {
    template <auto Start, auto End, auto Inc, class F>
    constexpr void constexpr_for(F&& f) {
        if constexpr (Start < End) {
            f(std::integral_constant<decltype(Start), Start>());
            constexpr_for<Start + Inc, End, Inc>(f);
        }
    }

    template <class F, class Tuple>
    constexpr void constexpr_for_tuple(F&& f, Tuple&& tuple) {
        constexpr size_t size = std::tuple_size_v<std::decay_t<Tuple>>;
        constexpr_for<0UL, size, 1UL>([&](auto i) {
            f(std::get<i.value>(tuple));
        });
    }
}

using byte_view = std::span<const std::byte>;
using writable_byte_view = std::span<std::byte>;

template <typename T>
struct serializer {};

template <typename T>
concept is_serializable = requires(T a) {
    { serializer<T>::to_bytes(a, std::declval<writable_byte_view>()) } -> std::integral;
    { serializer<T>::size(a) } -> std::integral;
    { serializer<T>::from_bytes(std::declval<byte_view&>()) } -> std::same_as<T>;
};

template <typename T>
constexpr std::size_t serialize(const T& obj, writable_byte_view buffer) {
    if constexpr (is_serializable<T>) {
        return serializer<T>::to_bytes(obj, buffer);
    } else if constexpr (std::is_trivial_v<T>) {
        std::span obj_buf(reinterpret_cast<const std::byte*>(&obj), sizeof(obj));
        std::ranges::copy(obj_buf, buffer.data());
        return sizeof(T);
    } else if constexpr (std::is_aggregate_v<T> && !std::is_array_v<T>) {
        std::size_t bytes_written = 0;
        pfr::for_each_field(obj, [&](const auto& field) {
            bytes_written += serialize(field, buffer.subspan(bytes_written));
        });
        return bytes_written;
    } else {
        SSS_STATIC_FAIL("Invalid type");
    }
}

template <typename T>
constexpr std::size_t serialized_size(const T& obj) {
    if constexpr (is_serializable<T>) {
        return serializer<T>::size(obj);
    } else if constexpr (std::is_trivial_v<T>) {
        return sizeof(T);
    } else if constexpr (std::is_aggregate_v<T> && !std::is_array_v<T>) {
        std::size_t size = 0;
        pfr::for_each_field(obj, [&](const auto& field) {
            size += serialized_size(field);
        });
        return size;
    } else {
        SSS_STATIC_FAIL("Invalid type");
    }
}

template <typename T>
constexpr T deserialize(byte_view& buffer) {
    if constexpr (is_serializable<T>) {
        return serializer<T>::from_bytes(buffer);
    } else if constexpr (std::is_trivial_v<T>) {
        T val;
        auto ptr = reinterpret_cast<std::byte*>(&val);
        std::copy_n(buffer.data(), sizeof(T), ptr);
        buffer = buffer.subspan(sizeof(T));
        return val;
    } else if constexpr (std::is_aggregate_v<T> && !std::is_array_v<T>) {
        T val;
        pfr::for_each_field(val, [&](auto& field) {
            field = deserialize<std::remove_reference_t<decltype(field)>>(buffer);
        });
        return val;
    } else {
        SSS_STATIC_FAIL("Invalid type");
    }
}

template <typename T>
std::vector<std::byte> save(const T& obj) {
    auto n_bytes = serialized_size(obj);
    std::vector<std::byte> buffer(n_bytes);
    serialize(obj, buffer);
    return buffer;
}

template <typename T>
T load(const std::vector<std::byte>& buffer) {
    byte_view view(buffer);
    return deserialize<T>(view);
}

template <typename... Ts>
struct serializer<std::tuple<Ts...>>
{
    static constexpr std::size_t to_bytes(const std::tuple<Ts...>& t, writable_byte_view buffer) {
        std::size_t bytes_written = 0;
        detail::constexpr_for_tuple([&](const auto& field) {
            bytes_written += serialize(field, buffer.subspan(bytes_written));
        }, t);
        return bytes_written;
    }

    static constexpr std::size_t size(const std::tuple<Ts...>& t) {
        std::size_t size = 0;
        detail::constexpr_for_tuple([&](const auto& field) {
            size += serialized_size(field);
        }, t);
        return size;
    }

    static constexpr std::tuple<Ts...> from_bytes(byte_view& buffer) {
        std::tuple<Ts...> out;
        detail::constexpr_for_tuple([&](auto& field) mutable {
            field = deserialize<std::remove_reference_t<decltype(field)>>(buffer);
        }, out);
        return out;
    }
};

template <typename T, typename U>
struct serializer<std::pair<T, U>>
{
    static constexpr std::size_t to_bytes(const std::pair<T, U>& p, writable_byte_view buffer) {
        auto n_1 = serialize(p.first, buffer);
        auto n_2 = serialize(p.second, buffer.subspan(n_1));
        return n_1 + n_2;
    }

    static constexpr std::size_t size(const std::pair<T, U>& p) {
        return serialized_size(p.first) + serialized_size(p.second);
    }

    static constexpr std::pair<T, U> from_bytes(byte_view& buffer) {
        std::pair<T, U> out;
        out.first = deserialize<T>(buffer);
        out.second = deserialize<U>(buffer);
        return out;
    }
};

template <typename T>
struct serializer<std::optional<T>>
{
    static constexpr std::size_t to_bytes(const std::optional<T>& opt, writable_byte_view buffer) {
        if (opt) {
            buffer.front() = std::byte(1);
            return serialize(*opt, buffer.subspan(1)) + 1;
        } else {
            buffer.front() = std::byte(0);
            return 1;
        }
    }

    static constexpr std::size_t size(const std::optional<T>& opt) {
        return opt ? 1 + serialized_size(*opt) : 1;
    }

    static constexpr std::optional<T> from_bytes(byte_view& buffer) {
        auto has_value = bool(buffer.front());
        buffer = buffer.subspan(1);
        if (has_value) {
            return deserialize<T>(buffer);
        } else {
            return {};
        }
    }
};

template <typename... Ts>
struct serializer<std::variant<Ts...>>
{
    static constexpr std::size_t to_bytes(const std::variant<Ts...>& v, writable_byte_view buffer) {
        buffer.front() = std::byte(v.index());
        return 1 + std::visit([&](const auto& field) { return serialize(field, buffer.subspan(1)); }, v);
    }

    static constexpr std::size_t size(const std::variant<Ts...>& v) {
        return 1 + std::visit([&](const auto& field) { return serialized_size(field); }, v);
    }

    static constexpr std::variant<Ts...> from_bytes(byte_view& buffer) {
        auto index = int(buffer.front());
        buffer = buffer.subspan(1);

        std::variant<Ts...> out;
        detail::constexpr_for<0, sizeof...(Ts), 1>([&](auto i) {
            if (index == i.value) {
                out = deserialize<std::variant_alternative_t<i.value, std::variant<Ts...>>>(buffer);
            }
        });

        return out;
    }
};

namespace detail {
    template<typename T>
    std::size_t serialize_container(const T& container, writable_byte_view buffer) {
        auto bytes_written = serialize(std::size(container), buffer);
        for (const auto& item : container) {
            bytes_written += serialize(item, buffer.subspan(bytes_written));
        }
        return bytes_written;
    }

    template<typename T>
    std::size_t serialized_container_size(const T& container) {
        auto s = sizeof(container.size());
        for (const auto& item : container) {
            s += serialized_size(item);
        }
        return s;
    }

    std::size_t container_elements(byte_view buffer) {
        return deserialize<std::size_t>(buffer);
    }

    template <typename T>
    void deserialize_container(byte_view& buffer, std::function<void(T)> callback) {
        auto size = deserialize<std::size_t>(buffer);
        for (std::size_t i = 0; i < size; ++i) {
            callback(deserialize<T>(buffer));
        }
    }

    constexpr std::size_t ceil_divide(std::size_t a, std::size_t b) {
        return (a + b - 1) / b;
    }

    constexpr void write_ith_bit(std::size_t i, bool bit, writable_byte_view buffer) {
        auto n_byte = i / CHAR_BIT;
        auto n_bit = i % CHAR_BIT;
        buffer[n_byte] |= std::byte(bit << n_bit);
    }

    constexpr bool read_ith_bit(std::size_t i, byte_view buffer) {
        auto n_byte = i / CHAR_BIT;
        auto n_bit = i % CHAR_BIT;
        return bool(buffer[n_byte] & std::byte(1U << n_bit));
    }
}

template <typename T>
struct serializer<std::basic_string<T>>
{
    static std::size_t to_bytes(const std::string& s, writable_byte_view buffer) {
        return detail::serialize_container(s, buffer);
    }

    static std::size_t size(const std::string& s) {
        return detail::serialized_container_size(s);
    }

    static std::string from_bytes(byte_view& buffer) {
        std::string out;
        out.reserve(detail::container_elements(buffer));
        detail::deserialize_container<T>(buffer, [&](auto&& c) { out.push_back(c); });
        return out;
    }
};

template <typename T, typename Alloc>
struct serializer<std::vector<T, Alloc>>
{
    static std::size_t to_bytes(const std::vector<T, Alloc>& vec, writable_byte_view buffer) {
        return detail::serialize_container(vec, buffer);
    }

    static std::size_t size(const std::vector<T, Alloc>& vec) {
        return detail::serialized_container_size(vec);
    }

    static std::vector<T, Alloc> from_bytes(byte_view& buffer) {
        std::vector<T, Alloc> out;
        out.reserve(detail::container_elements(buffer));
        detail::deserialize_container<T>(buffer, [&](auto&& item) { out.push_back(std::move(item)); });
        return out;
    }
};

template <typename K, typename V>
struct serializer<std::unordered_map<K, V>>
{
    static std::size_t to_bytes(const std::unordered_map<K, V>& map, writable_byte_view buffer) {
        return detail::serialize_container(map, buffer);
    }

    static std::size_t size(const std::unordered_map<K, V>& map) {
        return detail::serialized_container_size(map);
    }

    static std::unordered_map<K, V> from_bytes(byte_view& buffer) {
        std::unordered_map<K, V> out;
        detail::deserialize_container<std::pair<K, V>>(buffer, [&](auto&& item) { out.insert(std::move(item)); });
        return out;
    }
};

template <typename K, typename V>
struct serializer<std::map<K, V>>
{
    static std::size_t to_bytes(const std::map<K, V>& map, writable_byte_view buffer) {
        return detail::serialize_container(map, buffer);
    }

    static std::size_t size(const std::map<K, V>& map) {
        return detail::serialized_container_size(map);
    }

    static std::map<K, V> from_bytes(byte_view& buffer) {
        std::map<K, V> out;
        detail::deserialize_container<std::pair<K, V>>(buffer, [&](auto&& item) { out.insert(std::move(item)); });
        return out;
    }
};

template <typename T>
struct serializer<std::set<T>>
{
    static std::size_t to_bytes(const std::set<T>& set, writable_byte_view buffer) {
        return detail::serialize_container(set, buffer);
    }

    static std::size_t size(const std::set<T>& set) {
        return detail::serialized_container_size(set);
    }

    static std::set<T> from_bytes(byte_view& buffer) {
        std::set<T> out;
        detail::deserialize_container<T>(buffer, [&](auto&& item) { out.insert(std::move(item)); });
        return out;
    }
};

template <typename T>
struct serializer<std::unordered_set<T>>
{
    static std::size_t to_bytes(const std::unordered_set<T>& set, writable_byte_view buffer) {
        return detail::serialize_container(set, buffer);
    }

    static std::size_t size(const std::unordered_set<T>& set) {
        return detail::serialized_container_size(set);
    }

    static std::unordered_set<T> from_bytes(byte_view& buffer) {
        std::unordered_set<T> out;
        detail::deserialize_container<T>(buffer, [&](auto&& item) { out.insert(std::move(item)); });
        return out;
    }
};

template <typename T>
struct serializer<std::complex<T>>
{
    static constexpr std::size_t to_bytes(const std::complex<T>& cmplx, writable_byte_view buffer) {
        auto n_1 = serialize(cmplx.real(), buffer);
        auto n_2 = serialize(cmplx.imag(), buffer.subspan(n_1));
        return n_1 + n_2;
    }

    static constexpr std::size_t size(const std::complex<T>& cmplx) {
        return serialized_size(cmplx.real()) + serialized_size(cmplx.imag());
    }

    static constexpr std::complex<T> from_bytes(byte_view& buffer) {
        std::complex<T> out;
        out.real(deserialize<T>(buffer));
        out.imag(deserialize<T>(buffer));
        return out;
    }
};

template <typename Alloc>
struct serializer<std::vector<bool, Alloc>>
{
    static std::size_t to_bytes(const std::vector<bool, Alloc>& vec, writable_byte_view buffer) {
        auto bytes_written = serialize(vec.size(), buffer);
        buffer = buffer.subspan(bytes_written);

        auto n_bytes = detail::ceil_divide(vec.size(), CHAR_BIT);
        buffer[n_bytes - 1] = std::byte(0);
        for (std::size_t i = 0; i < vec.size(); ++i) {
            detail::write_ith_bit(i, bool(vec[i]), buffer);
        }
        return sizeof(vec.size()) + n_bytes;
    }

    static std::size_t size(const std::vector<bool, Alloc>& vec) {
        return sizeof(vec.size()) + detail::ceil_divide(vec.size(), CHAR_BIT);
    }

    static std::vector<bool, Alloc> from_bytes(byte_view& buffer) {
        auto size = deserialize<std::size_t>(buffer);
        std::vector<bool, Alloc> out(size);
        for (std::size_t i = 0; i < size; ++i) {
            out[i] = detail::read_ith_bit(i, buffer);
        }
        buffer = buffer.subspan(detail::ceil_divide(size, CHAR_BIT));
        return out;
    }
};

template <std::size_t N>
struct serializer<std::bitset<N>>
{
    static std::size_t to_bytes(const std::bitset<N>& bitset, writable_byte_view buffer) {
        auto n_bytes = detail::ceil_divide(N, CHAR_BIT);
        for (std::size_t i = 0; i < N; ++i) {
            detail::write_ith_bit(i, bool(bitset[i]), buffer);
        }
        return n_bytes;
    }

    static std::size_t size(const std::bitset<N>& bitset) {
        return detail::ceil_divide(N, CHAR_BIT);
    }

    static std::bitset<N> from_bytes(byte_view& buffer) {
        std::bitset<N> out;
        for (std::size_t i = 0; i < N; ++i) {
            out[i] = detail::read_ith_bit(i, buffer);
        }
        buffer = buffer.subspan(detail::ceil_divide(N, CHAR_BIT));
        return out;
    }
};

template <typename Rep, typename Period>
struct serializer<std::chrono::duration<Rep, Period>> {
    static constexpr std::size_t to_bytes(const std::chrono::duration<Rep, Period>& d, writable_byte_view buffer) {
        return serialize(d.count(), buffer);
    }

    static constexpr std::size_t size(const std::chrono::duration<Rep, Period>& d) {
        return sizeof(d.count());
    }

    static constexpr std::chrono::duration<Rep, Period> from_bytes(byte_view& buffer) {
        return std::chrono::duration<Rep, Period>(deserialize<Rep>(buffer));
    }
};

template <typename Clock, typename Duration>
struct serializer<std::chrono::time_point<Clock, Duration>> {
    static constexpr std::size_t to_bytes(const std::chrono::time_point<Clock, Duration>& t, writable_byte_view buffer) {
        return serialize(t.time_since_epoch().count(), buffer);
    }

    static constexpr std::size_t size(const std::chrono::time_point<Clock, Duration>& t) {
        return sizeof(t.time_since_epoch().count());
    }

    static constexpr std::chrono::time_point<Clock, Duration> from_bytes(byte_view& buffer) {
        return std::chrono::time_point<Clock, Duration>(
            Duration(deserialize<typename Duration::rep>(buffer))
        );
    }
};

template <>
struct serializer<std::filesystem::path> {
    static std::size_t to_bytes(const std::filesystem::path& p, writable_byte_view buffer) {
        return serialize(p.string(), buffer);
    }

    static std::size_t size(const std::filesystem::path& p) {
        return serialized_size(p.string());
    }

    static std::filesystem::path from_bytes(byte_view& buffer) {
        return std::filesystem::path(deserialize<std::string>(buffer));
    }
};

}