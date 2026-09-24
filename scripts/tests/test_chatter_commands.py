"""Test shipped command keywords against the real classifier, without server access."""
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
MODULE = ROOT / "modules/mod-playerbot-chatter"


class ChatterCommandTests(unittest.TestCase):
    def test_stats_with_template_and_fallback_keywords(self):
        config = (MODULE / "src/PBChatterConfig.cpp").read_text()
        fallback = config.split('GetOption<std::string>("PlayerbotChatter.CommandKeywords",', 1)[1].split(");", 1)[0]
        fallback = "".join(re.findall(r'"([^"]*)"', fallback))
        template = (MODULE / "conf/mod_playerbot_chatter.conf.dist").read_text()
        keywords = re.search(r'^PlayerbotChatter.CommandKeywords = "([^"]*)"', template, re.MULTILINE).group(1)
        source = (MODULE / "src/PBChatterClassifier.cpp").read_text()
        method = source[source.index("bool PBChatterClassifier::IsCommand("):
                        source.index("bool PBChatterClassifier::IsRealPlayerSender(")]
        harness = r'''
#include <algorithm>
#include <cassert>
#include <cctype>
#include <string>
#include <vector>
std::vector<std::string> g_PBChatCommandKeywords;
std::string Lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return value;
}
struct PBChatterClassifier { static bool IsCommand(std::string const&); };
''' + method + '\nint main() {\n'
        for values in (fallback, keywords):
            self.assertEqual(values.split(",").count("stats"), 1)
            harness += "g_PBChatCommandKeywords = {" + ",".join(map(json.dumps, values.split(","))) + "};\n"
            for text in ("stats", "  StAtS  ", "stats all", "pvp stats", "summon"):
                harness += f"assert(PBChatterClassifier::IsCommand({json.dumps(text)}));\n"
            for text in ("what stats should I prioritize?", "my stats are better", "statsman"):
                harness += f"assert(!PBChatterClassifier::IsCommand({json.dumps(text)}));\n"
        harness += 'g_PBChatCommandKeywords.clear();\n'
        for text in ("tank strategy", "tank strategy MT", "tank strategy offtank", "  TANK STRATEGY status"):
            harness += f"assert(PBChatterClassifier::IsCommand({json.dumps(text)}));\n"
        harness += 'assert(!PBChatterClassifier::IsCommand("tank strategyman"));\n'
        harness += "}\n"
        with tempfile.TemporaryDirectory() as directory:
            cpp = Path(directory) / "commands.cpp"
            binary = Path(directory) / "commands"
            cpp.write_text(harness)
            subprocess.run([os.environ.get("CXX", "g++"), "-std=c++20", "-Wall", "-Wextra", "-Werror",
                            str(cpp), "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True, timeout=10)


if __name__ == "__main__":
    unittest.main()
