#include "crow/app.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <stdexcept>

using namespace std;

// ================= TRIM =================
string trim(const string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

// ================= NORMALIZE LINE =================
// Splits compound lines so '{' and '}' are always standalone tokens.
// "if (a > 3) {"  -> ["if (a > 3)", "{"]
// "} else {"      -> ["}", "else", "{"]
// "}else{"        -> ["}", "else", "{"]
// "else {"        -> ["else", "{"]
// "} else"        -> ["}", "else"]
vector<string> normalizeLine(const string& rawLine) {
    string line = trim(rawLine);
    vector<string> result;
    if (line.empty()) return result;

    string noSpace;
    for (char c : line) if (c != ' ' && c != '\t') noSpace += c;

    if (noSpace == "}else{") { result.push_back("}"); result.push_back("else"); result.push_back("{"); return result; }
    if (noSpace == "else{")  { result.push_back("else"); result.push_back("{"); return result; }
    if (noSpace == "}else")  { result.push_back("}"); result.push_back("else"); return result; }

    if (line.back() == '{') {
        string before = trim(line.substr(0, line.size() - 1));
        if (!before.empty() && before.back() == '}') {
            string beforeBrace = trim(before.substr(0, before.size() - 1));
            if (!beforeBrace.empty()) result.push_back(beforeBrace);
            result.push_back("}");
        } else {
            if (!before.empty()) result.push_back(before);
        }
        result.push_back("{");
        return result;
    }

    if (line.front() == '}' && line.size() > 1) {
        result.push_back("}");
        string rest = trim(line.substr(1));
        if (!rest.empty()) result.push_back(rest);
        return result;
    }

    result.push_back(line);
    return result;
}

// ================= SPLIT LINES =================
vector<string> splitLines(const string& code) {
    vector<string> lines;
    stringstream ss(code);
    string line;
    while (getline(ss, line)) {
        line = trim(line);
        if (line.empty()) continue;
        auto tokens = normalizeLine(line);
        for (auto& t : tokens)
            if (!t.empty()) lines.push_back(t);
    }
    return lines;
}

// ================= SAFE STOI =================
int safeStoi(const string& s, unordered_map<string, int>& memory) {
    if (memory.count(s)) return memory[s];
    try {
        size_t pos;
        int val = stoi(s, &pos);
        if (pos != s.size()) throw runtime_error("Invalid number: " + s);
        return val;
    } catch (const invalid_argument&) {
        throw runtime_error("Invalid number or undeclared variable: " + s);
    } catch (const out_of_range&) {
        throw runtime_error("Number out of range: " + s);
    }
}

// ================= EXPRESSION EVALUATION =================
int evaluateExpression(const string& expr, unordered_map<string, int>& memory) {
    stringstream ss(expr);
    string token;
    vector<string> tokens;
    while (ss >> token) tokens.push_back(token);
    if (tokens.empty()) throw runtime_error("Empty expression");

    for (auto& t : tokens)
        if (memory.count(t)) t = to_string(memory[t]);

    int result = safeStoi(tokens[0], memory);
    for (size_t i = 1; i < tokens.size(); i += 2) {
        if (i + 1 >= tokens.size())
            throw runtime_error("Incomplete expression: " + expr);
        const string& op = tokens[i];
        int val = safeStoi(tokens[i + 1], memory);
        if      (op == "+") result += val;
        else if (op == "-") result -= val;
        else if (op == "*") result *= val;
        else if (op == "/") {
            if (val == 0) throw runtime_error("Division by zero");
            result /= val;
        } else throw runtime_error("Unknown operator: " + op);
    }
    return result;
}

// ================= CONDITION EVALUATION =================
bool evaluateCondition(const string& condRaw, unordered_map<string, int>& memory) {
    string cond = condRaw;
    cond.erase(remove(cond.begin(), cond.end(), '('), cond.end());
    cond.erase(remove(cond.begin(), cond.end(), ')'), cond.end());
    cond = trim(cond);

    stringstream ss(cond);
    string l, op, r;
    ss >> l >> op >> r;
    if (l.empty() || op.empty() || r.empty())
        throw runtime_error("Invalid condition: " + condRaw);

    int left  = safeStoi(l, memory);
    int right = safeStoi(r, memory);

    if (op == "<")  return left < right;
    if (op == ">")  return left > right;
    if (op == "<=") return left <= right;
    if (op == ">=") return left >= right;
    if (op == "==") return left == right;
    if (op == "!=") return left != right;
    throw runtime_error("Invalid operator in condition: " + op);
}

// ================= IS CONTROL KEYWORD =================
// Returns true if this token begins a control structure (must never go to executeLine)
bool isControlKeyword(const string& line) {
    return (line.size() >= 2 && line.substr(0, 2) == "if")    ||
           (line.size() >= 5 && line.substr(0, 5) == "while") ||
           (line.size() >= 3 && line.substr(0, 3) == "for")   ||
           line == "else" || line == "{" || line == "}";
}

// ================= GET BLOCK =================
// REQUIRES lines[i] == "{". Collects body tokens until the matching "}"
// and advances i to the position AFTER the closing "}".
// Because normalizeLine() guarantees '{' is always standalone, the '{' is
// always exactly at position i — no forward scanning needed.
vector<string> getBlock(const vector<string>& lines, int& i) {
    vector<string> block;
    int n = (int)lines.size();

    if (i >= n || lines[i] != "{") {
        string got = (i < n) ? lines[i] : "<EOF>";
        throw runtime_error("Expected '{' but got: '" + got + "'");
    }

    int braces = 1;
    i++; // consume '{'

    while (i < n && braces > 0) {
        if (lines[i] == "{") braces++;
        if (lines[i] == "}") braces--;
        if (braces > 0) block.push_back(lines[i]);
        i++;
    }

    if (braces != 0)
        throw runtime_error("Unmatched '{' — missing closing '}'");

    return block;
}

// ================= EXECUTE LINE =================
// ONLY handles assignment statements. Control keywords must never reach here.
void executeLine(const string& line, unordered_map<string, int>& memory) {
    if (line.empty() || line == "{" || line == "}") return;

    if (isControlKeyword(line))
        throw runtime_error("Internal error: control keyword reached executeLine: " + line);

    if (line.back() != ';')
        throw runtime_error("Missing semicolon: " + line);

    string temp = line.substr(0, line.size() - 1);

    size_t eq = temp.find('=');
    if (eq == string::npos)
        throw runtime_error("Invalid statement (no '='): " + line);

    string left  = trim(temp.substr(0, eq));
    string right = trim(temp.substr(eq + 1));

    if (left.size() >= 4 && left.substr(0, 4) == "int ")
        left = trim(left.substr(4));

    if (left.empty())
        throw runtime_error("Invalid variable name in: " + line);
    if (isdigit((unsigned char)left[0]))
        throw runtime_error("Invalid variable name: " + left);

    memory[left] = evaluateExpression(right, memory);
}

// ================= FORWARD DECLARATION =================
void executeBlock(
    const vector<string>& lines, int start, int end,
    unordered_map<string, int>& memory,
    vector<crow::json::wvalue>& steps,
    int& globalLineCounter);

// ================= RECORD STEP =================
void recordStep(const string& code, unordered_map<string, int>& memory,
                vector<crow::json::wvalue>& steps, int& counter) {
    crow::json::wvalue step;
    step["line"] = ++counter;
    step["code"] = code;
    crow::json::wvalue mem;
    for (auto& kv : memory) mem[kv.first] = kv.second;
    step["memory"] = std::move(mem);
    steps.push_back(std::move(step));
}

// ================= EXECUTE BLOCK =================
// Core recursive interpreter. Handles all control flow by recursing into
// sub-blocks. executeLine() is called ONLY for plain assignment statements.
void executeBlock(
    const vector<string>& lines, int start, int end,
    unordered_map<string, int>& memory,
    vector<crow::json::wvalue>& steps,
    int& globalLineCounter)
{
    int i = start;
    while (i < end) {
        const string& line = lines[i];

        // ===== SKIP BARE BRACES =====
        if (line == "{" || line == "}") { i++; continue; }

        // ===== IF / ELSE =====
        if (line.size() >= 2 && line.substr(0, 2) == "if") {
            size_t s = line.find('(');
            size_t e = line.rfind(')');
            if (s == string::npos || e == string::npos || e <= s)
                throw runtime_error("Malformed if condition: " + line);
            string cond = line.substr(s + 1, e - s - 1);

            i++; // advance to '{' (guaranteed by normalizeLine)
            auto ifBody = getBlock(lines, i); // i now points past '}'

            vector<string> elseBody;
            if (i < end && trim(lines[i]) == "else") {
                i++; // consume 'else', now at '{'
                elseBody = getBlock(lines, i); // i now points past '}'
            }

            // Recursively execute the chosen branch
            bool condResult = evaluateCondition(cond, memory);
            const vector<string>& execBody = condResult ? ifBody : elseBody;
            executeBlock(execBody, 0, (int)execBody.size(), memory, steps, globalLineCounter);
            continue;
        }

        // ===== WHILE LOOP =====
        if (line.size() >= 5 && line.substr(0, 5) == "while") {
            size_t s = line.find('(');
            size_t e = line.rfind(')');
            if (s == string::npos || e == string::npos || e <= s)
                throw runtime_error("Malformed while condition: " + line);
            string cond = line.substr(s + 1, e - s - 1);

            i++; // advance to '{'
            auto whileBody = getBlock(lines, i); // i now points past '}'

            const int MAX_ITER = 10000;
            for (int iter = 0; evaluateCondition(cond, memory); iter++) {
                if (iter >= MAX_ITER)
                    throw runtime_error("Infinite loop detected (exceeded " + to_string(MAX_ITER) + " iterations)");
                // Recursively execute loop body — handles nested control flow
                executeBlock(whileBody, 0, (int)whileBody.size(), memory, steps, globalLineCounter);
            }
            continue;
        }

        // ===== FOR LOOP =====
        if (line.size() >= 3 && line.substr(0, 3) == "for") {
            size_t s = line.find('(');
            size_t e = line.rfind(')');
            if (s == string::npos || e == string::npos || e <= s)
                throw runtime_error("Malformed for loop: " + line);
            string inner = line.substr(s + 1, e - s - 1);

            vector<string> parts;
            stringstream fs(inner);
            string part;
            while (getline(fs, part, ';')) parts.push_back(trim(part));
            if (parts.size() != 3)
                throw runtime_error("For loop must have 3 parts (init; cond; update): " + line);

            string initPart   = parts[0] + ";";
            string condPart   = parts[1];
            string updatePart = parts[2] + ";";

            i++; // advance to '{'
            auto forBody = getBlock(lines, i); // i now points past '}'

            // Execute init (plain assignment only — validated by executeLine)
            executeLine(initPart, memory);

            const int MAX_ITER = 10000;
            for (int iter = 0; evaluateCondition(condPart, memory); iter++) {
                if (iter >= MAX_ITER)
                    throw runtime_error("Infinite loop detected in for loop (exceeded " + to_string(MAX_ITER) + " iterations)");
                // Recursively execute loop body — handles nested control flow
                executeBlock(forBody, 0, (int)forBody.size(), memory, steps, globalLineCounter);
                // Execute update (plain assignment only)
                executeLine(updatePart, memory);
            }
            continue;
        }

        // ===== NORMAL ASSIGNMENT STATEMENT =====
        // Only plain statements reach here — control keywords are fully handled above.
        executeLine(line, memory);
        recordStep(line, memory, steps, globalLineCounter);
        i++;
    }
}

// ================= MAIN =================
int main() {
    crow::SimpleApp app;

    CROW_ROUTE(app, "/")
    ([]() {
        ifstream file("../frontend/index.html");
        if (!file.is_open()) return string("<h1>Frontend not found</h1>");
        stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    });

    CROW_ROUTE(app, "/execute").methods("POST"_method)
    ([](const crow::request& req) {
        try {
            auto body = crow::json::load(req.body);
            if (!body) return crow::response(400, "{\"error\":\"Invalid JSON\"}");

            string code = body["code"].s();
            if (code.empty()) return crow::response(400, "{\"error\":\"Empty code\"}");

            auto lines = splitLines(code);
            unordered_map<string, int> memory;
            vector<crow::json::wvalue> steps;
            int globalLineCounter = 0;

            executeBlock(lines, 0, (int)lines.size(), memory, steps, globalLineCounter);

            crow::json::wvalue result;
            for (size_t i = 0; i < steps.size(); i++)
                result[i] = std::move(steps[i]);

            return crow::response(result.dump());

        } catch (const exception& e) {
            crow::json::wvalue error;
            error["error"] = string(e.what());
            return crow::response(400, error.dump());
        }
    });

    app.port(18080).multithreaded().run();
}