#include <bitset>
#include <ios>
#include <iostream>
#include <simhash/Simhasher.hpp>
#include <string>

using namespace std;
using namespace simhash;

Simhasher hasher;

int hamming_distance(uint64_t x, uint64_t y)
{
    int distance = 0;
    uint64_t z = x ^ y;
    while (z)
    {
        z &= (z - 1);
        distance++;
    }
    return distance;
}

void print_similarity(const string &text1, const string &text2)
{
    uint64_t h1, h2;
    int topN = 5;
    hasher.make(text1, topN, h1);
    hasher.make(text2, topN, h2);

    cout << bitset<64>(h1) << endl;
    cout << bitset<64>(h2) << endl;

    cout << "海明距离: " << hamming_distance(h1, h2) << endl;

    cout << "是否相等: " << boolalpha << Simhasher::isEqual(h1, h2) << endl;
}

int main()
{

    string doc1 = "机器学习是人工智能的一个分支领域";
    string doc2 = "机器学习是人工智能领域的重要分支";
    print_similarity(doc1, doc2);

    string doc4 = "机器学习是人工智能的一个分支领域";
    string doc5 = "今天天气很好适合出门散步运动健康";
    print_similarity(doc4, doc5);

    return 0;
}