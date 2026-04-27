#pragma once
#include <cppjieba/Jieba.hpp>
#include <set>
#include <string>

class KeyWordProcessor
{
  public:
    KeyWordProcessor();

    // chDir:中文语料库
    // enDir:英文语料库
    // 分析中文语料库和英文语料库
    void process(const std::string &chDir, const std::string &enDir);

  private:
    // 创建中文词典库
    void create_cn_dict(const std::string &dir, const std::string &outfile);

    // 创建中文索引库
    void build_cn_index(const std::string &dict, const std::string &index);

    // 创建英文词典库
    void create_en_dict(const std::string &dir, const std::string &outfile);

    // 创建英文索引库
    void build_en_index(const std::string &dict, const std::string &index);

  private:
    cppjieba::Jieba tokenizer_;
    std::set<std::string> enStopWords_;
    std::set<std::string> chStopWords_;
};