#include "PBChatterClassifier.h"
#include "PBChatterConfig.h"

#include "gtest/gtest.h"

#include <string>
#include <utility>
#include <vector>

namespace
{
class ScopedCommandKeywords
{
public:
    explicit ScopedCommandKeywords(std::vector<std::string> keywords)
        : _old(g_PBChatCommandKeywords)
    {
        g_PBChatCommandKeywords = std::move(keywords);
    }

    ~ScopedCommandKeywords()
    {
        g_PBChatCommandKeywords = std::move(_old);
    }

private:
    std::vector<std::string> _old;
};
}

TEST(PBChatterClassifierTest, TreatsPlayerbotControlPrefixesAsCommands)
{
    ScopedCommandKeywords keywords({});

    EXPECT_TRUE(PBChatterClassifier::IsCommand(""));
    EXPECT_TRUE(PBChatterClassifier::IsCommand("   "));
    EXPECT_TRUE(PBChatterClassifier::IsCommand(".raidroster 10"));
    EXPECT_TRUE(PBChatterClassifier::IsCommand(" +co "));
    EXPECT_TRUE(PBChatterClassifier::IsCommand("-stay"));
    EXPECT_TRUE(PBChatterClassifier::IsCommand("!"));
    EXPECT_TRUE(PBChatterClassifier::IsCommand("#stats"));
    EXPECT_TRUE(PBChatterClassifier::IsCommand("@tank attack my target"));
}

TEST(PBChatterClassifierTest, TreatsConfiguredFirstWordKeywordsAsCommands)
{
    ScopedCommandKeywords keywords({ "summon", "stats", "quests all" });

    EXPECT_TRUE(PBChatterClassifier::IsCommand("summon"));
    EXPECT_TRUE(PBChatterClassifier::IsCommand("summon please"));
    EXPECT_TRUE(PBChatterClassifier::IsCommand("  stats"));
    EXPECT_TRUE(PBChatterClassifier::IsCommand("quests all"));
    EXPECT_TRUE(PBChatterClassifier::IsCommand("quests all incomplete"));

    EXPECT_FALSE(PBChatterClassifier::IsCommand("can someone summon me?"));
    EXPECT_FALSE(PBChatterClassifier::IsCommand("that pull was clean"));
}

TEST(PBChatterClassifierTest, RecognizesLikelyQuestionsWithoutTreatingEveryMentionAsQuestion)
{
    EXPECT_TRUE(PBChatterClassifier::IsLikelyQuestion("where is the scarlet key"));
    EXPECT_TRUE(PBChatterClassifier::IsLikelyQuestion("What's next?"));
    EXPECT_TRUE(PBChatterClassifier::IsLikelyQuestion("how do I hide cloak"));
    EXPECT_TRUE(PBChatterClassifier::IsLikelyQuestion("we run cathedral now?"));

    EXPECT_FALSE(PBChatterClassifier::IsLikelyQuestion("I wonder where Doan went"));
    EXPECT_FALSE(PBChatterClassifier::IsLikelyQuestion("nice pull"));
    EXPECT_FALSE(PBChatterClassifier::IsLikelyQuestion("   "));
}
