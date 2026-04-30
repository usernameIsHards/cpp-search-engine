#pragma once
#include <cppjieba/Jieba.hpp>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

class WebSearcher
{
  public:
    WebSearcher();

    // 搜索入口，返回 JSON 字符串
    std::string search(const std::string &query, int topK = 10);

  private:
    // -------- 初始化 --------
    void loadStopWords();
    void loadInvertedIndex(const std::string &filename);
    void loadOffsets(const std::string &filename);

    // -------- 搜索核心 --------

    // 1. 对 query 分词 + TF-IDF 归一化，返回 {词->权重}，同时输出关键词列表
    std::unordered_map<std::string, double> computeQueryVector(const std::string &query,
                                                               std::vector<std::string> &outKeywords);

    // 2. 倒排索引求交集，找包含所有关键字的文档 id
    std::set<int> findCandidateDocs(const std::vector<std::string> &keywords);

    // 3. 余弦相似度（文档向量已归一化，直接点积）
    double cosineSimilarity(const std::unordered_map<std::string, double> &queryVec, int docId);

    // -------- 文档读取 --------
    struct DocInfo
    {
        int id = 0;
        std::string link;
        std::string title;
        std::string content;
        std::string abstract; // 生成后填入
    };

    // 用偏移库定位，从网页库读取并解析一条 <doc>
    DocInfo fetchDocument(int docId);

    // -------- 摘要 --------

    // 动态摘要：找关键字附近文字；失败则退化为静态摘要
    std::string generateAbstract(const std::string &content, const std::vector<std::string> &keywords,
                                 int maxLen = 160);

  private:
    // 倒排索引：关键字 → [(docId, weight)]（weight 已归一化）
    std::unordered_map<std::string, std::vector<std::pair<int, double>>> invertedIndex_;

    // 网页偏移库：docId → (offset, length)
    std::unordered_map<int, std::pair<long long, long long>> offsets_;

    std::set<std::string> stopWords_;
    cppjieba::Jieba tokenizer_;

    int totalDocs_ = 0;
    std::ifstream pagesIfs_;           // 打开一次，访问多次
    std::string pagesFile_ = "./outfile/webPage.dat";
};