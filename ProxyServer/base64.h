#include <string>

std::string base64_encode_str(std::string s);
std::string base64_encode(unsigned char const* bytes_to_encode, unsigned int in_len);
std::string base64_decode(std::string const& s);
