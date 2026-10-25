// Fake `claude` CLI for the AgentRun tests (T-401). Plain C++ (no Qt) so it starts fast.
// The real flags (-p --output-format stream-json --verbose ...) are accepted and ignored;
// behaviour is selected with --fake=<scenario> plus optional --fake-arg=<value>,
// --fake-chunk=<bytes>, --fake-exit=<code>, --fake-bytes=<n>, --fake-ignore-term.
//
// Scenarios:
//   replay      write the file named by --fake-arg to stdout (optionally in --fake-chunk
//               pieces), then exit with --fake-exit (default 0)
//   echo        print the prompt read from stdin, the arguments and the working directory
//   exit        print stderr text and exit with --fake-exit (default 3), no result line
//   hang        print the init line, then sleep for a minute
//   big         print --fake-bytes of assistant text in 1 KB lines, then a result line
//   bigline     like big but as one single line
//   garbage     print text that is not JSON, exit 0
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr const char* kInit =
    R"({"type":"system","subtype":"init","session_id":"fake-session","model":"fake-model"})";

constexpr const char* kResult =
    R"({"type":"result","subtype":"success","is_error":false,"duration_ms":5,"total_cost_usd":0.5,"session_id":"fake-session","result":"done"})";

void writeOut(const std::string& data)
{
    std::fwrite(data.data(), 1, data.size(), stdout);
    std::fflush(stdout);
}

void writeLine(const std::string& line)
{
    writeOut(line + "\n");
}

std::string jsonEscape(const std::string& in)
{
    std::string out;
    for (const char c : in) {
        switch (c) {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            out += c;
        }
    }
    return out;
}

std::string assistantText(const std::string& text)
{
    return R"({"type":"assistant","message":{"role":"assistant","content":[{"type":"text","text":")" +
           jsonEscape(text) + R"("}]},"session_id":"fake-session"})";
}

std::string readStdin()
{
    return {std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>()};
}

std::string optionValue(const std::vector<std::string>& args, const std::string& name,
                        const std::string& fallback = {})
{
    const std::string prefix = name + "=";
    for (const std::string& a : args) {
        if (a.starts_with(prefix)) {
            return a.substr(prefix.size());
        }
    }
    return fallback;
}

bool hasFlag(const std::vector<std::string>& args, const std::string& name)
{
    for (const std::string& a : args) {
        if (a == name) {
            return true;
        }
    }
    return false;
}

void sleepForever()
{
    for (;;) {
        std::this_thread::sleep_for(std::chrono::seconds(60));
    }
}

} // namespace

int main(int argc, char* argv[])
{
    const std::span<char*> raw(argv, static_cast<std::size_t>(argc));
    std::vector<std::string> args;
    for (std::size_t i = 1; i < raw.size(); ++i) {
        args.emplace_back(raw[i]);
    }
    const std::string scenario = optionValue(args, "--fake", "echo");

    if (hasFlag(args, "--fake-ignore-term")) {
        // SIG_IGN is a C macro expanding to a cast.
        std::signal(
            SIGTERM,
            SIG_IGN); // NOLINT(cppcoreguidelines-pro-type-cstyle-cast,performance-no-int-to-ptr)
    }

    const std::string prompt = readStdin();

    if (scenario == "replay") {
        std::ifstream in(optionValue(args, "--fake-arg"), std::ios::binary);
        const std::string data{std::istreambuf_iterator<char>(in),
                               std::istreambuf_iterator<char>()};
        const auto chunk =
            static_cast<std::size_t>(std::stoul(optionValue(args, "--fake-chunk", "0")));
        if (chunk == 0) {
            writeOut(data);
        } else {
            for (std::size_t pos = 0; pos < data.size(); pos += chunk) {
                writeOut(data.substr(pos, chunk));
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        return std::stoi(optionValue(args, "--fake-exit", "0"));
    }
    if (scenario == "echo") {
        std::ostringstream joined;
        for (const std::string& a : args) {
            joined << a << '|';
        }
        writeLine(kInit);
        writeLine(assistantText("prompt=" + prompt));
        writeLine(assistantText("args=" + joined.str()));
        writeLine(assistantText("cwd=" + std::filesystem::current_path().generic_string()));
        writeLine(kResult);
        return 0;
    }
    if (scenario == "exit") {
        std::fputs("fake stderr: something went wrong\n", stderr);
        std::fflush(stderr);
        return std::stoi(optionValue(args, "--fake-exit", "3"));
    }
    if (scenario == "hang") {
        writeLine(kInit);
        sleepForever();
    }
    if (scenario == "big" || scenario == "bigline") {
        const auto total =
            static_cast<std::size_t>(std::stoull(optionValue(args, "--fake-bytes", "1000")));
        writeLine(kInit);
        if (scenario == "bigline") {
            writeLine(assistantText(std::string(total, 'x')));
        } else {
            const std::string line = assistantText(std::string(1000, 'y'));
            for (std::size_t written = 0; written < total; written += line.size() + 1) {
                writeLine(line);
            }
        }
        writeLine(kResult);
        return 0;
    }
    if (scenario == "garbage") {
        writeLine("this is definitely not json");
        writeLine("{broken");
        return 0;
    }
    std::fputs("unknown fake scenario\n", stderr);
    return 64;
}
