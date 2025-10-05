#include "jsonbuilder.h"

namespace blbench {

JSONBuilder::JSONBuilder(BLString* dst)
  : _dst(dst),
    _last(kTokenNone),
    _level(0) {}

JSONBuilder& JSONBuilder::open_array() {
  if (_last == kTokenValue)
    _dst->append(',');

  _dst->append('[');
  _last = kTokenNone;
  _level++;

  return *this;
}

JSONBuilder& JSONBuilder::close_array(bool nl) {
  _level--;
  if (nl) {
    _dst->append('\n');
    _dst->append(' ', _level * 2);
  }

  _dst->append(']');
  _last = kTokenValue;

  return *this;
}

JSONBuilder& JSONBuilder::open_object() {
  if (_last == kTokenValue)
    _dst->append(',');

  _dst->append('{');
  _last = kTokenNone;
  _level++;

  return *this;
}

JSONBuilder& JSONBuilder::close_object(bool nl) {
  _level--;
  if (nl) {
    _dst->append('\n');
    _dst->append(' ', _level * 2);
  }

  _dst->append('}');
  _last = kTokenValue;
  return *this;
}

JSONBuilder& JSONBuilder::comma() {
  if (_last == kTokenValue)
    _dst->append(',');
  _last = kTokenNone;

  return *this;
}

JSONBuilder& JSONBuilder::add_key(const char* str) {
  add_string(str);

  _dst->append(':');
  _last = kTokenNone;

  return *this;
}

JSONBuilder& JSONBuilder::add_bool(bool b) {
  if (_last == kTokenValue)
    _dst->append(',');

  _dst->append(b ? "true" : "false");
  _last = kTokenValue;

  return *this;
}

JSONBuilder& JSONBuilder::add_int(int64_t n) {
  if (_last == kTokenValue)
    _dst->append(',');

  _dst->append_format("%lld", (long long)n);
  _last = kTokenValue;

  return *this;
}

JSONBuilder& JSONBuilder::add_uint(uint64_t n) {
  if (_last == kTokenValue)
    _dst->append(',');

  _dst->append_format("%llu", (unsigned long long)n);
  _last = kTokenValue;

  return *this;
}

JSONBuilder& JSONBuilder::add_double(double d) {
  if (_last == kTokenValue)
    _dst->append(',');

  _dst->append_format("%g", d);
  _last = kTokenValue;

  return *this;
}

JSONBuilder& JSONBuilder::add_doublef(const char* fmt, double d) {
  if (_last == kTokenValue)
    _dst->append(',');

  _dst->append_format(fmt, d);
  _last = kTokenValue;

  return *this;
}

JSONBuilder& JSONBuilder::add_string(const char* str) {
  if (_last == kTokenValue)
    _dst->append(',');

  _dst->append_format("\"%s\"", str);
  _last = kTokenValue;

  return *this;
}

JSONBuilder& JSONBuilder::add_stringf(const char* fmt, ...) {
  if (_last == kTokenValue)
    _dst->append(',');

  va_list ap;
  va_start(ap, fmt);

  _dst->append('\"');
  _dst->appendFormatV(fmt, ap);
  _dst->append('\"');
  _last = kTokenValue;

  va_end(ap);

  return *this;
}

JSONBuilder& JSONBuilder::add_string_no_quotes(const char* str) {
  if (_last == kTokenValue)
    _dst->append(',');

  _dst->append(str);
  _last = kTokenValue;

  return *this;
}

JSONBuilder& JSONBuilder::align_to(size_t n) {
  size_t i = _dst->size();
  const char* p = _dst->data();

  while (i)
    if (p[--i] == '\n')
      break;

  size_t cur = _dst->size() - i;
  if (cur < n)
    _dst->append(' ', n - cur);

  return *this;
}

JSONBuilder& JSONBuilder::before_record() {
  if (_last == kTokenValue)
    _dst->append(',');

  _dst->append('\n');
  _dst->append(' ', _level * 2);
  _last = kTokenNone;

  return *this;
}

} // {blbench} namespace
