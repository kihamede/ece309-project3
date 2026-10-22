// ECE 309 Project 3 - Two-stack calculator.
// The tokenizer is already provided; these are just the calculation steps.

#include "tools/calculator.h"
#include <stack>

namespace {

using Kind = CalcToken::Kind;

int precedence(Kind op) {
    if (op == Kind::Times || op == Kind::Divide) return 2;
    if (op == Kind::Plus || op == Kind::Minus) return 1;
    return 0;
}

// Take two values and one operator and push the result back.
bool apply(std::stack<double>& values, std::stack<Kind>& ops,
           std::string* error) {
    if (ops.empty() || values.size() < 2) {
        *error = "missing value or operator";
        return false;
    }

    Kind op = ops.top();
    ops.pop();

    double right = values.top();
    values.pop();
    double left = values.top();
    values.pop();

    double answer = 0.0;
    if (op == Kind::Plus) answer = left + right;
    else if (op == Kind::Minus) answer = left - right;
    else if (op == Kind::Times) answer = left * right;
    else if (op == Kind::Divide) {
        if (right == 0.0) {
            *error = "division by zero";
            return false;
        }
        answer = left / right;
    } else {
        *error = "invalid operator";
        return false;
    }

    values.push(answer);
    return true;
}

}  // namespace

bool calc_evaluate(const std::vector<CalcToken>& tokens, double* out,
                   std::string* error) {
    std::stack<double> values;
    std::stack<Kind> ops;

    for (const CalcToken& t : tokens) {
        if (t.kind == Kind::Number) {
            values.push(t.value);
        } else if (t.kind == Kind::LParen) {
            ops.push(t.kind);
        } else if (t.kind == Kind::RParen) {
            while (!ops.empty() && ops.top() != Kind::LParen) {
                if (!apply(values, ops, error)) return false;
            }
            if (ops.empty()) {
                *error = "missing left parenthesis";
                return false;
            }
            ops.pop();
        } else {
            // Apply operators of higher or equal priority first.
            while (!ops.empty() && ops.top() != Kind::LParen &&
                   precedence(ops.top()) >= precedence(t.kind)) {
                if (!apply(values, ops, error)) return false;
            }
            ops.push(t.kind);
        }
    }

    while (!ops.empty()) {
        if (ops.top() == Kind::LParen) {
            *error = "missing right parenthesis";
            return false;
        }
        if (!apply(values, ops, error)) return false;
    }

    if (values.size() != 1) {
        *error = "invalid expression";
        return false;
    }

    *out = values.top();
    error->clear();
    return true;
}
