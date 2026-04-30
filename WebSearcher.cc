#include "WebSearcher.h"
#include "Logger.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <utfcpp/utf8.h>

using namespace std;
using namespace nlohmann;
WebSearcher::WebSearcher()
{
    LOG_INFO("==============构造网页搜索器开始=================");
    // 加载 倒排索引库 网页库 网页偏移库
    loadStopWords();
    loadInvertedIndex("./outfile/invertIndex.dat");
    loadOffsets("./outfile/offsetLib.dat");

    pagesIfs_.open(pagesFile_);
    if (!pagesIfs_.is_open())
        LOG_ERROR("网页库打开失败: {}", pagesFile_);
}

// 加载停用词
void WebSearcher::loadStopWords()
{
    ifstream ifs("./data/stopwords/cn_stopwords.txt");
    if (!ifs.is_open())
    {
        LOG_ERROR("停用词加载失败：{}", "./data/stopwords/cn_stopwords.txt");
        return;
    }
    string line;
    while (getline(ifs, line))
    {
        istringstream iss(line);
        string word;
        iss >> word;
        stopWords_.emplace(word);
    }

    ifs.close();
    LOG_INFO("停用词加载完成，共 {} 个", stopWords_.size());
}

// 加载 倒排索引库
void WebSearcher::loadInvertedIndex(const std::string &filename)
{
    ifstream ifs(filename);

    if (!ifs.is_open())
    {
        LOG_ERROR("倒排索引库加载失败");
        return;
    }

    string line;
    int total_lines = 0;
    while (getline(ifs, line))
    {

        istringstream iss(line);
        // 获取关键字
        string word;
        iss >> word;
        total_lines++;
        int docId;
        double weight;
        while (iss >> docId >> weight)
        {
            invertedIndex_[word].push_back({docId, weight});
        }
    }
    ifs.close();

    LOG_INFO("倒排索引库加载完成，共 {} 条,总行数:{}", invertedIndex_.size(), total_lines);
}

// 加载网页库

// 加载网页偏移库
void WebSearcher::loadOffsets(const std::string &filename)
{
    ifstream ifs(filename);
    if (!ifs.is_open())
    {
        LOG_ERROR("加载网页偏移库失败:{}", filename);
        return;
    }

    string line;
    while (getline(ifs, line))
    {
        istringstream iss(line);

        int docId;
        long long offset;
        long long length;
        iss >> docId >> offset >> length;
        offsets_[docId] = {offset, length};
    }
    totalDocs_ = offsets_.size();
    ifs.close();
    LOG_INFO("网页偏移库加载完成，共 {} 条", offsets_.size());
}

// 用偏移库定位，从网页库读取并解析一条 <doc>
WebSearcher::DocInfo WebSearcher::fetchDocument(int docId)
{
    DocInfo doc;
    auto it = offsets_.find(docId);
    if (it == offsets_.end())
    {
        return doc;
    }

    long long offset = it->second.first;
    long long length = it->second.second;

    pagesIfs_.seekg(offset, ios::beg);

    string raw(length, '\0');

    pagesIfs_.read(&raw[0], length);

    // 解析xml结构
    auto extract = [&](const string &tag) -> string {
        string open = "<" + tag + ">";
        string close = "</" + tag + ">";

        auto s = raw.find(open);
        auto e = raw.find(close);
        if (s == string::npos || e == string::npos)
        {
            return "";
        }

        s += open.size();
        return raw.substr(s, e - s);
    };

    doc.id = docId;
    doc.link = extract("link");
    doc.title = extract("title");
    doc.content = extract("content");

    return doc;
}

// 1. 对 query 分词 + TF-IDF 归一化，返回 {词->权重}，同时输出关键词列表
unordered_map<string, double> WebSearcher::computeQueryVector(const string &query, vector<string> &outKeywords)
{
    vector<string> words;
    tokenizer_.CutForSearch(query, words, true);

    // 统计词频
    unordered_map<string, int> tf;

    for (auto &word : words)
    {
        if (word.empty() || word == " ")
            continue;
        if (stopWords_.count(word))
            continue;
        if (word.size() < 2)
            continue;
        if (word.size() < 4 && (unsigned char)word[0] >= 0x80)
            continue;
        if (!invertedIndex_.count(word)) // 索引里没有的词直接跳过
            continue;
        tf[word]++;
    }

    LOG_INFO("query分词原始数量:{}, 过滤后关键词数量:{}", words.size(), tf.size());

    // 收集关键词列表
    for (const auto &[word, _] : tf)
        outKeywords.push_back(word);

    int total = 0; // 文档的总词数
    for (const auto &[_, cnt] : tf)
        total += cnt;

    if (total == 0)
    {
        LOG_WARN("query[{}] 无有效关键词，返回空向量", query);
        return {};
    }

    int N = totalDocs_;

    // 计算 TF-IDF
    unordered_map<string, double> queryVec;
    for (const auto &[word, cnt] : tf)
    {
        double tfVal = double(cnt) / total;
        double df = invertedIndex_.at(word).size();

        double idf = log((double)N / (df + 1));
        queryVec[word] = tfVal * idf;
    }

    // 归一化
    double norm = 0.0;
    for (const auto &[_, w] : queryVec)
        norm += w * w;

    norm = sqrt(norm);

    if (norm > 0.0)
    {
        for (auto &[_, w] : queryVec)
        {
            w /= norm;
        }
    }

    LOG_INFO("query[{}] 向量计算完成, 关键词:{}, norm:{:.4f}", query, outKeywords.size(), norm);
    return queryVec;
}

// 2. 倒排索引求交集，找包含所有关键字的文档 id
set<int> WebSearcher::findCandidateDocs(const vector<string> &keywords)
{
    if (keywords.empty())
        return {};

    // 用第一个关键词的文档列表初始化结果集
    set<int> result;
    for (const auto &[docId, weight] : invertedIndex_.at(keywords[0]))
        result.insert(docId);

    // 逐个关键字求交集
    for (int i = 1; i < keywords.size(); i++)
    {
        const auto &list = invertedIndex_.at(keywords[i]);
        set<int> cur;

        for (const auto &[docId, _] : list)
            cur.insert(docId);

        set<int> inter;
        set_intersection(result.begin(), result.end(), cur.begin(), cur.end(), inserter(inter, inter.begin()));
        result = move(inter);

        if (result.empty())
        {
            LOG_WARN("关键词交集为空，无匹配文档");
            return {};
        }
    }
    LOG_INFO("候选文档数量: {}", result.size());
    return result;
}

double WebSearcher::cosineSimilarity(const unordered_map<string, double> &queryVec, int docId)
{
    double score = 0.0;

    for (const auto &[word, qWeight] : queryVec)
    {
        auto it = invertedIndex_.find(word);
        if (it == invertedIndex_.end())
            continue;

        for (const auto &[id, dWeight] : it->second)
        {
            // 在该词的倒排列表里找 docId 对应的权重
            if (id == docId)
            {
                score += qWeight * dWeight;
                break;
            }
        }
    }
    return score;
}

// 在 content 中找第一个关键词出现的位置，截取其前后共 maxLen 个字符的窗口。失败则退化为静态摘要（content 前 maxLen
// 字符）。没搞懂
string WebSearcher::generateAbstract(const string &content, const vector<string> &keywords, int maxLen)
{
    // 动态摘要:找到第一个关键词的位置
    size_t hitPos = string::npos;
    for (const auto &kw : keywords)
    {
        hitPos = content.find(kw);
        if (hitPos != string::npos)
            break;
    }

    // 确定截取起点(字节位置),往前回退一半窗口
    size_t start = 0;
    if (hitPos != string::npos)
    {
        start = hitPos > (size_t)(maxLen * 3 / 2) ? hitPos - maxLen * 3 / 2 : 0;
        // 对齐到合法UTF-8字符边界
        while (start > 0 && (content[start] & 0xc0) == 0x80)
            --start;
    }

    // 按UTF-8字符截取maxLen个字符
    const char *curr = content.c_str() + start;
    const char *end = content.c_str() + content.size();
    int count = 0;
    const char *cut = curr;
    while (curr != end && count < maxLen)
    {
        cut = curr;
        utf8::next(curr, end);
        ++count;
    }
    return content.substr(start, curr - (content.c_str() + start));
}

string WebSearcher::search(const string &query, int topK)
{
    // 1.计算query向量
    vector<string> keywords;
    auto queryVec = computeQueryVector(query, keywords);
    if (queryVec.empty())
    {
        return "[]";
    }

    // 2.倒排索引求候选文档(包含所有关键词的交集)
    auto candidates = findCandidateDocs(keywords);
    if (candidates.empty())
    {
        LOG_WARN("无候选文档");
        return "[]";
    }

    // 3.余弦相似度打分，最小堆保留topK
    using ScoreDoc = pair<double, int>; //(score,docId)
    priority_queue<ScoreDoc, vector<ScoreDoc>, greater<ScoreDoc>> minHeap;

    for (int docId : candidates)
    {
        double score = cosineSimilarity(queryVec, docId);
        if ((int)minHeap.size() < topK)
        {
            minHeap.push({score, docId});
        }
        else if (score > minHeap.top().first)
        {
            minHeap.pop();
            minHeap.push({score, docId});
        }
    }

    // 4. 取出并降序排列
    std::vector<ScoreDoc> results;
    while (!minHeap.empty())
    {
        results.push_back(minHeap.top());
        minHeap.pop();
    }
    std::sort(results.rbegin(), results.rend());

    // 5.拼装JSON
    json arr = json::array();
    for (auto &[score, docId] : results)
    {
        DocInfo doc = fetchDocument(docId);
        doc.abstract = generateAbstract(doc.content, keywords);
        arr.push_back({{"id", doc.id}, {"title", doc.title}, {"link", doc.link}, {"abstract", doc.abstract}});
    }
    LOG_INFO("search 完成，返回 {} 条结果", arr.size());
    return arr.dump();
}

#ifdef SEARCH
int main()
{
    WebSearcher w;

    return 0;
}
#endif