#include "KeywordRecommender.h"
#include "Logger.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <utfcpp/utf8.h>

using namespace nlohmann;

using namespace std;
void KeywordRecommender::process()
{
}

// 加载词典 && 加载索引
KeywordRecommender::KeywordRecommender()
{
    LOG_INFO("=============构造KeywordRecommender关键词推荐器============");
    // 加载词典
    ifstream ifs_dict("./outfile/dict_ch.dat");
    if (!ifs_dict.is_open())
    {
        LOG_ERROR("加载词典库失败:{}", "./outfile/dict_ch.dat");
        return;
    }

    string line;
    int total_lines = 0;

    cnDict_.push_back({"", 0});

    while (getline(ifs_dict, line))
    {
        total_lines++;
        if (line.empty())
            continue;
        istringstream iss(line);
        long long freq;
        string word;
        if (!(iss >> word >> freq))
        {
            LOG_WARN("词典格式错误，跳过该行: {}", line);
            continue;
        }

        cnDict_.push_back({word, freq});
    }
    ifs_dict.close();

    LOG_INFO("词典加载完成，共 {} 条,总行数:{}", cnDict_.size(), total_lines);

    // 加载索引
    LOG_INFO("加载索引库before");
    ifstream ifs_index("./outfile/index_cn.dat");
    if (!ifs_index.is_open())
    {
        LOG_ERROR("加载索引库失败:{}", "./outfile/index_cn.dat");
        return;
    }
    LOG_INFO("加载索引库after");

    total_lines = 0;
    while (getline(ifs_index, line))
    {
        ++total_lines;

        if (line.empty())
            continue;

        // 按空格分割字符串 → 优先用 istringstream！
        istringstream iss(line);
        string word;

        iss >> word;
        int line_index;

        while (iss >> line_index)
        {
            cnIndex_[word].emplace(line_index);
        }
    }
    ifs_index.close();
    LOG_INFO("词典加载完成，共 {} 条,总行数:{}", cnIndex_.size(), total_lines);
    LOG_INFO("=============构造KeywordRecommender关键词推荐器已完成============");
}

// 从索引库获取候选词行号集合
std::set<int> KeywordRecommender::getCandidateLines(const std::string &keyword)
{
    set<int> result;

    const char *curr = keyword.c_str();
    const char *end = keyword.c_str() + keyword.size();

    while (curr != end)
    {
        auto start = curr;
        // 将 it 移动到下一个 utf8 字符所在的位置
        utf8::next(curr, end);
        // 一个汉字会占用多个字节，因此需要用 string 来表示一个汉字
        string character = string(start, curr);

        auto it = cnIndex_.find(character);
        if (it == cnIndex_.end())
        {
            LOG_WARN("字符'{}'不在索引库中", character);
            continue;
        }

        for (int lineNo : it->second)
        {
            result.insert(lineNo);
        }
    }

    LOG_DEBUG("合并后候选行号总数: {}", result.size());
    return result;
}

// 返回Json格式的推荐词列表
std::string KeywordRecommender::recommend(const std::string &keyword, int topK)
{

    LOG_INFO("开始推荐, keyword={}, topK={}", keyword, topK);
    // 获取候选词行号集合
    set<int> candidateLines = getCandidateLines(keyword);

    LOG_INFO("候选行号数量: {}", candidateLines.size());

    if (candidateLines.empty())
    {
        LOG_WARN("关键字:{}没有找到候选行号", keyword);
        return "[]";
    }

    // 根据行号从字典库取出候选词
    //  {编辑距离，词频，词语}  大根堆
    using Tuple = tuple<int, long long, string>;
    priority_queue<Tuple> pq;

    for (int lineNo : candidateLines)
    {

        if (lineNo <= 0 || lineNo >= static_cast<int>(cnDict_.size()))
            continue;

        auto &[word, freq] = cnDict_[lineNo];

        // 计算编辑距离
        int dist = editDistance(keyword, word);

        // 词频取负，距离相同时词频越大越靠前
        pq.push({dist, -freq, word});

        if (static_cast<int>(pq.size()) > topK)
        {
            pq.pop();
        }
    }
    // 取出结果(逆序)
    vector<string> result;
    while (!pq.empty())
    {
        // 取出单词本身
        result.push_back(get<2>(pq.top()));
        pq.pop();
    }
    reverse(result.begin(), result.end());

    string jsonResult = json(result).dump();
    LOG_INFO("推荐结果: keyword={}, result={}", keyword, jsonResult);

    return jsonResult;
}

// 计算编辑距离
int KeywordRecommender::editDistance(const std::string &s1, const std::string &s2)
{
    vector<string> chars1, chars2;

    auto toChars = [](const string &s, vector<string> &chars) {
        const char *curr = s.c_str();
        const char *end = s.c_str() + s.size();
        while (curr != end)
        {
            const char *start = curr;
            utf8::next(curr, end);
            chars.push_back(string(start, curr));
        }
    };

    toChars(s1, chars1);
    toChars(s2, chars2);

    int m = chars1.size();
    int n = chars2.size();

    vector<vector<int>> dp(m + 1, vector<int>(n + 1, 0));

    for (int i = 0; i <= m; i++)
        dp[i][0] = i;
    for (int j = 0; j <= n; j++)
        dp[0][j] = j;

    for (int i = 1; i <= m; i++)
    {
        for (int j = 1; j <= n; j++)
        {
            if (chars1[i - 1] == chars2[j - 1])
            {
                dp[i][j] = dp[i - 1][j - 1];
            }
            else
            {
                dp[i][j] = min({dp[i - 1][j] + 1, dp[i][j - 1] + 1, dp[i - 1][j - 1] + 1});
            }
        }
    }
    return dp[m][n];
}

#if TESTRECOM
int main()
{
    // ① 构造推荐器（自动加载词典和索引）
    KeywordRecommender recommender;

    // ② 测试关键字列表
    vector<string> testKeywords = {
        "艺",   // 词频最高 1197，命中最多
        "文化", // 词频 521
        "文学", // 词频 307
        "精神", // 词频 182
        "中国"  // 专有名词，易验证
    };

    // ③ 逐个测试
    for (auto &keyword : testKeywords)
    {
        cout << "==============================" << endl;
        cout << "输入关键字: " << keyword << endl;

        string result = recommender.recommend(keyword, 5);

        cout << "推荐结果: " << result << endl;
    }

    cout << "==============================" << endl;
    return 0;
}
#endif