#ifndef TESTING_COMMONS_JSONBUILDER_H_INCLUDED
#define TESTING_COMMONS_JSONBUILDER_H_INCLUDED

#include <blend2d/blend2d.h>

namespace blbench {

class JSONBuilder {
public:
  enum Token : uint32_t {
    kTokenNone  = 0,
    kTokenValue = 1
  };

  explicit JSONBuilder(BLString* dst);

  JSONBuilder& open_array();
  JSONBuilder& close_array(bool nl = false);

  JSONBuilder& open_object();
  JSONBuilder& close_object(bool nl = false);

  JSONBuilder& comma();

  JSONBuilder& add_key(const char* str);

  JSONBuilder& add_bool(bool b);
  JSONBuilder& add_int(int64_t n);
  JSONBuilder& add_uint(uint64_t n);
  JSONBuilder& add_double(double d);
  JSONBuilder& add_doublef(const char* fmt, double d);
  JSONBuilder& add_string(const char* str);
  JSONBuilder& add_stringf(const char* fmt, ...);
  JSONBuilder& add_string_no_quotes(const char* str);

  JSONBuilder& align_to(size_t n);
  JSONBuilder& before_record();

  JSONBuilder& nl() { _dst->append('\n'); return *this; }
  JSONBuilder& indent() { _dst->append(' ', _level); return *this; }

  BLString* _dst {};
  uint32_t _last {};
  uint32_t _level {};
};

} // {blbench} namespace

#endif // TESTING_COMMONS_JSONBUILDER_H_INCLUDED
