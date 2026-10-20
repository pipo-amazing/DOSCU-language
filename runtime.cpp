
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

// ============================================================
// DOSCU Runtime — C++17
// เปลี่ยนชื่อไฟล์ DOSCU หลักได้ที่บรรทัดนี้
// ============================================================
const std::string mainFile = 
"main.doscu"
;

namespace fs = std::filesystem;
struct Value {
    using Data = std::variant<
        std::monostate, double, bool, std::string
    >;

    Data data;

    Value() : data(std::monostate{}) {}
    Value(double v) : data(v) {}
    Value(int v) : data(static_cast<double>(v)) {}
    Value(bool v) : data(v) {}
    Value(const std::string& v) : data(v) {}
    Value(const char* v) : data(std::string(v)) {}

    double number() const {
        if (auto p = std::get_if<double>(&data)) return *p;
        if (auto p = std::get_if<bool>(&data)) return *p ? 1.0 : 0.0;
        if (auto p = std::get_if<std::string>(&data)) {
            try { return std::stod(*p); }
            catch (...) { return 0.0; }
        }
        return 0.0;
    }

    bool truth() const {
        if (auto p = std::get_if<bool>(&data)) return *p;
        if (auto p = std::get_if<double>(&data)) return *p != 0.0;
        if (auto p = std::get_if<std::string>(&data)) return !p->empty();
        return false;
    }

    std::string str() const {
        if (std::holds_alternative<std::monostate>(data)) return "null";
        if (auto p = std::get_if<double>(&data)) {
            std::ostringstream out;
            out << *p;
            return out.str();
        }
        if (auto p = std::get_if<bool>(&data)) return *p ? "true" : "false";
        return std::get<std::string>(data);
    }
};

struct Token {
    std::string text;
    int line = 1;
    bool spacedBefore = false;
};

class Lexer {
    std::string src;
    size_t pos = 0;
    int line = 1;
    bool hadSpace = false;

    char peek(size_t n = 0) const {
        return pos + n < src.size() ? src[pos + n] : '\0';
    }

    char take() {
        char c = peek();
        if (c) {
            ++pos;
            if (c == '\n') ++line;
        }
        return c;
    }

    static bool identStart(char c) {
        return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
    }

    static bool identPart(char c) {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
    }

public:
    explicit Lexer(std::string s) : src(std::move(s)) {}

    std::vector<Token> scan() {
        std::vector<Token> out;

        auto add = [&](std::string s, int ln) {
            out.push_back({std::move(s), ln, hadSpace});
            hadSpace = false;
        };

        while (peek()) {
            char c = peek();

            if (std::isspace(static_cast<unsigned char>(c))) {
                hadSpace = true;
                take();
                continue;
            }

            int ln = line;

            // #head, #part, #include และ directive อื่น ๆ
            if (c == '#') {
                std::string s;
                s += take();
                while (identPart(peek())) s += take();
                add(s, ln);
                continue;
            }

            // คอมเมนต์ // และ /* ... */
            if (c == '/' && peek(1) == '/') {
                while (peek() && peek() != '\n') take();
                hadSpace = true;
                continue;
            }
            if (c == '/' && peek(1) == '*') {
                take(); take();
                while (peek() && !(peek() == '*' && peek(1) == '/')) take();
                if (peek()) { take(); take(); }
                hadSpace = true;
                continue;
            }

            // String
            if (c == '"' || c == '\'') {
                char quote = take();
                std::string s;
                while (peek() && peek() != quote) {
                    char ch = take();
                    if (ch == '\\' && peek()) {
                        char e = take();
                        switch (e) {
                            case 'n': s += '\n'; break;
                            case 't': s += '\t'; break;
                            case 'r': s += '\r'; break;
                            default: s += e; break;
                        }
                    } else s += ch;
                }
                if (peek() == quote) take();
                add("\"" + s + "\"", ln);
                continue;
            }

            // Number; 78% ที่ติดกันหมายถึง 0.78
            if (std::isdigit(static_cast<unsigned char>(c)) ||
                (c == '.' && std::isdigit(static_cast<unsigned char>(peek(1))))) {
                std::string s;
                while (std::isdigit(static_cast<unsigned char>(peek())) ||
                       peek() == '.') s += take();

                if (peek() == '%') {
                    take();
                    try {
                        double n = std::stod(s) / 100.0;
                        s = std::to_string(n);
                    } catch (...) {}
                }
                add(s, ln);
                continue;
            }

            // Identifier รวมชื่อแบบ out.push, bit.and, log.2
            if (identStart(c)) {
                std::string s;
                while (identPart(peek())) s += take();

                while (peek() == '.' && identStart(peek(1))) {
                    s += take();
                    while (identPart(peek())) s += take();
                }

                // log.2 เป็นชื่อ builtin ที่มีเลขตามหลังจุด
                if (s.rfind("log.", 0) == 0 &&
                    std::isdigit(static_cast<unsigned char>(peek()))) {
                    s += take();
                }

                add(s, ln);
                continue;
            }

            // Multi-character operators
            const std::vector<std::string> ops = {
                "<<", ">>", "==", "!=", "<=", ">=", "&&", "||",
                "++", "--", "+=", "-=", "*=", "/=", "^?", "^/"
            };

            bool matched = false;
            for (const auto& op : ops) {
                if (src.compare(pos, op.size(), op) == 0) {
                    for (size_t i = 0; i < op.size(); ++i) take();
                    add(op, ln);
                    matched = true;
                    break;
                }
            }
            if (matched) continue;

            add(std::string(1, take()), ln);
        }

        out.push_back({"<eof>", line, false});
        return out;
    }
};

struct Expr;
struct Stmt;
using E = std::shared_ptr<Expr>;
using S = std::shared_ptr<Stmt>;

struct Expr {
    enum Kind { Literal, Variable, Unary, Binary, Call } kind;
    Value value;
    std::string text;
    E left, right;
    std::vector<E> args;

    static E literal(Value v) {
        auto e = std::make_shared<Expr>();
        e->kind = Literal; e->value = std::move(v);
        return e;
    }
};

struct Stmt {
    enum Kind {
        Block, Declaration, Assignment, Expression, If, While,
        Return, Break, Continue, Output, Wait, Empty
    } kind = Empty;

    std::string name;
    std::string type;
    E expr;
    E second;
    std::vector<E> exprs;
    std::vector<S> body;
    S otherwise;
};

class Parser {
    std::vector<Token> t;
    size_t p = 0;

    const Token& cur() const { return t.at(std::min(p, t.size() - 1)); }
    std::string peek() const { return cur().text; }
    std::string take() { return t.at(p++).text; }

    bool accept(const std::string& s) {
        if (peek() == s) { ++p; return true; }
        return false;
    }

    void expect(const std::string& s) {
        if (!accept(s))
            throw std::runtime_error("Line " + std::to_string(cur().line) +
                                     ": expected '" + s + "', got '" + peek() + "'");
    }

    static int precedence(const std::string& op) {
        if (op == "||" || op == "or") return 1;
        if (op == "xor") return 2;
        if (op == "&&" || op == "and") return 3;
        if (op == "==" || op == "!=" || op == "~") return 4;
        if (op == "<" || op == ">" || op == "<=" || op == ">=") return 5;
        if (op == "+" || op == "-") return 6;
        if (op == "*" || op == "/" || op == "%") return 7;
        if (op == "^" || op == "^?" || op == "^/") return 8;
        return 0;
    }

    E primary() {
        std::string s = peek();

        if (accept("(")) {
            E e = expression();
            expect(")");
            return e;
        }

        if (accept("!")) {
            auto e = std::make_shared<Expr>();
            e->kind = Expr::Unary; e->text = "!"; e->left = primary();
            return e;
        }

        if (accept("-") || accept("+")) {
            auto e = std::make_shared<Expr>();
            e->kind = Expr::Unary; e->text = s; e->left = primary();
            return e;
        }

        if (accept("|")) {
            E inner = expression();
            expect("|");
            auto e = std::make_shared<Expr>();
            e->kind = Expr::Call; e->text = "abs"; e->args.push_back(inner);
            return e;
        }

        if (s.size() >= 2 && s.front() == '"') {
            take();
            return Expr::literal(Value(s.substr(1, s.size() - 2)));
        }

        if (s == "true" || s == "false") {
            take();
            return Expr::literal(Value(s == "true"));
        }

        if (s == "<eof>")
            throw std::runtime_error("Unexpected end of expression");

        if (std::isdigit(static_cast<unsigned char>(s[0])) ||
            (s.size() > 1 && s[0] == '.' &&
             std::isdigit(static_cast<unsigned char>(s[1])))) {
            take();
            return Expr::literal(Value(std::stod(s)));
        }

        if (std::isalpha(static_cast<unsigned char>(s[0])) || s[0] == '_') {
            take();

            if (accept("(")) {
                auto e = std::make_shared<Expr>();
                e->kind = Expr::Call;
                e->text = s;

                if (!accept(")")) {
                    do { e->args.push_back(expression()); }
                    while (accept(","));
                    expect(")");
                }
                return e;
            }

            auto e = std::make_shared<Expr>();
            e->kind = Expr::Variable;
            e->text = s;
            return e;
        }

        throw std::runtime_error("Unexpected token: " + s);
    }

    E expression(int minPrec = 1) {
        E left = primary();

        // Postfix factorial
        while (accept("!")) {
            auto e = std::make_shared<Expr>();
            e->kind = Expr::Unary; e->text = "post!"; e->left = left;
            left = e;
        }

        while (true) {
            std::string op = peek();
            int prec = precedence(op);
            if (prec < minPrec) break;
            take();
            E right = expression(prec + ((op == "^") ? 0 : 1));

            auto e = std::make_shared<Expr>();
            e->kind = Expr::Binary;
            e->text = op;
            e->left = left;
            e->right = right;
            left = e;
        }

        return left;
    }

public:
    explicit Parser(std::vector<Token> tokens) : t(std::move(tokens)) {}

    S statement() {
        if (accept(";")) return std::make_shared<Stmt>();

        if (accept("{")) {
            auto s = std::make_shared<Stmt>();
            s->kind = Stmt::Block;
            while (peek() != "}" && peek() != "<eof>")
                s->body.push_back(statement());
            expect("}");
            return s;
        }

        if (accept("if")) {
            auto s = std::make_shared<Stmt>();
            s->kind = Stmt::If;
            expect("("); s->expr = expression(); expect(")");
            s->body.push_back(statement());
            if (accept("else")) s->otherwise = statement();
            return s;
        }

        if (accept("while")) {
            auto s = std::make_shared<Stmt>();
            s->kind = Stmt::While;
            expect("("); s->expr = expression(); expect(")");
            s->body.push_back(statement());
            return s;
        }

        if (accept("return")) {
            auto s = std::make_shared<Stmt>();
            s->kind = Stmt::Return;
            if (peek() != ";") s->expr = expression();
            expect(";");
            return s;
        }

        if (accept("break")) {
            auto s = std::make_shared<Stmt>();
            s->kind = Stmt::Break;
            expect(";");
            return s;
        }

        if (accept("continue")) {
            auto s = std::make_shared<Stmt>();
            s->kind = Stmt::Continue;
            expect(";");
            return s;
        }

        if (accept("wait")) {
            auto s = std::make_shared<Stmt>();
            s->kind = Stmt::Wait;
            expect("("); s->expr = expression(); expect(")"); expect(";");
            return s;
        }

        // cout << expr << expr << endl;
        if (accept("cout")) {
            auto s = std::make_shared<Stmt>();
            s->kind = Stmt::Output;
            expect("<<");
            while (true) {
                if (accept("endl")) {
                    s->name = "endl";
                    break;
                }
                s->exprs.push_back(expression());
                if (!accept("<<")) break;
            }
            expect(";");
            return s;
        }

        // Type declarations
        static const std::vector<std::string> types = {
            "int", "float", "double", "bool", "string", "auto", "char"
        };

        if (std::find(types.begin(), types.end(), peek()) != types.end()) {
            auto s = std::make_shared<Stmt>();
            s->kind = Stmt::Declaration;
            s->type = take();
            s->name = take();
            if (accept("=")) s->expr = expression();
            expect(";");
            return s;
        }

        std::string name = take();

        if (accept("=")) {
            auto s = std::make_shared<Stmt>();
            s->kind = Stmt::Assignment;
            s->name = name;
            s->expr = expression();
            expect(";");
            return s;
        }

        if (accept("+=") || accept("-=") || accept("*=") || accept("/=")) {
            std::string op = t[p - 1].text;
            auto s = std::make_shared<Stmt>();
            s->kind = Stmt::Assignment;
            s->name = name;
            auto lhs = std::make_shared<Expr>();
            lhs->kind = Expr::Variable; lhs->text = name;
            auto bin = std::make_shared<Expr>();
            bin->kind = Expr::Binary;
            bin->text = op.substr(0, 1);
            bin->left = lhs; bin->right = expression();
            s->expr = bin;
            expect(";");
            return s;
        }

        if (accept("(")) {
            auto e = std::make_shared<Expr>();
            e->kind = Expr::Call; e->text = name;
            if (!accept(")")) {
                do { e->args.push_back(expression()); }
                while (accept(","));
                expect(")");
            }
            expect(";");
            auto s = std::make_shared<Stmt>();
            s->kind = Stmt::Expression; s->expr = e;
            return s;
        }

        throw std::runtime_error("Unknown statement starting with: " + name);
    }

    std::vector<S> parseBody() {
        std::vector<S> out;
        while (peek() != "<eof>") out.push_back(statement());
        return out;
    }
};

struct Function {
    std::vector<std::string> params;
    std::vector<S> body;
};

struct Part {
    std::string name;
    bool enabled = true;
    bool done = false;
    std::vector<S> code;
    size_t pc = 0;
    std::map<std::string, Value> locals;
};

struct ControlFlow {
    enum Kind { Return, Break, Continue } kind;
    Value value;
};

class Runtime {
    std::map<std::string, Value> globals;
    std::map<std::string, Function> functions;
    std::vector<Part> parts;
    std::map<fs::path, fs::file_time_type> watchedFiles;
    bool running = true;

    static std::string readFile(const fs::path& path) {
        std::ifstream f(path);
        if (!f) throw std::runtime_error("Cannot open file: " + path.string());
        std::ostringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }

    static std::string trim(std::string s) {
        auto notSpace = [](unsigned char c) { return !std::isspace(c); };
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
        s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
        return s;
    }

    std::string loadIncludes(const fs::path& path,
                             std::vector<fs::path>& stack) {
        fs::path full = fs::absolute(path).lexically_normal();

        if (std::find(stack.begin(), stack.end(), full) != stack.end())
            throw std::runtime_error("Circular #include: " + full.string());

        stack.push_back(full);
        watchedFiles[full] = fs::last_write_time(full);

        std::string source = readFile(full);
        std::istringstream in(source);
        std::ostringstream out;
        std::string line;

        while (std::getline(in, line)) {
            std::string stripped = trim(line);

            if (stripped.rfind("#include", 0) == 0) {
                size_t a = stripped.find('"');
                size_t b = stripped.find('"', a == std::string::npos ? a : a + 1);

                if (a == std::string::npos || b == std::string::npos)
                    throw std::runtime_error("Invalid #include in " + full.string());

                fs::path child = full.parent_path() /
                    stripped.substr(a + 1, b - a - 1);

                std::string included = loadIncludes(child, stack);

                // Included files may declare functions but not parts.
                if (included.find("#part") != std::string::npos)
                    throw std::runtime_error("#include files cannot contain #part");

                out << included << "\n";
            } else {
                out << line << "\n";
            }
        }

        stack.pop_back();
        return out.str();
    }

    Value eval(const E& e, Part* part = nullptr) {
        if (!e) return {};

        if (e->kind == Expr::Literal) return e->value;

        if (e->kind == Expr::Variable) {
            if (part) {
                auto it = part->locals.find(e->text);
                if (it != part->locals.end()) return it->second;
            }
            auto it = globals.find(e->text);
            if (it != globals.end()) return it->second;

            if (e->text == "pi") return Value(3.14159265358979323846);
            if (e->text == "e") return Value(2.71828182845904523536);
            throw std::runtime_error("Unknown variable: " + e->text);
        }

        if (e->kind == Expr::Unary) {
            Value v = eval(e->left, part);
            if (e->text == "!") return Value(!v.truth());
            if (e->text == "-") return Value(-v.number());
            if (e->text == "+") return Value(v.number());

            if (e->text == "post!") {
                double n = v.number();
                if (n < 0 || n > 170 || std::floor(n) != n)
                    throw std::runtime_error("Factorial requires integer 0..170");
                double result = 1;
                for (int i = 2; i <= static_cast<int>(n); ++i) result *= i;
                return Value(result);
            }
        }

        if (e->kind == Expr::Binary) {
            Value a = eval(e->left, part);
            Value b = eval(e->right, part);
            double x = a.number(), y = b.number();

            if (e->text == "+") {
                if (std::holds_alternative<std::string>(a.data) ||
                    std::holds_alternative<std::string>(b.data))
                    return Value(a.str() + b.str());
                return Value(x + y);
            }
            if (e->text == "-") return Value(x - y);
            if (e->text == "*") return Value(x * y);
            if (e->text == "/") {
                if (y == 0) throw std::runtime_error("Division by zero");
                return Value(x / y);
            }
            if (e->text == "%") {
                if (y == 0) throw std::runtime_error("Modulo by zero");
                return Value(std::fmod(x, y));
            }
            if (e->text == "^") return Value(std::pow(x, y));
            if (e->text == "^?") {
                if (x <= 0 || x == 1 || y <= 0)
                    throw std::runtime_error("Invalid logarithm");
                return Value(std::log(y) / std::log(x));
            }
            if (e->text == "^/") {
                if (y == 0) throw std::runtime_error("Root degree cannot be zero");
                if (x < 0 && std::floor(y) != y)
                    throw std::runtime_error("Invalid real root");
                return Value(x < 0 ? -std::pow(-x, 1.0 / y)
                                   : std::pow(x, 1.0 / y));
            }
            if (e->text == "==") return Value(a.str() == b.str());
            if (e->text == "!=") return Value(a.str() != b.str());
            if (e->text == "<") return Value(x < y);
            if (e->text == ">") return Value(x > y);
            if (e->text == "<=") return Value(x <= y);
            if (e->text == ">=") return Value(x >= y);
            if (e->text == "&&" || e->text == "and")
                return Value(a.truth() && b.truth());
            if (e->text == "||" || e->text == "or")
                return Value(a.truth() || b.truth());
            if (e->text == "xor")
                return Value(a.truth() != b.truth());
        }

        if (e->kind == Expr::Call) {
            std::vector<Value> args;
            for (const auto& a : e->args) args.push_back(eval(a, part));

            auto arg = [&](size_t i) -> double {
                if (i >= args.size()) throw std::runtime_error("Missing argument");
                return args[i].number();
            };

            const std::string& n = e->text;
            if (n == "abs") return Value(std::abs(arg(0)));
            if (n == "round") return Value(std::round(arg(0)));
            if (n == "floor") return Value(std::floor(arg(0)));
            if (n == "ceil") return Value(std::ceil(arg(0)));
            if (n == "sqrt") return Value(std::sqrt(arg(0)));
            if (n == "sin") return Value(std::sin(arg(0)));
            if (n == "cos") return Value(std::cos(arg(0)));
            if (n == "tan") return Value(std::tan(arg(0)));
            if (n == "log.2") return Value(std::log2(arg(0)));
            if (n == "log.10") return Value(std::log10(arg(0)));
            if (n == "log") {
                if (args.size() == 1) return Value(std::log(arg(0)));
                return Value(std::log(arg(1)) / std::log(arg(0)));
            }

            if (n == "bit.not") {
                int bits = static_cast<int>(arg(1));
                if (bits < 1 || bits > 64) throw std::runtime_error("bit width must be 1..64");
                unsigned long long mask = bits == 64
                    ? ~0ULL : ((1ULL << bits) - 1ULL);
                return Value(static_cast<double>((~static_cast<unsigned long long>(arg(0))) & mask));
            }
            if (n == "bit.and" || n == "bit.or") {
                int bits = static_cast<int>(arg(2));
                if (bits < 1 || bits > 64) throw std::runtime_error("bit width must be 1..64");
                unsigned long long mask = bits == 64
                    ? ~0ULL : ((1ULL << bits) - 1ULL);
                auto A = static_cast<unsigned long long>(arg(0)) & mask;
                auto B = static_cast<unsigned long long>(arg(1)) & mask;
                return Value(static_cast<double>(
                    (n == "bit.and" ? (A & B) : (A | B)) & mask
                ));
            }

            if (n == "print" || n == "out.push" || n == "out.report") {
                for (const auto& v : args) std::cout << v.str();
                if (n == "print") std::cout << '\n';
                std::cout.flush();
                return {};
            }

            if (n == "stop") {
                running = false;
                return {};
            }

            auto it = functions.find(n);
            if (it != functions.end()) {
                if (it->second.params.size() != args.size())
                    throw std::runtime_error("Wrong argument count for " + n);

                // A function frame prevents local variables leaking globally.
                // This minimal runtime supports expression returns only through
                // the shared evaluator; statement returns are handled below.
                std::map<std::string, Value> savedLocals;
                std::map<std::string, Value> savedGlobals;
                if (part) savedLocals = part->locals;
                else savedGlobals = globals;

                auto restoreFrame = [&]() {
                    if (part) part->locals = savedLocals;
                    else globals = savedGlobals;
                };

                for (size_t i = 0; i < args.size(); ++i) {
                    if (part) part->locals[it->second.params[i]] = args[i];
                    else globals[it->second.params[i]] = args[i];
                }

                try {
                    executeList(it->second.body, part);
                } catch (const ControlFlow& flow) {
                    restoreFrame();
                    if (flow.kind == ControlFlow::Return) return flow.value;
                    throw std::runtime_error(
                        flow.kind == ControlFlow::Break
                            ? "break used outside a loop"
                            : "continue used outside a loop"
                    );
                } catch (...) {
                    restoreFrame();
                    throw;
                }

                restoreFrame();
                return {};
            }

            throw std::runtime_error("Unknown function: " + n);
        }

        return {};
    }

    bool executeOne(const S& s, Part& part) {
        if (!s) return true;

        switch (s->kind) {
            case Stmt::Empty:
                return true;

            case Stmt::Block:
                for (const auto& child : s->body) {
                    if (!executeOne(child, part)) return false;
                    if (!running) return false;
                }
                return true;

            case Stmt::Declaration: {
                Value v = s->expr ? eval(s->expr, &part) : Value(0);
                part.locals[s->name] = v;
                return true;
            }

            case Stmt::Assignment: {
                Value v = eval(s->expr, &part);
                if (part.locals.count(s->name)) part.locals[s->name] = v;
                else if (globals.count(s->name)) globals[s->name] = v;
                else part.locals[s->name] = v;
                return true;
            }

            case Stmt::Expression:
                eval(s->expr, &part);
                return true;

            case Stmt::Output:
                for (const auto& e : s->exprs)
                    std::cout << eval(e, &part).str();
                if (s->name == "endl") std::cout << '\n';
                std::cout.flush();
                return true;

            case Stmt::Wait: {
                double ms = eval(s->expr, &part).number();
                if (!std::isfinite(ms))
                    throw std::runtime_error("wait duration must be finite");
                if (ms < 0) ms = 0;
                auto duration = std::chrono::duration<double, std::milli>(ms);
                std::this_thread::sleep_for(duration);
                return true;
            }

            case Stmt::If:
                if (eval(s->expr, &part).truth())
                    return s->body.empty() || executeOne(s->body[0], part);
                return !s->otherwise || executeOne(s->otherwise, part);

            case Stmt::While:
                while (eval(s->expr, &part).truth()) {
                    try {
                        if (!s->body.empty() && !executeOne(s->body[0], part))
                            return false;
                    } catch (const ControlFlow& flow) {
                        if (flow.kind == ControlFlow::Break) break;
                        if (flow.kind == ControlFlow::Continue) continue;
                        throw;
                    }
                    if (!running) return false;
                }
                return true;

            case Stmt::Return:
                throw ControlFlow{ControlFlow::Return,
                    s->expr ? eval(s->expr, &part) : Value()};

            case Stmt::Break:
                throw ControlFlow{ControlFlow::Break, Value()};

            case Stmt::Continue:
                throw ControlFlow{ControlFlow::Continue, Value()};
        }

        return true;
    }

    void executeList(const std::vector<S>& list, Part* part) {
        Part temporary;
        Part& current = part ? *part : temporary;
        for (const auto& s : list) {
            if (!executeOne(s, current) || !running) break;
        }
    }

    static std::string betweenBraces(const std::string& s, size_t open) {
        int depth = 0;
        bool quoted = false;
        char quote = '\0';
        for (size_t i = open; i < s.size(); ++i) {
            char c = s[i];
            if (quoted) {
                if (c == '\\') { ++i; continue; }
                if (c == quote) quoted = false;
                continue;
            }
            if (c == '"' || c == '\'') {
                quoted = true; quote = c; continue;
            }
            if (c == '{') ++depth;
            if (c == '}' && --depth == 0)
                return s.substr(open + 1, i - open - 1);
        }
        throw std::runtime_error("Unclosed block");
    }

    static std::string findBlock(const std::string& s,
                                 const std::string& marker,
                                 size_t from = 0) {
        size_t pos = s.find(marker, from);
        if (pos == std::string::npos) return {};
        size_t open = s.find('{', pos + marker.size());
        if (open == std::string::npos) throw std::runtime_error("Expected block after " + marker);
        return betweenBraces(s, open);
    }

    static std::string partName(const std::string& header) {
        size_t a = header.find('"');
        if (a == std::string::npos) return "Part";
        size_t b = header.find('"', a + 1);
        if (b == std::string::npos) return "Part";
        return header.substr(a + 1, b - a - 1);
    }

    void parseSource(const std::string& source) {
        size_t hp = source.find("#head");
        if (hp != std::string::npos) {
            size_t open = source.find('{', hp);
            if (open == std::string::npos) throw std::runtime_error("Invalid #head");
            std::string head = betweenBraces(source, open);

            // Find function declarations: return-type name(args) { ... }
            // Remaining top-level declarations are parsed as ordinary statements.
            std::istringstream lines(head);
            std::string line;
            std::string declarations;

            while (std::getline(lines, line)) {
                std::string x = trim(line);
                if (x.empty()) continue;
                declarations += line + "\n";
            }

            // Keep a copy of #head with function definitions blanked out.
            // The remaining top-level statements are parsed normally so initializers
            // such as `int hp = 100;` are evaluated instead of defaulting to zero.
            std::string globalCode = head;

            // Extract functions by scanning identifier followed by (...) and {...}.
            size_t i = 0;
            while (i < head.size()) {
                size_t par = head.find('(', i);
                if (par == std::string::npos) break;

                size_t nameEnd = par;
                while (nameEnd > 0 &&
                       std::isspace(static_cast<unsigned char>(head[nameEnd - 1])))
                    --nameEnd;

                size_t nameStart = nameEnd;
                while (nameStart > 0 &&
                       (std::isalnum(static_cast<unsigned char>(head[nameStart - 1])) ||
                        head[nameStart - 1] == '_' ||
                        head[nameStart - 1] == '.'))
                    --nameStart;

                std::string name = head.substr(nameStart, nameEnd - nameStart);
                size_t close = head.find(')', par);
                if (close == std::string::npos) break;

                size_t brace = head.find('{', close);
                size_t semi = head.find(';', close);
                if (brace == std::string::npos ||
                    (semi != std::string::npos && semi < brace)) {
                    i = close + 1;
                    continue;
                }

                // Check that this resembles a function declaration.
                if (name.empty() || name[0] == '#') {
                    i = close + 1;
                    continue;
                }

                std::string paramsText = head.substr(par + 1, close - par - 1);
                std::vector<std::string> params;
                std::istringstream ps(paramsText);
                std::string param;
                while (std::getline(ps, param, ',')) {
                    param = trim(param);
                    if (param.empty()) continue;
                    std::istringstream one(param);
                    std::string first, second;
                    one >> first >> second;
                    if (second.empty()) params.push_back(first);
                    else params.push_back(second);
                }

                std::string body = betweenBraces(head, brace);
                Lexer lex(body);
                Parser parser(lex.scan());

                Function f;
                f.params = std::move(params);
                f.body = parser.parseBody();
                functions[name] = std::move(f);

                // Determine the function signature's start (normally its line start)
                // and blank the entire definition while preserving source positions.
                size_t sigStart = nameStart;
                while (sigStart > 0 && head[sigStart - 1] != '\n' &&
                       head[sigStart - 1] != ';' && head[sigStart - 1] != '}')
                    --sigStart;

                // Avoid treating nested declarations as separate functions.
                int depth = 1;
                size_t end = brace + 1;
                while (end < head.size() && depth > 0) {
                    if (head[end] == '{') ++depth;
                    if (head[end] == '}') --depth;
                    ++end;
                }

                for (size_t k = sigStart; k < end; ++k) {
                    if (globalCode[k] != '\n') globalCode[k] = ' ';
                }
                i = end;
            }

            // Parse top-level global declarations and evaluate their initializers.
            // This supports multiple declarations and expressions, not just `= 0`.
            Lexer globalLexer(globalCode);
            Parser globalParser(globalLexer.scan());
            auto globalStatements = globalParser.parseBody();
            Part globalContext;

            for (const auto& statement : globalStatements) {
                if (!statement) continue;
                if (statement->kind == Stmt::Declaration) {
                    Value initial = statement->expr
                        ? eval(statement->expr, &globalContext)
                        : Value(0);
                    globals[statement->name] = initial;
                } else if (statement->kind == Stmt::Assignment) {
                    globals[statement->name] = eval(statement->expr, &globalContext);
                } else if (statement->kind != Stmt::Empty) {
                    throw std::runtime_error(
                        "Only global declarations and assignments are allowed directly in #head"
                    );
                }
            }
        }

        size_t pos = 0;
        while ((pos = source.find("#part", pos)) != std::string::npos) {
            size_t par = source.find('(', pos);
            size_t close = source.find(')', par);
            size_t open = source.find('{', close);
            if (par == std::string::npos || close == std::string::npos ||
                open == std::string::npos)
                throw std::runtime_error("Invalid #part declaration");

            std::string header = source.substr(par + 1, close - par - 1);
            std::string body = betweenBraces(source, open);

            Part part;
            part.name = partName(header);
            part.enabled = header.find("false") == std::string::npos;

            Lexer lex(body);
            Parser parser(lex.scan());
            part.code = parser.parseBody();

            parts.push_back(std::move(part));
            pos = open + body.size() + 2;
        }
    }

    bool reloadRequested() {
        for (const auto& [path, oldTime] : watchedFiles) {
            try {
                if (fs::last_write_time(path) != oldTime) return true;
            } catch (...) {
                return true;
            }
        }
        return false;
    }

public:
    void run(const std::string& file) {
        std::vector<fs::path> stack;
        std::string source = loadIncludes(file, stack);
        parseSource(source);

        while (running) {
            bool anyWork = false;

            for (auto& part : parts) {
                if (!part.enabled || part.done) continue;

                anyWork = true;

                try {
                    executeList(part.code, &part);
                    part.done = true;
                } catch (const ControlFlow& flow) {
                    if (flow.kind == ControlFlow::Return) {
                        part.done = true;
                    } else {
                        std::cerr << "[DOSCU Part: " << part.name << "] "
                                  << (flow.kind == ControlFlow::Break
                                          ? "break" : "continue")
                                  << " used outside a loop\n";
                        part.done = true;
                    }
                } catch (const std::exception& ex) {
                    std::cerr << "[DOSCU Part: " << part.name << "] "
                            << ex.what() << '\n';
                    part.done = true;
                }

                if (!running) break;
            }

            if (reloadRequested()) {
                std::cout << "[DOSCU] Source file changed; restart the runtime "
                            "to load the new version.\n";
                break;
            }

            if (!anyWork) break;
        }
    }
};

int main(int argc, char** argv) {
  
    try {
        std::string file = mainFile;
        if (argc > 1) file = argv[1];

        Runtime runtime;
        runtime.run(file);
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "[DOSCU Fatal] " << ex.what() << '\n';
        return 1;
    }
}
