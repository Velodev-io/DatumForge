#include "Expressions.hpp"
#include <cmath>
#include <numbers>
#include <vector>
namespace cad::expression {
using domain::check;
namespace {
const QMap<QString, Quantity> units{{"mm", {.001, length}},
                                    {"cm", {.01, length}},
                                    {"m", {1, length}},
                                    {"in", {.0254, length}},
                                    {"deg", {std::numbers::pi / 180, angle}},
                                    {"rad", {1, angle}},
                                    {"kg", {1, mass}},
                                    {"g", {.001, mass}}};
bool identifier(QString n) {
    return QRegularExpression("^[A-Za-z][A-Za-z0-9_]{0,63}$").match(n).hasMatch();
}
Quantity finite(Quantity q) {
    check(std::isfinite(q.value) && std::abs(q.value) <= 1e100, "InvalidInput",
          "Expression produced a nonfinite or oversized value");
    for (int n : q.dimension)
        check(std::abs(n) <= 8, "ResourceLimit", "Dimension exponent exceeds 8");
    return q;
}
void same(Quantity a, Quantity b) {
    check(a.dimension == b.dimension, "UnitMismatch",
          "Expression operands have different dimensions");
}
class Parser {
    Environment &env;
    QString text, token;
    int at = 0, nodes = 0;
    bool numeric = false;
    void next() {
        while (at < text.size() && text[at].isSpace())
            ++at;
        token.clear();
        numeric = false;
        if (at == text.size())
            return;
        int begin = at;
        QChar c = text[at];
        if (c.isDigit() || c == '.') {
            bool digits = false;
            while (at < text.size() && text[at].isDigit()) {
                ++at;
                digits = true;
            }
            if (at < text.size() && text[at] == '.') {
                ++at;
                while (at < text.size() && text[at].isDigit()) {
                    ++at;
                    digits = true;
                }
            }
            check(digits, "InvalidInput", "Invalid numeric literal");
            if (at < text.size() && (text[at] == 'e' || text[at] == 'E')) {
                ++at;
                if (at < text.size() && (text[at] == '+' || text[at] == '-'))
                    ++at;
                int exponent = at;
                while (at < text.size() && text[at].isDigit())
                    ++at;
                check(at > exponent, "InvalidInput", "Invalid exponent");
            }
            numeric = true;
        } else if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
            ++at;
            while (at < text.size() && (text[at].isLetterOrNumber() || text[at] == '_'))
                ++at;
        } else {
            check(QString("+-*/^(),").contains(c), "InvalidInput",
                  "Unsupported expression character");
            ++at;
        }
        token = text.mid(begin, at - begin);
    }
    void node() {
        check(++nodes <= 256, "ResourceLimit", "Expression exceeds 256 AST nodes");
    }
    Quantity call(QString f, std::vector<Quantity> args) {
        check(args.size() == 1 || ((f == "min" || f == "max") && args.size() == 2), "InvalidInput",
              "Invalid function argument count");
        auto a = args[0];
        if (f == "min" || f == "max") {
            check(args.size() == 2, "InvalidInput", "min/max require two arguments");
            same(a, args[1]);
            a.value =
                f == "min" ? std::min(a.value, args[1].value) : std::max(a.value, args[1].value);
        } else if (f == "abs")
            a.value = std::abs(a.value);
        else if (f == "sqrt") {
            check(a.value >= 0, "InvalidInput", "sqrt domain requires a nonnegative value");
            for (auto &n : a.dimension) {
                check(n % 2 == 0, "UnitMismatch", "sqrt requires even dimension exponents");
                n /= 2;
            }
            a.value = std::sqrt(a.value);
        } else if (f == "sin" || f == "cos" || f == "tan") {
            check(a.dimension == angle, "UnitMismatch", "Trigonometric functions require angles");
            if (f == "tan")
                check(std::abs(std::cos(a.value)) > 1e-12, "InvalidInput",
                      "tan is singular at this angle");
            a.value = f == "sin"   ? std::sin(a.value)
                      : f == "cos" ? std::cos(a.value)
                                   : std::tan(a.value);
            a.dimension = scalar;
        } else if (f == "asin" || f == "acos" || f == "atan") {
            check(a.dimension == scalar, "UnitMismatch",
                  "Inverse trigonometric functions require scalars");
            check(f == "atan" || std::abs(a.value) <= 1, "InvalidInput",
                  "Inverse trigonometric domain exceeded");
            a.value = f == "asin"   ? std::asin(a.value)
                      : f == "acos" ? std::acos(a.value)
                                    : std::atan(a.value);
            a.dimension = angle;
        } else
            throw domain::Error("UnsupportedOperation", "Function is not allowlisted: " + f);
        return finite(a);
    }
    Quantity parse(int precedence, int depth) {
        check(depth <= 32, "ResourceLimit", "Expression exceeds 32 nesting levels");
        node();
        Quantity a;
        if (token == "+" || token == "-") {
            bool minus = token == "-";
            next();
            a = parse(3, depth + 1);
            if (minus)
                a.value = -a.value;
        } else if (token == "(") {
            next();
            a = parse(0, depth + 1);
            check(token == ")", "InvalidInput", "Missing closing parenthesis");
            next();
        } else if (numeric) {
            bool ok;
            a.value = token.toDouble(&ok);
            check(ok && std::isfinite(a.value), "InvalidInput", "Invalid or nonfinite number");
            next();
            if (units.contains(token)) {
                auto u = units.value(token);
                a.value *= u.value;
                a.dimension = u.dimension;
                next();
            }
        } else {
            auto id = token;
            check(identifier(id), "InvalidInput", "Expected a quantity or name");
            next();
            if (token == "(") {
                next();
                std::vector<Quantity> args;
                args.push_back(parse(0, depth + 1));
                if (token == ",") {
                    next();
                    args.push_back(parse(0, depth + 1));
                }
                check(token == ")", "InvalidInput", "Expected function closing parenthesis");
                next();
                a = call(id, args);
            } else
                a = env.name(id);
        }
        a = finite(a);
        while (true) {
            int p = token == "+" || token == "-"   ? 1
                    : token == "*" || token == "/" ? 2
                    : token == "^"                 ? 3
                                                   : -1;
            if (p < precedence)
                break;
            auto op = token;
            node();
            next();
            auto b = parse(op == "^" ? p : p + 1, depth + 1);
            if (op == "+" || op == "-") {
                same(a, b);
                a.value += op == "+" ? b.value : -b.value;
            } else if (op == "*" || op == "/") {
                check(op != "/" || b.value != 0, "InvalidInput", "Division by zero");
                a.value = op == "*" ? a.value * b.value : a.value / b.value;
                for (int i = 0; i < 3; ++i)
                    a.dimension[i] += b.dimension[i] * (op == "*" ? 1 : -1);
            } else {
                check(b.dimension == scalar && std::floor(b.value) == b.value &&
                          std::abs(b.value) <= 8,
                      "UnitMismatch", "Powers require an integer scalar exponent within ±8");
                check(!(a.value == 0 && b.value <= 0), "InvalidInput", "Invalid zero power");
                a.value = std::pow(a.value, b.value);
                for (int &n : a.dimension)
                    n *= int(b.value);
            }
            a = finite(a);
        }
        return a;
    }

  public:
    Parser(Environment &e, QString value) : env(e), text(std::move(value)) {
        check(!text.trimmed().isEmpty() && text.toUtf8().size() <= 4096, "ResourceLimit",
              "Expression requires 1–4096 UTF-8 bytes");
        next();
    }
    Quantity evaluate() {
        auto q = parse(0, 0);
        check(token.isEmpty(), "InvalidInput", "Trailing expression input");
        return finite(q);
    }
};
} // namespace
Quantity unit(QString name) {
    if (name == "1")
        return {1, scalar};
    check(units.contains(name), "UnitMismatch", "Unsupported display unit");
    return units.value(name);
}
Environment::Environment(QMap<QString, QString> names, int maximumNames)
    : definitions(std::move(names)) {
    check(maximumNames >= 0 && maximumNames <= 1024 && definitions.size() <= maximumNames,
          "ResourceLimit", "Named expression admission exceeded");
    for (auto i = definitions.begin(); i != definitions.end(); ++i)
        check(identifier(i.key()) && !units.contains(i.key()) && i.key() != "pi", "InvalidInput",
              "Invalid or reserved expression name");
}
Quantity Environment::name(QString n) {
    if (units.contains(n))
        return units.value(n);
    if (n == "pi")
        return {std::numbers::pi, scalar};
    if (cache.contains(n))
        return cache.value(n);
    check(definitions.contains(n), "ReferenceMissing", "Unknown expression name: " + n);
    check(!visiting.contains(n), "DependencyCycle", "Cyclic named expression: " + n);
    check(visiting.size() < 32, "ResourceLimit", "Named expression depth exceeds 32");
    visiting.insert(n);
    try {
        auto q = evaluate(definitions.value(n));
        cache.insert(n, q);
        visiting.remove(n);
        return q;
    } catch (...) {
        visiting.remove(n);
        throw;
    }
}
Quantity Environment::evaluate(QString text) {
    return Parser(*this, std::move(text)).evaluate();
}
void Environment::validateAll() {
    for (auto i = definitions.begin(); i != definitions.end(); ++i)
        name(i.key());
}
QString format(Quantity q, QString u, int precision) {
    auto factor = unit(u);
    same(q, factor);
    check(precision >= 1 && precision <= 17, "InvalidInput", "Invalid display precision");
    finite(q);
    return QString::number(q.value / factor.value, 'g', precision) +
           (u == "1" ? QString() : " " + u);
}
} // namespace cad::expression
