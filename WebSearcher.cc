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

    ifs.close();
    LOG_INFO("网页偏移库加载完成，共 {} 条", offsets_.size());
}

// 用偏移库定位，从网页库读取并解析一条 <doc>
WebSearcher::DocInfo WebSearcher::fetchDocument(int docId)
{
    DocInfo doc;
    auto it = offsets_.find(docId);
    if (it == offsets_.end())
        return doc;

    long long offset = it->second.first;
    long long length = it->second.second;

    // 定位到相应位置
    pagesIfs_.seekg(offset, ios::beg);

    string raw(length, '\0');
    pagesIfs_.read(&raw[0], length);

    // 解析XML结构
    auto extract = [&](const string &tag) -> string {
        string open = "<" + tag + ">";
        string close = "</" + tag + ">";
        auto s = raw.find(open);
        auto e = raw.find(close);
        if (s == string::npos || e == string::npos)
            return "";
        s += open.size();
        return raw.substr(s, e - s);
    };

    doc.id = docId;
    doc.link = extract("link");
    doc.title = extract("title");
    doc.content = extract("content");

    return doc;
}
int main()
{
    WebSearcher w;

    return 0;
}