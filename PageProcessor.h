#pragma once
#include <string>
#include <vector>

#include <set>

#include "cppjieba/Jieba.hpp"
#include "simhash/Simhasher.hpp"

class PageProcessor
{
  public:
    PageProcessor();
    void process(const std::string &dir);

  private:
    void parseRss(const std::string &filename);
    // 解析dir目录下的xml文件，提取文档，放入documents中
    void extract_documents(const std::string &dir);

    // 依据SimHash算法对documents_去重
    void deduplicate_documents();

    // 构建网页库和网页偏移库
    void build_pages_and_offsets(const std::string &pages, const std::string &offsets);

    // 构建倒排索引库
    void build_inverted_index(const std::string &filename);

  private:
    struct Document
    {
        int id;
        std::string link;
        std::string title;
        std::string content;
    };

  private:
    cppjieba::Jieba tokenizer_;
    simhash::Simhasher hasher_;
    std::set<std::string> stopWords_; // 方便查找
    std::vector<Document> documents_;
    std::unordered_map<std::string, std::vector<std::pair<int, double>>> invertedIndex_;
};