// tests/p3/test_p3.cpp — YOUR P3 TEST SUITE.
//
// The five tests below are worked examples. Keep them, and add your own
// until you have AT LEAST 15 (spec §6 lists what must be covered).
// Name every test <area>_<what>: reg_ for the registry, ws_ for the
// workspace, calc_ for the calculator.
//
// Build & run:   cmake --build build && ./build/test_p3
// One area:      ./build/test_p3 reg_        (names starting with "reg_")
// One test:      ./build/test_p3 =reg_add_and_find

#include "../mini_test.h"
#include "../test_helpers.h"     // TempDir, write_text, args1, ...

#include "tools/calculator.h"
#include "tools/tool_registry.h"
#include "tools/workspace.h"

#include <algorithm>
#include <cstdio>
#include <set>
#include <memory>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

// A tool whose name you choose — use it to fill the registry in tests.
class NamedTool : public Tool {
public:
    explicit NamedTool(std::string n) : n_(std::move(n)), schema_(JsonValue::object()) {}
    std::string name() const override { return n_; }
    std::string description() const override { return "test tool"; }
    const JsonValue& schema() const override { return schema_; }
    ToolResult invoke(const JsonValue&, ToolContext&) override { return ToolResult::success(n_); }
private:
    std::string n_;
    JsonValue   schema_;   // an empty JSON object: {}
};

// Checks that the registry really owns and deletes its tools.
class CountTool : public Tool {
public:
    CountTool(std::string n, int* counter)
        : name_(std::move(n)), counter_(counter), schema_(JsonValue::object()) {}
    ~CountTool() override { ++(*counter_); }
    std::string name() const override { return name_; }
    std::string description() const override { return "counting test tool"; }
    const JsonValue& schema() const override { return schema_; }
    ToolResult invoke(const JsonValue&, ToolContext&) override {
        return ToolResult::success("ok");
    }
private:
    std::string name_;
    int* counter_;
    JsonValue schema_;
};

// Tokenize with the provided tokenizer, then run YOUR calc_evaluate.
bool eval(const std::string& expr, double* value, std::string* error) {
    std::vector<CalcToken> tokens;
    if (!calc_tokenize(expr, &tokens, error)) return false;
    return calc_evaluate(tokens, value, error);
}

}  // namespace

// ---------------------------------------------------------------- examples

TEST(reg_add_and_find) {
    ToolRegistry r;
    auto t = std::make_unique<NamedTool>("alpha");
    Tool* raw = t.get();                        // remember the address before moving
    REQUIRE(r.add(std::move(t)));
    CHECK(r.size() == 1);
    CHECK(r.find("alpha") == raw);              // same object, not a copy
    CHECK(r.find("beta") == nullptr);
}

TEST(reg_duplicate_keeps_original) {
    ToolRegistry r;
    auto first = std::make_unique<NamedTool>("alpha");
    Tool* original = first.get();
    REQUIRE(r.add(std::move(first)));
    CHECK(!r.add(std::make_unique<NamedTool>("alpha")));
    CHECK(r.find("alpha") == original);
    CHECK(r.size() == 1);
}

TEST(ws_rejects_parent_escape) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
    REQUIRE(ws.resolve("notes/todo.txt", &out, &reason));   // sanity: accepts *something*
    CHECK(out == ws.root() / "notes" / "todo.txt");
    CHECK(!ws.resolve("../etc/passwd", &out, &reason));
    CHECK(!reason.empty());
}

TEST(calc_precedence) {
    double v = 0;
    std::string err;
    REQUIRE(eval("2 + 3 * 4", &v, &err));
    CHECK(v == 14);
}

TEST(calc_division_by_zero) {
    double v = 0;
    std::string err;
    CHECK(!eval("1 / 0", &v, &err));
    CHECK(err == "division by zero");
}

// ------------------------------------------------------------- your tests
//
// Still to write (spec §6 has the full list). Suggested names:
//   reg_null_rejected, reg_for_each_visits_each_once
//   reg_10000_tools               REQUIRED: print bucket_count / load_factor /
//                                 max_probe_length, check the bound
//   reg_alternative_hash          REQUIRED: longest chain of a weak hash
//   ws_<case>                     REQUIRED: every row of the table in spec §4.4
//   calc_<case>                   left-to-right, parentheses, negatives

// ------------------------------------------------------ registry tests

TEST(reg_owns_and_destroys_tools) {
    int destroyed = 0;
    {
        ToolRegistry r;
        REQUIRE(r.add(std::make_unique<CountTool>("original", &destroyed)));
        CHECK(!r.add(std::make_unique<CountTool>("original", &destroyed)));
        CHECK(destroyed == 1); // only the rejected new tool got destroyed
        REQUIRE(r.add(std::make_unique<CountTool>("second", &destroyed)));
    }
    CHECK(destroyed == 3); // the two stored tools are deleted once
}

TEST(reg_starts_empty) {
    ToolRegistry r;
    CHECK_EQ(r.size(), std::size_t{0});
    CHECK(r.bucket_count() >= 1);
    CHECK(r.bucket_count() <= 64);
    CHECK(r.load_factor() == 0.0);
    CHECK(r.max_probe_length() == 0);
    CHECK(r.find("unknown") == nullptr);
}

TEST(reg_null_rejected) {
    ToolRegistry r;
    CHECK(!r.add(nullptr));
    CHECK(r.size() == 0);
}

TEST(reg_for_each_once) {
    ToolRegistry r;
    REQUIRE(r.add(std::make_unique<NamedTool>("one")));
    REQUIRE(r.add(std::make_unique<NamedTool>("two")));
    REQUIRE(r.add(std::make_unique<NamedTool>("three")));
    std::set<std::string> visited;
    int calls = 0;
    r.for_each([&](const Tool& tool) {
        visited.insert(tool.name());
        calls++;
    });
    CHECK(calls == 3);
    CHECK(visited.size() == 3);
    CHECK(visited.count("one") == 1);
    CHECK(visited.count("two") == 1);
    CHECK(visited.count("three") == 1);
}

TEST(reg_hash_uses_entire_name) {
    CHECK(ToolRegistry::hash("ab") != ToolRegistry::hash("ba"));
    CHECK(ToolRegistry::hash("tool_1234") != ToolRegistry::hash("tool_1243"));
    std::set<std::size_t> hashes;
    for (int i = 0; i < 10000; i++) {
        hashes.insert(ToolRegistry::hash("tool_" + std::to_string(i)));
    }
    CHECK(hashes.size() >= 9990);
}

TEST(reg_10000_tools_and_weak_hash) {
    ToolRegistry r;
    auto first = std::make_unique<NamedTool>("tool_0");
    Tool* original = first.get();
    REQUIRE(r.add(std::move(first)));
    for (int i = 1; i < 10000; ++i) {
        REQUIRE(r.add(std::make_unique<NamedTool>("tool_" + std::to_string(i))));
        CHECK(r.load_factor() <= 1.0);
    }
    CHECK(r.size() == 10000);
    CHECK(r.find("tool_0") == original);
    for (int i = 0; i < 10000; ++i) {
        CHECK(r.find("tool_" + std::to_string(i)) != nullptr);
    }
    CHECK(r.max_probe_length() <= 64);

    // Compare with the weak alternative s[0] + s.size(), same bucket count.
    std::vector<int> weak(r.bucket_count(), 0);
    for (int i = 0; i < 10000; ++i) {
        std::string name = "tool_" + std::to_string(i);
        std::size_t weak_hash = static_cast<unsigned char>(name[0]) + name.size();
        weak[weak_hash % weak.size()]++;
    }
    int weak_longest = *std::max_element(weak.begin(), weak.end());
    std::printf("hash measurements: buckets=%zu, load=%.4f, longest real chain=%zu, longest weak chain=%d\n",
                r.bucket_count(), r.load_factor(), r.max_probe_length(), weak_longest);
    CHECK(weak_longest > static_cast<int>(r.max_probe_length()));
}

TEST(reg_multiple_resizes_maintain_pointer) {
    ToolRegistry r;
    REQUIRE(r.add(std::make_unique<NamedTool>("keep")));
    Tool* address = r.find("keep");
    for (int i = 0; i < 1500; i++) {
        REQUIRE(r.add(std::make_unique<NamedTool>("extra_" + std::to_string(i))));
    }
    CHECK(r.find("keep") == address);
    CHECK(r.bucket_count() > 16);
    CHECK(r.size() == 1501);
}

// ------------------------------------------------------ workspace tests

TEST(ws_relative_and_normalized) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
    REQUIRE(ws.resolve("notes/todo.txt", &out, &reason));
    CHECK(out == ws.root() / "notes/todo.txt");
    REQUIRE(ws.resolve("./notes//todo.txt", &out, &reason));
    CHECK(out == ws.root() / "notes/todo.txt");
}

TEST(ws_dot_and_dotdot_inside) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string why;
    REQUIRE(ws.resolve(".", &out, &why));
    CHECK(out == ws.root());
    REQUIRE(ws.resolve("a/../b", &out, &why));
    CHECK(out == ws.root() / "b");
    REQUIRE(ws.resolve("a/..", &out, &why));
    CHECK(out == ws.root());
}

TEST(ws_new_file_and_dots_in_filename) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string why;
    REQUIRE(ws.resolve("new/dir/file.txt", &out, &why));
    CHECK(out == ws.root() / "new/dir/file.txt");
    REQUIRE(ws.resolve("my..notes.txt", &out, &why));
    CHECK(out == ws.root() / "my..notes.txt");
    CHECK(!fs::exists(ws.root() / "new")); // resolve did not create folders
}

TEST(ws_empty_rejected) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string why;
    CHECK(!ws.resolve("", &out, &why));
    CHECK(!why.empty());
}

TEST(ws_embedded_null_rejected) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string why;
    std::string with_null = std::string("notes") + '\0' + "/x";
    CHECK(!ws.resolve(with_null, &out, &why));
    CHECK(!why.empty());
}

TEST(ws_absolute_rejected) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string why;
    CHECK(!ws.resolve("/etc/passwd", &out, &why));
    CHECK(!why.empty());
}

TEST(ws_parent_paths_rejected) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string why;
    for (const std::string s : {"..", "../etc/passwd", "a/../../b",
                                "notes/../../ws/notes/todo.txt"}) {
        CHECK(!ws.resolve(s, &out, &why));
        CHECK(!why.empty());
    }
}

// ------------------------------------------------------ calculator tests

TEST(calc_left_associative_subtraction) {
    double v = 0;
    std::string err;
    REQUIRE(eval("10 - 4 - 3", &v, &err));
    CHECK(v == 3);
}

TEST(calc_left_associative_division) {
    double v = 0;
    std::string err;
    REQUIRE(eval("100 / 10 / 5", &v, &err));
    CHECK(v == 2);
}

TEST(calc_parentheses) {
    double v = 0;
    std::string err;
    REQUIRE(eval("(2 + 3) * 4", &v, &err));
    CHECK(v == 20);
    REQUIRE(eval("2 * (3 + 4) - 5", &v, &err));
    CHECK(v == 9);
}

TEST(calc_negative_numbers) {
    double v = 0;
    std::string err;
    REQUIRE(eval("-3 * -3", &v, &err));
    CHECK(v == 9);
}

TEST(calc_decimal) {
    double v = 0;
    std::string err;
    REQUIRE(eval("7 / 2", &v, &err));
    CHECK(v == 3.5);
    REQUIRE(eval("1.5 + 2.25", &v, &err));
    CHECK(v == 3.75);
}

TEST(calc_zero_inside_parentheses) {
    double v = 0;
    std::string err;
    CHECK(!eval("1 / (2 - 2)", &v, &err));
    CHECK(err == "division by zero");
}

TEST(calc_two_more_expressions) {
    double v = 0;
    std::string err;
    REQUIRE(eval("(8 - 2) / 3 + 7", &v, &err));
    CHECK(v == 9);
    REQUIRE(eval("20 / (2 * 5)", &v, &err));
    CHECK(v == 2);
}

TEST(calc_many_operations_no_recursion) {
    std::string expr = "1";
    for (int i = 0; i < 20000; i++) expr += "+1";
    double v = 0;
    std::string err;
    REQUIRE(eval(expr, &v, &err));
    CHECK(v == 20001);
}

TEST(calc_deep_parentheses_no_recursion) {
    std::string expr(5000, '(');
    expr += "7";
    expr += std::string(5000, ')');
    double v = 0;
    std::string err;
    REQUIRE(eval(expr, &v, &err));
    CHECK(v == 7);
}

int main(int argc, char** argv) { return mt::run_all(argc, argv); }
