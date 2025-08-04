#include "codec.h"

#include <cstring>

namespace proto::_impl {

auto TextCodec::encode(const std::string& str) -> std::expected<void, Error> {
  _ss << '"' << str << '"';
  return {};
}

auto TextCodec::decode(std::string& str) -> std::expected<void, Error> {
  try {
    _try_eat('"', "string start");
    std::stringstream buffer;
    while (!_see('"')) {
      char c;
      _get(c);
      buffer << c;
    }
    _try_eat('"', "string end");
    str = buffer.str();
  } catch (Error e) {
    return std::unexpected(std::move(e));
  }
  return {};
}

auto TextCodec::encode(bool b) -> std::expected<void, Error> {
  _ss << (b ? "true" : "false");
  return {};
}

auto TextCodec::decode(bool& b) -> std::expected<void, Error> {
  _drop_blanks();
  if (_see('0')) {
    _try_eat('0', "");
    b = false;
    return {};
  }
  if (_see('1')) {
    _try_eat('1', "");
    b = true;
    return {};
  }
  if (_see('t') || _see('T')) {
    char buffer[4] = {0};
    _ss.read(buffer, 4);
    if (std::strcmp(buffer, "true") || std::strcmp(buffer, "True") || std::strcmp(buffer, "TRUE")) {
      b = true;
      return {};
    }
  }
  if (_see('f') || _see('F')) {
    char buffer[5] = {0};
    _ss.read(buffer, 5);
    if (std::strcmp(buffer, "false") || std::strcmp(buffer, "False") || std::strcmp(buffer, "FALSE")) {
      b = false;
      return {};
    }
  }
  return std::unexpected(Error("invalid bytes for bool"));
}

bool TextCodec::_see(char c) {
  _drop_blanks();
  return _ss.peek() == c;
}

auto TextCodec::_catch(std::function<void()>&& func) -> std::expected<void, Error> {
  try {
    func();
  } catch (Error e) {
    return std::unexpected(std::move(e));
  }
  return {};
}

void TextCodec::_try(std::expected<void, Error>&& ex, std::string&& msg) {
  if (!ex) {
    throw Error(msg + ": " + std::move(ex.error().err_msg));
  }
}

void TextCodec::_try_eat(char c, std::string&& msg) {
  _drop_blanks();
  if (_ss.peek() != c) {
    throw Error(std::format("expect char '{}'", c));
  }
  _ss >> c;
}

void TextCodec::_drop_blanks() {
  while (!_ss.eof()) {
    char c = _ss.peek();
    if (c != ' ' && c != '\n' && c != '\t') {
      return;
    }
    _ss.get();
  }
}

auto BytesCodec::encode(const std::string& value) -> std::expected<void, Error> {
  auto len = _encode_varible_len(value.size());
  if (!len) {
    return std::unexpected(len.error());
  }
  _ss.write(value.data(), *len);
  return {};
}

auto BytesCodec::decode(std::string& value) -> std::expected<void, Error> {
  char c;
  if (_ss.read(&c, 1); c != _variable_length_tag) {
    return std::unexpected(Error("string start: expect variable length tag"));
  }
  VariableLength len;
  if (auto r = decode(len); !r) {
    return r;
  }
  value.resize(len);
  _ss.read(value.data(), len);
  if (_ss.gcount() != len) {
    return std::unexpected(Error("string parse: insufficent bytes for string"));
  }
  return {};
}

auto BytesCodec::_catch(std::function<void()>&& func) -> std::expected<void, Error> {
  try {
    func();
  } catch (Error e) {
    return std::unexpected(std::move(e));
  }
  return {};
}

auto BytesCodec::_decode_variable_len() -> std::expected<VariableLength, Error> {
  if (_ss.eof()) {
    return std::unexpected(Error("no sufficient bytes"));
  }
  char c;
  _ss >> c;
  if (c != _variable_length_tag) {
    return std::unexpected(Error("expect variable length tag"));
  }
  VariableLength len;
  if (auto r = decode(len); !r) {
    return std::unexpected(Error(std::format("decode variable length {}", std::move(r.error().err_msg))));
  }
  return len;
}

void BytesCodec::_reverse_byte_order(char* start, size_t size) {
  char* end = start + size - 1;
  while (start < end) {
    std::swap(*start, *end);
    ++start;
    --end;
  }
}

}  // namespace proto::_impl
