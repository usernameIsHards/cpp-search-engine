#include "KeywordProcessor.h"
#include "DirectoryScanner.h"

#include <fstream>
#include <map>
#include <ostream>
#include <set>
#include <utfcpp/utf8.h>

#include "Logger.h"

using namespace std;
const string &EN_dir = "./data/corpus/EN/";
const string &CN_dir = "./data/corpus/CN/";
const string &stopwords_dir = "./data/stopwords/";

const string &dict_dir = "./outfile/";
const string &dict_en = "dict_en.dat";
const string &index_en_name = "index_en.dat";

const string &index_cn_name = "index_cn.dat";

const string &dict_ch = "dict_ch.dat";

KeyWordProcessor::KeyWordProcessor()
{
    LOG_INFO("初始化KeyWordProcessor");
    vector<string> filenames = DirectoryScanner::scan(stopwords_dir);
    LOG_INFO("扫描到{}个停用词文件", filenames.size());

    for (auto &filename : filenames)
    {
        ifstream ifs(filename);

        if (!ifs.is_open())
        {
            LOG_ERROR("停用词文件打开失败:{}", filename);
            continue;
        }

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

    LOG_INFO("停用词加载完成 中文:{}个 英文:{}个", chStopWords_.size(), enStopWords_.size());
}

// chDir:中文语料库
// enDir:英文语料库
// 分析中文语料库和英文语料库
void KeyWordProcessor::process(const std::string &chDir, const std::string &enDir)
{
    LOG_INFO("开始构建索引");

    LOG_INFO("构建英文词典库");
    create_en_dict(enDir, dict_en);

    LOG_INFO("构建英文索引库");
    build_en_index(dict_dir, index_en_name);

    LOG_INFO("构建中文词典库");
    create_cn_dict(chDir, dict_ch);

    LOG_INFO("构建中文索引库");
    build_cn_index(dict_dir, index_cn_name);

    LOG_INFO("索引构建完成");
}

// 提取汉字
string filterWord_ch(const string &word)
{
    string res;
    auto it = utf8::iterator<string::const_iterator>(word.begin(), word.begin(), word.end());
    auto end = utf8::iterator<string::const_iterator>(word.end(), word.begin(), word.end());

    for (; it != end; ++it)
    {
        if (*it >= 0x4E00 && *it <= 0x9FFF)
        {
            utf8::append(*it, back_inserter(res));
        }
    }

    return res;
}

// 创建中文词典库
void KeyWordProcessor::create_cn_dict(const std::string &dir, const std::string &outfile)
{
    LOG_INFO("创建中文词典: {}", dict_dir + outfile);

    ofstream ofs(dict_ch + outfile);

    if (!ofs.is_open())
    {
        LOG_ERROR("中文词典文件创建失败: {}", dict_dir + outfile);
        return;
    }

    const vector<string> &filenames = DirectoryScanner::scan(dir);
    LOG_INFO("中文语料库文件数量: {}", filenames.size());

    int fileCount = 0;
    map<string, long long> tokenFrequency;

    int wordCount = 0;
    for (const auto &filename : filenames)
    {
        ifstream ifs(filename);

        assert(ifs.is_open() && "ifs.is_open()");
        if (!ifs.is_open())
        {
            LOG_WARN("文件跳过(打开失败): {}", filename);
            continue;
        }

        string line; // 读取一行汉字
        while (getline(ifs, line))
        {
            // 使用jieba分词
            vector<string> words;
            tokenizer_.Cut(line, words);

            for (auto &word : words)
            {
                word = filterWord_ch(word);
                if (word.empty() || chStopWords_.find(word) != chStopWords_.end())
                    continue;
                if (utf8::distance(word.begin(), word.end()) < 2)
                    continue;
                tokenFrequency[word]++;
                wordCount++;
            }
        }

        ifs.close();
        fileCount++;
        LOG_DEBUG("  处理文件: {} 有效词: {} 个", filename, wordCount);
    }
    for (const auto &token : tokenFrequency)
    {
        ofs << token.first << " " << token.second << "\n";
    }

    ofs.close();

    LOG_INFO("中文词典完成 处理文件:{} 个 共 {} 个词", fileCount, tokenFrequency.size());
}

// 创建中文索引库
void KeyWordProcessor::build_cn_index(const std::string &dict, const std::string &index)
{
    LOG_INFO("构建中文索引: {}", dict + dict_ch);
    ifstream ifs(dict + dict_ch);

    // assert(ifs.is_open() && "ifs.is_open()");
    if (!ifs.is_open())
    {
        LOG_ERROR("中文索引文件创建失败: {}", dict + dict_ch);
        return;
    }
    ofstream ofs(dict_dir + index);

    if (!ofs.is_open())
    {
        LOG_ERROR("中文词典文件创建失败: {}", dict_dir + index);
        return;
    }

    map<string, set<int>> wordToIndices;

    int line = 1;
    // 初始化索引

    string word_line;
    while (getline(ifs, word_line))
    {

        const char *curr = word_line.c_str();
        const char *end = word_line.c_str() + word_line.size();

        while (curr != end)
        {
            auto start = curr;
            utf8::next(curr, end);

            string character = string(start, curr);
            character = filterWord_ch(character);
            if (character.empty())
                continue;

            wordToIndices[character].insert(line);
            // cout << character << " " << line << endl;
        }
        line++;
    }

    for (const auto &word : wordToIndices)
    {
        ofs << word.first << " ";
        for (const auto &pos : word.second)
        {
            ofs << pos << " ";
        }
        ofs << "\n";
    }
    ifs.close();

    ofs.close();

    LOG_INFO(" 中文索引完成 共 {} 个字符", wordToIndices.size());
}

// 处理分词
string filterWord_en(const string &word)
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
    LOG_INFO("创建英文词典库:{}", dict_dir + outfile);

    ofstream ofs(dict_dir + outfile); // 创建英文词典库

    if (!ofs.is_open())
    {
        LOG_ERROR("英文词典创建失败:{}", dict_dir + outfile);
        return;
    }

    const vector<string> &filenames = DirectoryScanner::scan(dir);

    LOG_INFO("英文语料库文件数量:{}", filenames.size());
    int fileCount = 0;
    map<string, long long> tokenFrequency;
    for (const auto &filename : filenames)
    {
        ifstream ifs(filename);

        if (!ifs.is_open())
        {
            LOG_WARN("打开文件失败(文件跳过):{}", filename);
            continue;
        }

        string word;
        int wordCount = 0;
        while (ifs >> word)
        {
            string new_word = filterWord_en(word);

            if (enStopWords_.find(new_word) != enStopWords_.end() || new_word.empty())
                continue;
            tokenFrequency[new_word]++;
            wordCount++;
        }
        ifs.close();
        fileCount++;

        LOG_DEBUG("  处理文件: {} 有效词: {} 个", filename, wordCount);
    }

    for (const auto &token : tokenFrequency)
    {
        ofs << token.first << " " << token.second << "\n";
    }

    ofs.close();

    LOG_INFO("英文词典完成 处理文件:{} 个 共 {} 个词", fileCount, tokenFrequency.size());
}

// 创建英文索引库       dict表示英文词典库路径
void KeyWordProcessor::build_en_index(const std::string &dict, const std::string &index)
{
    // 从dict_en.dat中读取数据
    ifstream ifs(dict + dict_en);
    LOG_INFO("构建英文索引:{}", dict + dict_en);

    // cout << dict_en_dir + dict_en << endl;
    // assert(ifs.is_open() && "ifs.is_open()");

    if (!ifs.is_open())
    {
        LOG_ERROR("英语词典打开失败:{}", dict + dict_en);
        return;
    }

    // 写入到索引库
    ofstream ofs(dict_dir + index);
    //  cout << dict_en_dir + index << endl;

    if (!ofs.is_open())
    {
        LOG_ERROR("英文索引文件创建失败: {}", dict_dir + index);
        return;
    }

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

    LOG_INFO("英文索引完成 共 {} 行", line - 1);
}