#include <string>
#include "utilities.hpp"
std::string convertCStringtoStandardString(char* buffer, int len)
{
    std::string ret = "";
    for (int i = 0; i < len; i++) // need the for loop to force null byte in the payload to the packet
    {
      char c = buffer[i];
      ret += c;
    }
    return ret;
}