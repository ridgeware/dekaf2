#include <string>

constexpr std::string get_first_word(std::string s)
{
	size_t n = s.find_first_not_of(' ');
	if (n == s.npos) return {};
	return s.substr(n, s.find(' ', n) - n);
}

constexpr std::string constexpr_str()
{
	return "  constexpr test ";
}

static_assert(get_first_word(constexpr_str()) == std::string("constexpr"));

int main()
{
	return constexpr_str()[0];
}

