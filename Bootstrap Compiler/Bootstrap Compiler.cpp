#include <iostream>
#include <filesystem>
#include <string>
#include <sstream>
#include <vector>
#include <fstream>
#include <chrono>
#include <algorithm>
#include <format>

// Error class
class Error {
public:
    std::string message;
};

// Entity structure
struct Entity {
    std::string command;
    std::vector<std::string> args;
};

// Bootstrap AST
class BAST {
public:
    std::vector<std::string> Imports;
    std::vector<Entity> Entities;
    std::vector<std::string> References;
    std::vector<std::string> Details;
    std::vector<std::string> Raw;
};

// Split raw text into lines
std::vector<std::string> split_lines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream iss(text);
    std::string line;
    while (std::getline(iss, line)) lines.push_back(line);
    return lines;
}

// Get Unix timestamp
int get_time() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
}

// Parse a single entity line into structured Entity
Entity parse_entity_line(const std::string& line) {
    Entity ent;
    auto cmd_pos = line.find("??");
    if (cmd_pos != std::string::npos) {
        ent.command = line.substr(0, cmd_pos);
        std::string rest = line.substr(cmd_pos + 2);
        if (!rest.empty() && rest.front() == '(' && rest.back() == ')') {
            rest = rest.substr(1, rest.size() - 2);
        }
        std::stringstream ss(rest);
        std::string token;
        while (std::getline(ss, token, ',')) {
            token.erase(0, token.find_first_not_of(" \t\n\r"));
            token.erase(token.find_last_not_of(" \t\n\r") + 1);
            if (!token.empty()) ent.args.push_back(token);
        }
    }
    else {
        ent.command = line;
    }
    return ent;
}

// Parse all entities between #start and #end
std::vector<Entity> parse_entities_structured(const std::vector<std::string>& lines) {
    std::vector<Entity> entities;
    bool inside_program = false;
    for (const auto& line : lines) {
        if (line == "#start") { inside_program = true; continue; }
        if (line == "#end") { inside_program = false; continue; }
        if (inside_program && !line.empty()) {
            entities.push_back(parse_entity_line(line));
        }
    }
    return entities;
}

// Parse imports (#import lines, remove duplicates)
void parse_imports(const std::vector<std::string>& lines, BAST& bast) {
    for (const auto& line : lines) {
        if (line.starts_with("#import ")) {
            std::string lib = line.substr(8);
            if (std::find(bast.Imports.begin(), bast.Imports.end(), lib) == bast.Imports.end())
                bast.Imports.push_back(lib);
        }
    }
}

// Parse references (#start/#end lines + bootstrap metadata)
void parse_references(const std::vector<std::string>& lines, BAST& bast) {
    int start_line = -1, end_line = -1;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (lines[i] == "#start") start_line = i + 1;
        if (lines[i] == "#end") end_line = i + 1;
    }
    bast.References.push_back(std::format("start:{};", start_line));
    bast.References.push_back(std::format("end:{};", end_line));
    bast.References.push_back(std::format("endcode:{};", end_line));
    bast.References.push_back("bootstrapver:b26;");
    bast.References.push_back("bootstraprqcomp:b26c;");
    bast.References.push_back("bootstrapast:b26bast;");
}

// Compiler namespace
namespace Compiler {
    int compile(std::string fileloc, std::string o = "main") {
        Error e;
        std::ifstream file(fileloc, std::ios::in);
        if (!file) {
            std::cout << "b26c=1\nfile-opened: 0\n";
            return 1;
        }

        try {
            int start_time = get_time();
            std::stringstream buffer;
            buffer << file.rdbuf();
            std::string content = buffer.str();
            std::vector<std::string> filelines = split_lines(content);

            BAST BAST26;
            BAST26.Raw = filelines;

            parse_imports(filelines, BAST26);
            parse_references(filelines, BAST26);
            BAST26.Entities = parse_entities_structured(filelines);

            BAST26.Details = {
                std::format("projectname={}", fileloc),
                std::format("compile-start:{}", start_time),
                std::format("num-entities:{}", BAST26.Entities.size())
            };

            std::ofstream debugFile(o + ".btspdebug");
            debugFile << ";;details\n";
            for (const auto& line : BAST26.Details) debugFile << line << '\n';

            debugFile << ";;raw\n";
            for (const auto& line : BAST26.Raw) debugFile << line << '\n';

            debugFile << ";;imports\n";
            for (const auto& line : BAST26.Imports) debugFile << line << '\n';

            debugFile << ";;entities\n";
            for (const auto& ent : BAST26.Entities) {
                debugFile << ent.command;
                if (!ent.args.empty()) {
                    debugFile << " ?? (";
                    for (size_t i = 0; i < ent.args.size(); ++i) {
                        debugFile << ent.args[i];
                        if (i + 1 < ent.args.size()) debugFile << ", ";
                    }
                    debugFile << ")";
                }
                debugFile << ";\n";
            }

            debugFile << ";;references\n";
            for (const auto& line : BAST26.References) debugFile << line << '\n';

        }
        catch (const std::exception& ex) {
            e.message = ex.what();
            std::cout << "b26c=1\nerror: " << e.message << std::endl;
            return 1;
        }
        catch (...) {
            std::cout << "b26c=1\nerror: ?" << std::endl;
            return 1;
        }

        return 0;
    }
}

// Main CLI
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "b26c expected 2 or more arguments, instead got " << argc << "." << std::endl;
        return 1;
    }

    if (std::string(argv[1]) == "build") {
        bool build_properties_valid = (argc == 3);
        bool build_path_valid = (std::filesystem::exists(argv[2]));
        bool build_path_suffix_valid = (std::string(argv[2]).ends_with(".btsp"));

        if (!build_properties_valid || !build_path_valid || !build_path_suffix_valid) {
            std::cout << "b26c=1:\n"
                << "build-properties-valid: " << build_properties_valid << "\n"
                << "build-path-valid: " << build_path_valid << " (" << std::filesystem::absolute(argv[2]) << ")\n"
                << "build-path-suffix-valid: " << build_path_suffix_valid << std::endl;
            return 1;
        }
        else if (build_properties_valid && build_path_valid && build_path_suffix_valid) {
            Compiler::compile(argv[2]);
        }
    }

    return 0;
}
