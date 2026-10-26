// JsonLite.cpp — see JsonLite.h.  Built without the precompiled header.
#include "JsonLite.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdint>

namespace jsonlite
{

// ═════════════════════════════════════════════════════════════════════════════
//  UTF conversion
// ═════════════════════════════════════════════════════════════════════════════

static void AppendUtf8(std::string& out, uint32_t cp)
{
    if (cp < 0x80) out += (char)cp;
    else if (cp < 0x800) {
        out += (char)(0xC0 | (cp >> 6));
        out += (char)(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += (char)(0xE0 | (cp >> 12));
        out += (char)(0x80 | ((cp >> 6) & 0x3F));
        out += (char)(0x80 | (cp & 0x3F));
    } else {
        out += (char)(0xF0 | (cp >> 18));
        out += (char)(0x80 | ((cp >> 12) & 0x3F));
        out += (char)(0x80 | ((cp >> 6) & 0x3F));
        out += (char)(0x80 | (cp & 0x3F));
    }
}

static void AppendWide(std::wstring& out, uint32_t cp)
{
    if (sizeof(wchar_t) == 2 && cp >= 0x10000) {
        cp -= 0x10000;
        out += (wchar_t)(0xD800 + (cp >> 10));
        out += (wchar_t)(0xDC00 + (cp & 0x3FF));
    } else {
        out += (wchar_t)cp;
    }
}

std::string WideToUtf8(const std::wstring& w)
{
    std::string out;
    out.reserve(w.size());
    for (size_t i = 0; i < w.size(); ++i)
    {
        uint32_t cp = (uint32_t)w[i];
        if (sizeof(wchar_t) == 2 && cp >= 0xD800 && cp <= 0xDBFF && i + 1 < w.size())
        {
            uint32_t lo = (uint32_t)w[i + 1];
            if (lo >= 0xDC00 && lo <= 0xDFFF) {
                cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                ++i;
            }
        }
        if (cp > 0x10FFFF) cp = 0xFFFD;
        AppendUtf8(out, cp);
    }
    return out;
}

std::wstring Utf8ToWide(const std::string& s)
{
    std::wstring out;
    out.reserve(s.size());
    size_t i = 0;
    while (i < s.size())
    {
        unsigned char c = (unsigned char)s[i];
        uint32_t cp; int extra;
        if (c < 0x80)              { cp = c;        extra = 0; }
        else if ((c >> 5) == 0x6)  { cp = c & 0x1F; extra = 1; }
        else if ((c >> 4) == 0xE)  { cp = c & 0x0F; extra = 2; }
        else if ((c >> 3) == 0x1E) { cp = c & 0x07; extra = 3; }
        else { AppendWide(out, 0xFFFD); ++i; continue; }

        bool ok = true;
        for (int k = 1; k <= extra; ++k)
        {
            if (i + k >= s.size()) { ok = false; break; }
            unsigned char cc = (unsigned char)s[i + k];
            if ((cc >> 6) != 0x2) { ok = false; break; }
            cp = (cp << 6) | (cc & 0x3F);
        }
        if (!ok) { AppendWide(out, 0xFFFD); ++i; continue; }
        AppendWide(out, cp);
        i += extra + 1;
    }
    return out;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Value
// ═════════════════════════════════════════════════════════════════════════════

bool Value::AsBool(bool fallback) const
{
    return m_type == Type::Bool ? m_bool : fallback;
}

double Value::AsNumber(double fallback) const
{
    return m_type == Type::Number ? m_num : fallback;
}

std::wstring Value::AsString(const std::wstring& fallback) const
{
    return m_type == Type::String ? m_str : fallback;
}

Value& Value::Append(const Value& v)
{
    if (m_type != Type::Array) { *this = MakeArray(); }
    m_arr.push_back(v);
    return m_arr.back();
}

Value& Value::Set(const std::wstring& key, const Value& v)
{
    if (m_type != Type::Object) { *this = MakeObject(); }
    for (auto& m : m_obj)
        if (m.first == key) { m.second = v; return m.second; }
    m_obj.emplace_back(key, v);
    return m_obj.back().second;
}

const Value* Value::Find(const std::wstring& key) const
{
    if (m_type != Type::Object) return nullptr;
    for (auto& m : m_obj)
        if (m.first == key) return &m.second;
    return nullptr;
}

std::wstring Value::GetString(const std::wstring& key, const std::wstring& fallback) const
{
    const Value* v = Find(key);
    return v ? v->AsString(fallback) : fallback;
}

bool Value::GetBool(const std::wstring& key, bool fallback) const
{
    const Value* v = Find(key);
    return v ? v->AsBool(fallback) : fallback;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Writer
// ═════════════════════════════════════════════════════════════════════════════

static void WriteString(std::string& out, const std::wstring& s)
{
    out += '"';
    std::string u = WideToUtf8(s);
    for (unsigned char c : u)
    {
        switch (c)
        {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b";  break;
        case '\f': out += "\\f";  break;
        case '\n': out += "\\n";  break;
        case '\r': out += "\\r";  break;
        case '\t': out += "\\t";  break;
        default:
            if (c < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "\\u%04x", (unsigned)c);
                out += buf;
            } else out += (char)c;
        }
    }
    out += '"';
}

static void WriteNumber(std::string& out, double d)
{
    if (!std::isfinite(d)) { out += "null"; return; }
    char buf[40];
    if (d == std::floor(d) && std::fabs(d) < 1e15)
        std::snprintf(buf, sizeof(buf), "%.0f", d);
    else
        std::snprintf(buf, sizeof(buf), "%.17g", d);
    out += buf;
}

static void Newline(std::string& out, int indent, int level)
{
    if (indent < 0) return;
    out += '\n';
    out.append((size_t)(indent * level), ' ');
}

static void WriteValue(std::string& out, const Value& v, int indent, int level)
{
    switch (v.Kind())
    {
    case Value::Type::Null:   out += "null"; break;
    case Value::Type::Bool:   out += v.AsBool() ? "true" : "false"; break;
    case Value::Type::Number: WriteNumber(out, v.AsNumber()); break;
    case Value::Type::String: WriteString(out, v.AsString()); break;
    case Value::Type::Array:
    {
        const auto& items = v.Items();
        if (items.empty()) { out += "[]"; break; }
        out += '[';
        for (size_t i = 0; i < items.size(); ++i)
        {
            if (i) out += ',';
            Newline(out, indent, level + 1);
            WriteValue(out, items[i], indent, level + 1);
        }
        Newline(out, indent, level);
        out += ']';
        break;
    }
    case Value::Type::Object:
    {
        const auto& members = v.Members();
        if (members.empty()) { out += "{}"; break; }
        out += '{';
        for (size_t i = 0; i < members.size(); ++i)
        {
            if (i) out += ',';
            Newline(out, indent, level + 1);
            WriteString(out, members[i].first);
            out += indent < 0 ? ":" : ": ";
            WriteValue(out, members[i].second, indent, level + 1);
        }
        Newline(out, indent, level);
        out += '}';
        break;
    }
    }
}

std::string Write(const Value& v, int indent)
{
    std::string out;
    WriteValue(out, v, indent, 0);
    if (indent >= 0) out += '\n';
    return out;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Parser
// ═════════════════════════════════════════════════════════════════════════════

namespace {

class Parser
{
public:
    explicit Parser(const std::string& s) : m_s(s) {}

    bool Run(Value& out, std::wstring* err)
    {
        if (m_s.size() >= 3 && (unsigned char)m_s[0] == 0xEF &&
            (unsigned char)m_s[1] == 0xBB && (unsigned char)m_s[2] == 0xBF)
            m_pos = 3;
        bool ok = ParseValue(out, 0);
        if (ok) {
            SkipWs();
            if (m_pos != m_s.size()) { Fail("unexpected data after the JSON value"); ok = false; }
        }
        if (!ok && err) *err = m_err;
        return ok;
    }

private:
    const std::string& m_s;
    size_t       m_pos = 0;
    std::wstring m_err;

    bool Fail(const char* msg)
    {
        if (m_err.empty())
        {
            size_t line = 1;
            for (size_t i = 0; i < m_pos && i < m_s.size(); ++i)
                if (m_s[i] == '\n') ++line;
            m_err = L"JSON error at line " + std::to_wstring(line) + L": " +
                    Utf8ToWide(msg);
        }
        return false;
    }

    void SkipWs()
    {
        while (m_pos < m_s.size())
        {
            char c = m_s[m_pos];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++m_pos;
            else break;
        }
    }

    bool Literal(const char* word)
    {
        size_t n = 0;
        while (word[n]) ++n;
        if (m_s.compare(m_pos, n, word) != 0) return false;
        m_pos += n;
        return true;
    }

    bool ParseValue(Value& out, int depth)
    {
        if (depth > 200) return Fail("nesting too deep");
        SkipWs();
        if (m_pos >= m_s.size()) return Fail("unexpected end of input");
        char c = m_s[m_pos];
        if (c == '{') return ParseObject(out, depth);
        if (c == '[') return ParseArray(out, depth);
        if (c == '"') { std::wstring s; if (!ParseString(s)) return false; out = Value(s); return true; }
        if (c == 't') { if (Literal("true"))  { out = Value(true);  return true; } return Fail("invalid literal"); }
        if (c == 'f') { if (Literal("false")) { out = Value(false); return true; } return Fail("invalid literal"); }
        if (c == 'n') { if (Literal("null"))  { out = Value();      return true; } return Fail("invalid literal"); }
        if (c == '-' || (c >= '0' && c <= '9')) return ParseNumber(out);
        return Fail("unexpected character");
    }

    bool ParseNumber(Value& out)
    {
        size_t start = m_pos;
        if (m_s[m_pos] == '-') ++m_pos;
        auto digits = [&]() { size_t b = m_pos; while (m_pos < m_s.size() && m_s[m_pos] >= '0' && m_s[m_pos] <= '9') ++m_pos; return m_pos > b; };
        if (!digits()) return Fail("invalid number");
        if (m_pos < m_s.size() && m_s[m_pos] == '.') { ++m_pos; if (!digits()) return Fail("invalid number"); }
        if (m_pos < m_s.size() && (m_s[m_pos] == 'e' || m_s[m_pos] == 'E'))
        {
            ++m_pos;
            if (m_pos < m_s.size() && (m_s[m_pos] == '+' || m_s[m_pos] == '-')) ++m_pos;
            if (!digits()) return Fail("invalid number");
        }
        std::string num = m_s.substr(start, m_pos - start);
        out = Value(std::strtod(num.c_str(), nullptr));
        return true;
    }

    static int Hex(char c)
    {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }

    bool Read4Hex(uint32_t& v)
    {
        if (m_pos + 4 > m_s.size()) return Fail("truncated \\u escape");
        v = 0;
        for (int i = 0; i < 4; ++i)
        {
            int h = Hex(m_s[m_pos + i]);
            if (h < 0) return Fail("invalid \\u escape");
            v = (v << 4) | (uint32_t)h;
        }
        m_pos += 4;
        return true;
    }

    bool ParseString(std::wstring& out)
    {
        ++m_pos;                                  // opening quote
        std::string raw;                          // UTF-8 accumulated
        while (true)
        {
            if (m_pos >= m_s.size()) return Fail("unterminated string");
            char c = m_s[m_pos++];
            if (c == '"') break;
            if ((unsigned char)c < 0x20) return Fail("control character in string");
            if (c != '\\') { raw += c; continue; }

            if (m_pos >= m_s.size()) return Fail("unterminated string");
            char e = m_s[m_pos++];
            switch (e)
            {
            case '"':  raw += '"';  break;
            case '\\': raw += '\\'; break;
            case '/':  raw += '/';  break;
            case 'b':  raw += '\b'; break;
            case 'f':  raw += '\f'; break;
            case 'n':  raw += '\n'; break;
            case 'r':  raw += '\r'; break;
            case 't':  raw += '\t'; break;
            case 'u':
            {
                uint32_t cp;
                if (!Read4Hex(cp)) return false;
                if (cp >= 0xD800 && cp <= 0xDBFF)
                {
                    uint32_t lo = 0;
                    if (m_pos + 1 < m_s.size() && m_s[m_pos] == '\\' && m_s[m_pos + 1] == 'u')
                    {
                        m_pos += 2;
                        if (!Read4Hex(lo)) return false;
                    }
                    if (lo >= 0xDC00 && lo <= 0xDFFF)
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    else
                        cp = 0xFFFD;
                }
                else if (cp >= 0xDC00 && cp <= 0xDFFF)
                    cp = 0xFFFD;
                AppendUtf8(raw, cp);
                break;
            }
            default: return Fail("invalid escape sequence");
            }
        }
        out = Utf8ToWide(raw);
        return true;
    }

    bool ParseArray(Value& out, int depth)
    {
        ++m_pos;
        out = Value::MakeArray();
        SkipWs();
        if (m_pos < m_s.size() && m_s[m_pos] == ']') { ++m_pos; return true; }
        while (true)
        {
            Value item;
            if (!ParseValue(item, depth + 1)) return false;
            out.Append(item);
            SkipWs();
            if (m_pos >= m_s.size()) return Fail("unterminated array");
            char c = m_s[m_pos++];
            if (c == ']') return true;
            if (c != ',') return Fail("expected ',' or ']'");
        }
    }

    bool ParseObject(Value& out, int depth)
    {
        ++m_pos;
        out = Value::MakeObject();
        SkipWs();
        if (m_pos < m_s.size() && m_s[m_pos] == '}') { ++m_pos; return true; }
        while (true)
        {
            SkipWs();
            if (m_pos >= m_s.size() || m_s[m_pos] != '"') return Fail("expected a member name");
            std::wstring key;
            if (!ParseString(key)) return false;
            SkipWs();
            if (m_pos >= m_s.size() || m_s[m_pos] != ':') return Fail("expected ':'");
            ++m_pos;
            Value v;
            if (!ParseValue(v, depth + 1)) return false;
            out.Set(key, v);
            SkipWs();
            if (m_pos >= m_s.size()) return Fail("unterminated object");
            char c = m_s[m_pos++];
            if (c == '}') return true;
            if (c != ',') return Fail("expected ',' or '}'");
        }
    }
};

} // namespace

bool Parse(const std::string& utf8, Value& out, std::wstring* error)
{
    Parser p(utf8);
    Value v;
    if (!p.Run(v, error)) return false;
    out = v;
    return true;
}

} // namespace jsonlite
