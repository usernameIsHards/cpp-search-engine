#include "KeywordProcessor.h"
#include "DirectoryScanner.h"

#include <fstream>
#include <map>
#include <ostream>
#include <set>

using namespace std;
const string &EN_dir = "./data/corpus/EN/";
const string &CN_dir = "./data/corpus/CN/";
const string &stopwords_dir = "./data/stopwords/";

const string &dict_en_dir = "./outfile/";
const string &dict_en = "dict_en.dat";
const string &index = "index.dat";

const string &dict_ch_dir = "./outfile/";
const string &dict_ch = "dict_ch.dat";

KeyWordProcessor::KeyWordProcessor()
{
    vector<string> filenames = DirectoryScanner::scan(stopwords_dir);

    for (auto &filename : filenames)
    {
        ifstream ifs(filename);

        assert(ifs.is_open() && "ifs.open()");
        string word;
        if (filename.find("cn") != std::string::npos)
        {
            // 中文
            while (getline(ifs, word))
            {
                chStopWords_.emplace(word);
            }
        }
        else if (filename.find("en") != std::string::npos)
        {
            // 英文
            while (getline(ifs, word))
            {
                enStopWords_.emplace(word);
            }
        }

        ifs.close();
    }
}

// chDir:中文语料库
// enDir:英文语料库
// 分析中文语料库和英文语料库
void KeyWordProcessor::process(const std::string &chDir, const std::string &enDir)
{

    create_en_dict(enDir, dict_en);
    build_en_index(dict_en_dir, index);

    create_cn_dict(chDir, dict_ch);
    bulid_cn_index(dict_en_dir, index);
}

// 创建中文词典库
void KeyWordProcessor::create_cn_dict(const std::string &dir, const std::string &outfile)
{
    ofstream ofs(dict_ch_dir + outfile);
    const vector<string> &filenames = DirectoryScanner::scan(dir);
    map<string, long long> tokenFrequency;

    for (const auto &filename : filenames)
    {
        ifstream ifs(filename);

        assert(ifs.is_open() && "ifs.is_open()");

        // 读取汉字
        
    }
}

// 创建中文索引库
void KeyWordProcessor::bulid_cn_index(const std::string &dict, const std::string &index)
{
}

// 处理分词
string filterWord(const string &word)
{
    string res;
    for (char c : word)
    {
        if (isalpha(c))
        {
            res += tolower(c);
        }
    }
    return res;
}

// 创建英文词典库
void KeyWordProcessor::create_en_dict(const std::string &dir, const std::string &outfile)
{
    ofstream ofs(dict_en_dir + outfile); // 创建英文词典库

    const vector<string> &filenames = DirectoryScanner::scan(dir);

    map<string, long long> tokenFrequency;
    for (const auto &filename : filenames)
    {
        ifstream ifs(filename);

        assert(ifs.is_open() && "ifs.is_open()");

        string word;
        while (ifs >> word)
        {
            string new_word = filterWord(word);

            if (enStopWords_.find(new_word) != enStopWords_.end() || new_word.empty())
                continue;
            tokenFrequency[new_word]++;
        }
        ifs.close();
    }

    for (const auto &token : tokenFrequency)
    {
        ofs << token.first << " " << token.second << "\n";
    }

    ofs.close();
}

// 创建英文索引库       dict表示英文词典库路径
void KeyWordProcessor::build_en_index(const std::string &dict, const std::string &index)
{
    // 从dict_en.dat中读取数据
    ifstream ifs(dict + dict_en);
    // cout << dict_en_dir + dict_en << endl;
    assert(ifs.is_open() && "ifs.is_open()");

    // 写入到索引库
    ofstream ofs(dict_en_dir + index);
    //  cout << dict_en_dir + index << endl;
    map<char, set<int>> wordToIndices;

    int line = 1;
    // 初始化索引

    for (int i = 0; i < 26; i++)
    {

        wordToIndices['a' + i];
    }

    string word_line;
    while (getline(ifs, word_line))
    {
        for (const auto &ch : word_line)
        {
            if (ch < 'a' || ch > 'z')
                continue;

            wordToIndices[ch].insert(line);
        }
        line++;
    }

    for (const auto &entry : wordToIndices)
    {

        ofs << entry.first << " ";
        for (const auto pos : entry.second)
        {
            ofs << pos << " ";
        }
        ofs << "\n";
    }

    ifs.close();

    ofs.close();
}

int main()
{
    KeyWordProcessor k;
    k.process(CN_dir, EN_dir);

    return 0;
}