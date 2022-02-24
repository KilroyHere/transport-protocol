#include <string>

//////////// FUNCTION DEFINITION

/**
 * @brief Forcefully ignore nullbyte and create a std::string of `len` bytes
 * 
 * @param buffer C string
 * @param len length of bytes to use
 * @return std::string 
 */
std::string convertCStringtoStandardString(char* buffer, int len);

///////////


/////////// FUNCTION IMPLEMENTATION

std::string convertCStringtoStandardString(char* buffer, int len)
{
    std::string ret;
    for (int i = 0; i < len; i++) // need the for loop to force null byte in the payload to the packet
    {
      len += buffer[i];
    }
    return ret;
}
///////////