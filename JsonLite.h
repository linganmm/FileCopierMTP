#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  JsonLite — a small, dependency-free JSON value / reader / writer.
//
//  • Strings are std::wstring in memory and UTF-8 on disk.
//  • Object members keep their insertion order, so files written by the
//    program stay readable and diff-friendly.
//  • No MFC / Windows dependency (compiled without the precompiled header).
// ─────────────────────────────────────────────────────────────────────────────
#include <string>
#include <vector>
#include <utility>

namespace jsonlite
{

class Value
{
public:
    enum class Type { Null, Bool, Number, String, Array, Object };
    using Member = std::pair<std::wstring, Value>;

    Value() = default;
    Value(bool b)                : m_type(Type::Bool),   m_bool(b) {}
    Value(int n)                 : m_type(Type::Number), m_num(n) {}
    Value(double d)              : m_type(Type::Number), m_num(d) {}
    Value(const wchar_t* s)      : m_type(Type::String), m_str(s ? s : L"") {}
    Value(const std::wstring& s) : m_type(Type::String), m_str(s) {}

    static Value MakeArray()  { Value v; v.m_type = Type::Array;  return v; }
    static Value MakeObject() { Value v; v.m_type = Type::Object; return v; }

    Type Kind() const     { return m_type; }
    bool IsNull() const   { return m_type == Type::Null; }
    bool IsBool() const   { return m_type == Type::Bool; }
    bool IsNumber() const { return m_type == Type::Number; }
    bool IsString() const { return m_type == Type::String; }
    bool IsArray() const  { return m_type == Type::Array; }
    bool IsObject() const { return m_type == Type::Object; }

    // Tolerant accessors: return the fallback when the type does not match.
    bool         AsBool(bool fallback = false) const;
    double       AsNumber(double fallback = 0) const;
    std::wstring AsString(const std::wstring& fallback = L"") const;

    // Arrays
    const std::vector<Value>& Items() const { return m_arr; }
    Value& Append(const Value& v);

    // Objects
    const std::vector<Member>& Members() const { return m_obj; }
    Value& Set(const std::wstring& key, const Value& v);   // replaces existing key
    const Value* Find(const std::wstring& key) const;      // nullptr if absent
    // Convenience: member string / bool, with fallback when absent or mistyped
    std::wstring GetString(const std::wstring& key, const std::wstring& fallback = L"") const;
    bool         GetBool(const std::wstring& key, bool fallback = false) const;

private:
    Type                m_type = Type::Null;
    bool                m_bool = false;
    double              m_num  = 0;
    std::wstring        m_str;
    std::vector<Value>  m_arr;
    std::vector<Member> m_obj;
};

// Serialize to UTF-8 text.  indent < 0 → compact, otherwise pretty-printed
// with that many spaces per level.
std::string Write(const Value& v, int indent = 2);

// Parse UTF-8 text (an optional UTF-8 BOM is accepted).  On failure returns
// false and puts a message with the line number into *error.
bool Parse(const std::string& utf8, Value& out, std::wstring* error = nullptr);

// UTF-8 <-> wide conversion (UTF-16 or UTF-32 wchar_t, decided at compile time)
std::string  WideToUtf8(const std::wstring& w);
std::wstring Utf8ToWide(const std::string& s);

} // namespace jsonlite
