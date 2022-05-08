#include "catch.hpp"
#include "sss.hpp"

template <typename T>
struct pre_post
{
    int pre;
    T val;
    int post;

    bool operator==(const pre_post&) const = default;
};

template <typename T>
bool check_round_trip(T value) {
    pre_post<T> original{10, std::move(value), 20};
    auto new_obj = sss::load<pre_post<T>>(sss::save(original));
    return original == new_obj;
}

TEST_CASE("Round Trip") {
    const std::string t = "test";

    REQUIRE((check_round_trip(15)));
    REQUIRE((check_round_trip(3.4)));
    REQUIRE((check_round_trip(t)));
    REQUIRE((check_round_trip(std::tuple<std::string, int>(t, 17))));
    REQUIRE((check_round_trip(std::pair<std::string, int>(t, 17))));
    REQUIRE((check_round_trip(std::optional<std::string>(t))));
    REQUIRE((check_round_trip(std::optional<std::string>())));
    REQUIRE((check_round_trip(std::variant<int, std::string>(17))));
    REQUIRE((check_round_trip(std::variant<int, std::string>(t))));
    REQUIRE((check_round_trip(std::vector<std::string>{"abc", "def"})));
    REQUIRE((check_round_trip(std::array<std::string, 2>{"abc", "def"})));
    REQUIRE((check_round_trip(std::unordered_map<int, std::string>{{1, "abc"}, {2, "def"}})));
    REQUIRE((check_round_trip(std::map<int, std::string>{{1, "abc"}, {2, "def"}})));
    REQUIRE((check_round_trip(std::unordered_set<std::string>{"abc", "def"})));
    REQUIRE((check_round_trip(std::set<std::string>{"abc", "def"})));
    REQUIRE((check_round_trip(std::complex<double>(1.1, 2.2))));
    REQUIRE((check_round_trip(std::vector<bool>{true, false, true, true, false, false, false, true, true})));
    REQUIRE((check_round_trip(std::bitset<9>("101100011"))));
    REQUIRE((check_round_trip(std::chrono::system_clock::now().time_since_epoch())));
    REQUIRE((check_round_trip(std::chrono::system_clock::now())));
    REQUIRE((check_round_trip(std::filesystem::current_path())));
}