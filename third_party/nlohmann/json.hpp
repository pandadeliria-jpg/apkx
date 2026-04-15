#pragma once

// Tiny JSON (registry-only) - minimal, self-contained.
// Supports: object, array, string, number(int/double), bool, null.
// Enough to parse and write ~/.android_compat/apps/apps.json produced by our tooling.

#include <cctype>
#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace nlohmann {

class json {
 public:
  using object_t = std::map<std::string, json>;
  using array_t = std::vector<json>;
  using string_t = std::string;
  using number_integer_t = long long;
  using number_float_t = double;
  using boolean_t = bool;
  struct null_t {};

  using value_t = std::variant<null_t, object_t, array_t, string_t, number_integer_t, number_float_t, boolean_t>;

  json() : v_(null_t{}) {}
  static json object() { return json(object_t{}); }
  static json array() { return json(array_t{}); }

  json(std::nullptr_t) : v_(null_t{}) {}
  json(const object_t& o) : v_(o) {}
  json(object_t&& o) : v_(std::move(o)) {}
  json(const array_t& a) : v_(a) {}
  json(array_t&& a) : v_(std::move(a)) {}
  json(const string_t& s) : v_(s) {}
  json(string_t&& s) : v_(std::move(s)) {}
  json(const char* s) : v_(string_t(s)) {}
  json(number_integer_t i) : v_(i) {}
  json(number_float_t d) : v_(d) {}
  json(boolean_t b) : v_(b) {}

  bool is_object() const { return std::holds_alternative<object_t>(v_); }
  bool is_array() const { return std::holds_alternative<array_t>(v_); }
  bool is_string() const { return std::holds_alternative<string_t>(v_); }
  bool is_number_integer() const { return std::holds_alternative<number_integer_t>(v_); }
  bool is_number_float() const { return std::holds_alternative<number_float_t>(v_); }
  bool is_boolean() const { return std::holds_alternative<boolean_t>(v_); }
  bool is_null() const { return std::holds_alternative<null_t>(v_); }

  object_t& get_object() { return std::get<object_t>(v_); }
  const object_t& get_object() const { return std::get<object_t>(v_); }
  array_t& get_array() { return std::get<array_t>(v_); }
  const array_t& get_array() const { return std::get<array_t>(v_); }
  string_t& get_string() { return std::get<string_t>(v_); }
  const string_t& get_string() const { return std::get<string_t>(v_); }

  bool contains(const std::string& k) const {
    if (!is_object()) return false;
    return get_object().find(k) != get_object().end();
  }

  json& operator[](const std::string& k) {
    if (!is_object()) v_ = object_t{};
    return get_object()[k];
  }
  const json& operator[](const std::string& k) const {
    return get_object().at(k);
  }

  // Iteration
  struct iterator {
    object_t::iterator it;
    object_t::iterator end;
    bool operator!=(const iterator& o) const { return it != o.it; }
    void operator++() { ++it; }
    std::pair<const std::string, json>& operator*() const { return const_cast<std::pair<const std::string, json>&>(*it); }
  };

  struct const_iterator {
    object_t::const_iterator it;
    object_t::const_iterator end;
    bool operator!=(const const_iterator& o) const { return it != o.it; }
    void operator++() { ++it; }
    const std::pair<const std::string, json>& operator*() const { return *it; }
  };

  iterator begin() {
    if (!is_object()) throw std::runtime_error("not object");
    return iterator{get_object().begin(), get_object().end()};
  }
  iterator end() {
    if (!is_object()) throw std::runtime_error("not object");
    return iterator{get_object().end(), get_object().end()};
  }
  const_iterator begin() const {
    if (!is_object()) throw std::runtime_error("not object");
    return const_iterator{get_object().begin(), get_object().end()};
  }
  const_iterator end() const {
    if (!is_object()) throw std::runtime_error("not object");
    return const_iterator{get_object().end(), get_object().end()};
  }

  // value helpers
  template <class T>
  T value(const std::string& k, const T& def) const {
    if (!is_object()) return def;
    auto it = get_object().find(k);
    if (it == get_object().end()) return def;
    return it->second.get<T>(def);
  }

  template <class T>
  T get() const {
    if constexpr (std::is_same_v<T, std::string>) {
      if (!is_string()) throw std::runtime_error("not string");
      return get_string();
    } else if constexpr (std::is_same_v<T, bool>) {
      if (!is_boolean()) throw std::runtime_error("not bool");
      return std::get<boolean_t>(v_);
    } else if constexpr (std::is_same_v<T, int>) {
      if (is_number_integer()) return (int)std::get<number_integer_t>(v_);
      if (is_number_float()) return (int)std::get<number_float_t>(v_);
      throw std::runtime_error("not number");
    } else if constexpr (std::is_same_v<T, long long>) {
      if (is_number_integer()) return std::get<number_integer_t>(v_);
      if (is_number_float()) return (long long)std::get<number_float_t>(v_);
      throw std::runtime_error("not number");
    } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
      if (!is_array()) throw std::runtime_error("not array");
      std::vector<std::string> out;
      for (auto& e : get_array()) out.push_back(e.get<std::string>());
      return out;
    } else {
      static_assert(sizeof(T) == 0, "unsupported get<T>");
    }
  }

  template <class T>
  T get(const T& def) const {
    try {
      return get<T>();
    } catch (...) {
      return def;
    }
  }

  std::string dump(int indent = -1) const {
    (void)indent;
    std::string out;
    dump_into(out);
    return out;
  }

  static json parse(const std::string& s) {
    size_t i = 0;
    json j = parse_value(s, i);
    skip_ws(s, i);
    if (i != s.size()) throw std::runtime_error("trailing json");
    return j;
  }

 private:
  value_t v_;

  void dump_into(std::string& out) const {
    if (is_null()) { out += "null"; return; }
    if (is_boolean()) { out += (std::get<boolean_t>(v_) ? "true" : "false"); return; }
    if (is_string()) {
      out += '"';
      for (char c : get_string()) {
        if (c == '"' || c == '\\') { out += '\\'; out += c; }
        else if (c == '\n') out += "\\n";
        else out += c;
      }
      out += '"';
      return;
    }
    if (is_number_integer()) { out += std::to_string(std::get<number_integer_t>(v_)); return; }
    if (is_number_float()) { out += std::to_string(std::get<number_float_t>(v_)); return; }
    if (is_array()) {
      out += "[";
      bool first = true;
      for (auto& e : get_array()) {
        if (!first) out += ",";
        first = false;
        e.dump_into(out);
      }
      out += "]";
      return;
    }
    if (is_object()) {
      out += "{";
      bool first = true;
      for (auto& [k, v] : get_object()) {
        if (!first) out += ",";
        first = false;
        out += '"' + k + '"' + ":";
        v.dump_into(out);
      }
      out += "}";
      return;
    }
  }

  static void skip_ws(const std::string& s, size_t& i) {
    while (i < s.size() && std::isspace((unsigned char)s[i])) i++;
  }

  static json parse_value(const std::string& s, size_t& i) {
    skip_ws(s, i);
    if (i >= s.size()) throw std::runtime_error("eof");
    char c = s[i];
    if (c == '{') return parse_object(s, i);
    if (c == '[') return parse_array(s, i);
    if (c == '"') return json(parse_string(s, i));
    if (c == 't' && s.substr(i, 4) == "true") { i += 4; return json(true); }
    if (c == 'f' && s.substr(i, 5) == "false") { i += 5; return json(false); }
    if (c == 'n' && s.substr(i, 4) == "null") { i += 4; return json(nullptr); }
    return parse_number(s, i);
  }

  static json parse_object(const std::string& s, size_t& i) {
    if (s[i] != '{') throw std::runtime_error("expected {");
    i++;
    json obj = json::object();
    skip_ws(s, i);
    if (i < s.size() && s[i] == '}') { i++; return obj; }
    while (true) {
      skip_ws(s, i);
      std::string key = parse_string(s, i);
      skip_ws(s, i);
      if (s[i] != ':') throw std::runtime_error("expected :");
      i++;
      json val = parse_value(s, i);
      obj[key] = val;
      skip_ws(s, i);
      if (s[i] == '}') { i++; break; }
      if (s[i] != ',') throw std::runtime_error("expected ,");
      i++;
    }
    return obj;
  }

  static json parse_array(const std::string& s, size_t& i) {
    if (s[i] != '[') throw std::runtime_error("expected [");
    i++;
    json arr = json::array();
    skip_ws(s, i);
    if (i < s.size() && s[i] == ']') { i++; return arr; }
    while (true) {
      json val = parse_value(s, i);
      arr.get_array().push_back(val);
      skip_ws(s, i);
      if (s[i] == ']') { i++; break; }
      if (s[i] != ',') throw std::runtime_error("expected ,");
      i++;
    }
    return arr;
  }

  static std::string parse_string(const std::string& s, size_t& i) {
    if (s[i] != '"') throw std::runtime_error("expected string");
    i++;
    std::string out;
    while (i < s.size()) {
      char c = s[i++];
      if (c == '"') break;
      if (c == '\\') {
        if (i >= s.size()) throw std::runtime_error("bad escape");
        char e = s[i++];
        if (e == 'n') out.push_back('\n');
        else out.push_back(e);
      } else {
        out.push_back(c);
      }
    }
    return out;
  }

  static json parse_number(const std::string& s, size_t& i) {
    size_t start = i;
    if (s[i] == '-') i++;
    while (i < s.size() && std::isdigit((unsigned char)s[i])) i++;
    bool is_float = false;
    if (i < s.size() && s[i] == '.') {
      is_float = true;
      i++;
      while (i < s.size() && std::isdigit((unsigned char)s[i])) i++;
    }
    auto token = s.substr(start, i - start);
    if (is_float) return json(std::stod(token));
    return json((number_integer_t)std::stoll(token));
  }
};

} // namespace nlohmann
