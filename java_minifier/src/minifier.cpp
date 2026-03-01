#include "minifier.h"

#include <cctype>
#include <string>
#include <vector>
#include <stdexcept>

static bool isIdentChar(unsigned char c)
{
    return std::isalnum(c) || c == '_' || c == '$';
}

static bool isSpace(unsigned char c)
{
    return std::isspace(c) != 0;
}

static bool isNumberStart(unsigned char c)
{
    return std::isdigit(c) != 0;
}


enum class State
{
    NORMAL,
    IN_STRING,
    IN_CHAR,
    IN_LINE_COMMENT,
    IN_BLOCK_COMMENT
};

static std::string basic_minify_to_string(std::istream& in)
{
    State st = State::NORMAL;

    std::string out;
    out.reserve(1024);

    bool pendingSpace = false;
    bool escape       = false;
    char prevSig      = 0;

    auto maybe_emit_space = [&](char nextSig)
    {
        if (!pendingSpace)
            return;

        unsigned char a = static_cast<unsigned char>(prevSig);
        unsigned char b = static_cast<unsigned char>(nextSig);

        bool left  = isIdentChar(a);
        bool right = isIdentChar(b);

        if (prevSig != 0 && left && right)
            out.push_back(' ');
        pendingSpace = false;
    };

    int ci;
    while ((ci = in.get()) != EOF)
    {
        char c = static_cast<char>(ci);

        switch (st)
        {
            case State::NORMAL:
            {
                if (isSpace((unsigned char)c))
                {
                    pendingSpace = true;
                    break;
                }

                if (c == '/')
                {
                    int pi = in.peek();
                    if (pi == '/')
                    {
                        in.get();
                        st = State::IN_LINE_COMMENT;
                        break;
                    }
                    if (pi == '*')
                    {
                        in.get();
                        st = State::IN_BLOCK_COMMENT;
                        break;
                    }

                    maybe_emit_space('/');
                    out.push_back('/');
                    prevSig = '/';
                    break;
                }

                if (c == '"')
                {
                    maybe_emit_space('"');
                    out.push_back('"');
                    prevSig = '"';
                    st = State::IN_STRING;
                    escape = false;
                    break;
                }

                if (c == '\'')
                {
                    maybe_emit_space('\'');
                    out.push_back('\'');
                    prevSig = '\'';
                    st = State::IN_CHAR;
                    escape = false;
                    break;
                }

                maybe_emit_space(c);
                out.push_back(c);
                if (!isSpace((unsigned char)c))
                    prevSig = c;
                break;
            }

            case State::IN_LINE_COMMENT:
            {
                if (c == '\n' || c == '\r')
                {
                    st = State::NORMAL;
                    pendingSpace = true;
                }
                break;
            }

            case State::IN_BLOCK_COMMENT:
            {
                if (c == '*')
                {
                    int pi = in.peek();
                    if (pi == '/')
                    {
                        in.get();
                        st = State::NORMAL;
                        pendingSpace = true;
                    }
                }
                break;
            }

            case State::IN_STRING:
            {
                out.push_back(c);
                prevSig = c;

                if (escape)
                    escape = false;
                else if (c == '\\')
                    escape = true;
                else if (c == '"')
                    st = State::NORMAL;
                break;
            }

            case State::IN_CHAR:
            {
                out.push_back(c);
                prevSig = c;

                if (escape)
                    escape = false;
                else if (c == '\\')
                    escape = true;
                else if (c == '\'')
                    st = State::NORMAL;
                break;
            }
        }
    }

    return out;
}

enum class TokKind
{
    IDENT,
    NUMBER,
    STRING,
    CHAR,
    SYMBOL
};

struct Tok
{
    TokKind     kind;
    std::string text;
};

static bool is_keyword(const std::string& s)
{
    return s == "if" || s == "else" || s == "return";
}

static std::vector<Tok> tokenize_minified_java(const std::string& s)
{
    std::vector<Tok> toks;
    toks.reserve(s.size() / 2);

    size_t i = 0;
    while (i < s.size())
    {
        unsigned char c = (unsigned char)s[i];

        if (isSpace(c))
        {
            ++i;
            continue;
        }

        if (s[i] == '"')
        {
            size_t j = i + 1;
            bool esc = false;
            while (j < s.size())
            {
                char ch = s[j++];
                if (esc)
                    { esc = false; continue; }
                if (ch == '\\')
                    { esc = true; continue; }
                if (ch == '"')
                    break;
            }

            toks.push_back({TokKind::STRING, s.substr(i, j - i)});
            i = j;
            continue;
        }

        if (s[i] == '\'')
        {
            size_t j = i + 1;
            bool esc = false;
            while (j < s.size())
            {
                char ch = s[j++];
                if (esc)
                    { esc = false; continue; }
                if (ch == '\\')
                    { esc = true; continue; }
                if (ch == '\'')
                    break;
            }

            toks.push_back({TokKind::CHAR, s.substr(i, j - i)});
            i = j;
            continue;
        }

        if (isIdentChar(c) && !isNumberStart(c))
        {
            size_t j = i + 1;
            while (j < s.size() && isIdentChar((unsigned char)s[j])) j++;
            toks.push_back({TokKind::IDENT, s.substr(i, j - i)});
            i = j;
            continue;
        }

        if (isNumberStart(c))
        {
            size_t j = i + 1;
            while (j < s.size())
            {
                unsigned char cj = (unsigned char)s[j];
                if (std::isalnum(cj) || cj == '.' || cj == '_' )
                    j++;
                else
                    break;
            }

            toks.push_back({TokKind::NUMBER, s.substr(i, j - i)});
            i = j;

            continue;
        }

        toks.push_back({TokKind::SYMBOL, std::string(1, s[i])});
        i++;
    }

    return toks;
}

static bool token_is_return(const Tok& t)
{
    return t.kind == TokKind::IDENT && t.text == "return";
}

static bool token_is_if(const Tok& t)
{
    return t.kind == TokKind::IDENT && t.text == "if";
}

static bool token_is_else(const Tok& t)
{
    return t.kind == TokKind::IDENT && t.text == "else";
}

static bool token_is_symbol(const Tok& t, const char* sym)
{
    return t.kind == TokKind::SYMBOL && t.text == sym;
}

static bool collect_paren_group(const std::vector<Tok>& toks, size_t& i, std::vector<Tok>& group)
{
    if (i >= toks.size() || !token_is_symbol(toks[i], "("))
        return false;

    int    depth = 0;
    size_t start = i;

    while (i < toks.size())
    {
        const Tok& t = toks[i];
        if (t.kind == TokKind::SYMBOL && t.text == "(")
            depth++;
        if (t.kind == TokKind::SYMBOL && t.text == ")")
            depth--;

        group.push_back(t);
        i++;

        if (depth == 0)
            return true;
        if (depth < 0)
            return false;
    }

    i = start;
    group.clear();
    return false;
}

static bool collect_expr_until_semicolon(const std::vector<Tok>& toks, size_t& i, std::vector<Tok>& expr)
{
    int p = 0, b = 0, c = 0;
    size_t start = i;

    while (i < toks.size())
    {
        const Tok& t = toks[i];

        if (t.kind == TokKind::SYMBOL)
        {
            const std::string& s = t.text;
            if      (s == "(") p++;
            else if (s == ")") p--;
            else if (s == "[") b++;
            else if (s == "]") b--;
            else if (s == "{") c++;
            else if (s == "}") c--;
            else if (s == ";" && p == 0 && b == 0 && c == 0)
            {
                return true;
            }

            if ((s == "{" || s == "}") && p == 0 && b == 0)
            {
                i = start;
                expr.clear();
                return false;
            }

            if (p < 0 || b < 0 || c < 0)
            {
                i = start;
                expr.clear();
                return false;
            }
        }

        expr.push_back(t);
        i++;
    }

    i = start;
    expr.clear();
    return false;
}

static std::string join_tokens_minimal(const std::vector<Tok>& toks)
{
    std::string out;
    out.reserve(toks.size() * 2);

    auto is_wordlike = [](const Tok& t)
    {
        return t.kind == TokKind::IDENT || t.kind == TokKind::NUMBER;
    };

    for (size_t i = 0; i < toks.size(); i++)
    {
        if (i > 0) {
            if (is_wordlike(toks[i - 1]) && is_wordlike(toks[i]))
            {
                out.push_back(' ');
            }
        }
        out += toks[i].text;
    }
    return out;
}

static std::string aggressive_transform_if_return_else_return(const std::string& minified)
{
    auto toks = tokenize_minified_java(minified);
    std::vector<Tok> out;
    out.reserve(toks.size());

    size_t i = 0;
    while (i < toks.size())
    {
        size_t j = i;

        if (j < toks.size() && token_is_if(toks[j]))
        {
            j++;

            std::vector<Tok> condGroup;
            if (j < toks.size() && collect_paren_group(toks, j, condGroup))
            {

                // expect: return
                if (j < toks.size() && token_is_return(toks[j]))
                {
                    j++;

                    std::vector<Tok> exprA;
                    if (collect_expr_until_semicolon(toks, j, exprA))
                    {
                        // expect ';'
                        if (j < toks.size() && token_is_symbol(toks[j], ";"))
                        {
                            j++;

                            // expect else
                            if (j < toks.size() && token_is_else(toks[j]))
                            {
                                j++;

                                // expect return
                                if (j < toks.size() && token_is_return(toks[j]))
                                {
                                    j++;

                                    std::vector<Tok> exprB;
                                    if (collect_expr_until_semicolon(toks, j, exprB))
                                    {
                                        // expect ';'
                                        if (j < toks.size() && token_is_symbol(toks[j], ";"))
                                        {
                                            j++;

                                            out.push_back({TokKind::IDENT, "return"});
                                            for (auto& t : condGroup) out.push_back(t);
                                            out.push_back({TokKind::SYMBOL, "?"});
                                            for (auto& t : exprA) out.push_back(t);
                                            out.push_back({TokKind::SYMBOL, ":"});
                                            for (auto& t : exprB) out.push_back(t);
                                            out.push_back({TokKind::SYMBOL, ";"});

                                            i = j;
                                            continue;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        out.push_back(toks[i]);
        i++;
    }

    return join_tokens_minimal(out);
}

void minify(std::istream& in, std::ostream& out, const MinifyOptions& opts)
{
    std::string s = basic_minify_to_string(in);

    if (opts.aggressive)
        s = aggressive_transform_if_return_else_return(s);

    out << s;
}