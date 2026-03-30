#pragma once

#include <stddef.h>
#include <stdint.h>

#define TXT_TYPE_PLAIN          0    // a plain text message
#define TXT_TYPE_CLI_DATA       1    // a CLI command
#define TXT_TYPE_SIGNED_PLAIN   2    // plain text, signed by sender

class StrHelper {
public:
  static void strncpy(char* dest, const char* src, size_t buf_sz);
  static void strzcpy(char* dest, const char* src, size_t buf_sz);   // pads with trailing nulls
  static const char* ftoa(float f);
  static const char* ftoa3(float f); //Converts float to string with 3 decimal places
  static bool isBlank(const char* str);
  static uint32_t fromHex(const char* src);

  /**
   * \brief  Transliterate Russian (Cyrillic) UTF-8 characters to Latin ASCII equivalents.
   *         Non-Russian characters are passed through unchanged.
   * \param  dest   destination buffer for transliterated string
   * \param  src    source UTF-8 string
   * \param  buf_sz  size of destination buffer
   * \returns  number of bytes written to dest (excluding null terminator)
   */
  static int transliterateUtf8(char* dest, const char* src, size_t buf_sz);
};
