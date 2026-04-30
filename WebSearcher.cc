#include "WebSearcher.h"
#include "Logger.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <utfcpp/utf8.h>

using namespace std;

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
    fetchDocument(1);

    vector<std::string> outKeywords;
    computeQueryVector(
        "在步入“金九银十”，传统家居行业迎来销售旺季之际，东阳市积极推进促进消费工作，响应“浙里来消费·2019金秋购物节”活"
        "动，将于本月至11月推出第十一届休闲购物节。 "
        "作为其中的一项重要活动，第二届东作红木文化艺术节将于9月29日启幕，并持续至11月20日，为消费者带来一场可观、可赏"
        "、可购、可玩的中式文化盛宴。 "
        "据悉，第二届东作红木文化艺术节由浙江省家具协会等单位主办，东阳市红木家具协会、东阳红木家具市场、水墨会承办。活"
        "动地点位于东阳市世贸大道599号的东阳红木家具市场。 "
        "在上届成功举办的基础上，本届艺术节将从层级到规模，再到内容进行全面优化提升。活动内容丰富、形式多样，旨在探讨新"
        "经济新常态下中国传统家具的发展趋势与未来，发扬和传承中国红木文化，让红木文化更加深入民心。名家书画•红木艺术融"
        "合展和周逢俊周逢刚水墨情思与文人空间展将贯穿本届东作红木文化艺术节始终。 "
        "本次艺术节还将开展古风国韵——国庆游园活动、爱心助学公益活动、“国摄天香”摄影大赛活动等，推出599号超级免单节给消"
        "费者实实在在让利，名企让利、名师赠画等一系列活动也将同期举行。 "
        "东阳市红木家具行业协会常务副会长、东阳红木家具市场总经理曹伟表示，东阳红木产业将以本次艺术节为契机，集中宣传、"
        "集中爆破，炮制一场中国传统文化的盛宴，向各地的消费者宣传展示东阳红木行业的发展实力，促进和拉动东阳红木商贸的发"
        "展，提高东阳红木的知名度和美誉度。（房晓雯）",
        outKeywords);
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

    // 收集关键词列表
    for (const auto &[word, _] : tf)
        outKeywords.push_back(word);

    int total = 0; // 文档的总词数
    for (const auto &[_, cnt] : tf)
        total += cnt;

    if (total == 0)
        return {};

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

    LOG_INFO("处理分词完成");
    return queryVec;
}

int main()
{
    WebSearcher w;

    return 0;
}