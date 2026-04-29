#pragma once
#include <cppjieba/Jieba.hpp>
#include <map>
#include <set>
#include <string>
#include <vector>

class KeywordRecommender
{
  public:
    KeywordRecommender();

    void process();

    // 返回Json格式的推荐词列表
    std::string recommend(const std::string &keyword, int topK = 5);

  private:
    // 从索引库获取候选词行号集合
    std::set<int> getCandidateLines(const std::string &keyword);

    // 计算编辑距离
    int editDistance(const std::string &s1, const std::string &s2);

  private:
    // 词典: word -》 frequency
    std::vector<std::pair<std::string, long long>> cnDict_;

    // 索引：character ->{行号集合}
    std::map<std::string, std::set<int>> cnIndex_;
};