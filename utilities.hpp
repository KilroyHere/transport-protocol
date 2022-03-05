#ifndef UTILITIES_HPP
#define UTILITIES_HPP
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

#endif