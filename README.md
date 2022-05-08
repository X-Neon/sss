# SSS

Stupid Simple Serialization (SSS) is a serialization library for C++20. It serializes types to a naive binary format - trivial types use their memory representations, `std::string`s are a length field followed by a `char` array, etc. SSS makes no attempt to correct for endianness, integral width differences, etc. between platforms. It is assumed that the serialization and deserialization platforms are the same.

## Example

```cpp
struct position
{
    double latitude;
    double longitude;
    std::optional<double> altitude;
};

struct request
{
    uint64_t id;
    std::chrono::seconds time;
    position user_position;
};

void do_round_trip(request original_request) {
    std::vector<std::byte> bytes = sss::save(original_request);
    request new_request = sss::load<request>(bytes);
    assert(original == new_obj);
}
```

## API

```cpp
using byte_view = std::span<const std::byte>;
using writable_byte_view = std::span<std::byte>;

// Serializes an object by writing to the buffer
template <typename T>
constexpr std::size_t serialize(const T& obj, writable_byte_view buffer);

// Return the size (in bytes) of the buffer required to serialize the object
template <typename T>
constexpr std::size_t serialized_size(const T& obj);

// Deserializes the object by reading from the buffer and returning the object
// This function will advance the passed-in buffer by the number of bytes read
// (It will not modify the underlying bytes of the buffer)
template <typename T>
constexpr T deserialize(byte_view& buffer);

// Convenience function for serializing an object to a byte vector
template <typename T>
std::vector<std::byte> save(const T& obj);

// Convenience function for deserializing an object from a byte vector
template <typename T>
T load(const std::vector<std::byte>& buffer);
```

## Custom serialization/deserialization

Serialization/deserialization for custom types can be implemented by specializing `sss::serializer`.

```cpp
namespace sss
{

template <>
struct serializer<my_type>
{
    static constexpr std::size_t to_bytes(const my_type& obj, writable_byte_view buffer) {
        // Serialize obj to buffer
    }

    static constexpr std::size_t size(const my_type& obj) {
        // Return the size (in bytes) of the buffer required to serialize the object
    }

    static constexpr my_type from_bytes(byte_view& buffer) {
        // Deserialize the object from the buffer, and advance the buffer by the
        // number of bytes read
    }
};

}
```

## Requirements

SSS requires a C++20 compliant compiler and PFR (either the [Boost](https://github.com/boostorg/pfr) version or the [non-Boost](https://github.com/apolukhin/pfr_non_boost) version). Tested on GCC 11.2 / Ubuntu 22.04.
