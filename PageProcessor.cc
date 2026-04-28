#include "PageProcessor.h"
#include "DirectoryScanner.h"
#include "KeywordProcessor.h"
#include "Logger.h"
#include <iostream>
#include <regex>
#include <set>
#include <tinyxml2.h>
#include <utfcpp/utf8.h>

using namespace std;
using namespace tinyxml2;

extern const string &stopwords_dir;

PageProcessor::PageProcessor() : tokenizer_()
{
    LOG_INFO("初始化PageProcessor");
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
                stopWords_.emplace(word);
            }
        }
        else if (filename.find("en") != std::string::npos)
        {
            // 英文
            while (getline(ifs, word))
            {
                stopWords_.emplace(word);
            }
        }

        ifs.close();
    }

    LOG_INFO("停用词加载完成 :{}个", stopWords_.size());
}

void PageProcessor::process(const string &dir)
{
    extract_documents(dir);
    deduplicate_documents();

    build_pages_and_offsets("./outfile/webPage.dat", "./outfile/offsetLib.dat");
    build_inverted_index("./outfile/invertIndex.dat");
}

// 解码HTML实体
string decodeHtmlEntities(const string &str)
{
    string res = str;

    // 顺序很重要！&amp; 必须最后处理，否则会二次替换
    struct
    {
        const char *entity;
        const char *ch;
    } entities[] = {
        {"&nbsp;", " "}, {"&lt;", "<"},  {"&gt;", ">"},  {"&quot;", "\""},
        {"&apos;", "'"}, {"&#39;", "'"}, {"&amp;", "&"}, // ← 最后处理！
    };

    for (const auto &e : entities)
    {
        string::size_type pos = 0;
        while ((pos = res.find(e.entity, pos)) != string::npos)
        {
            res.replace(pos, strlen(e.entity), e.ch);
            pos += strlen(e.ch);
        }
    }
    return res;
}
// 辅助函数  去除HTML标签
static string clearHtmlTag(const string &str)
{
    if (str.empty())
    {
        return "";
    }
    // 第一步：去除HTML标签
    regex htmlReg("<[^>]+>");

    string res = regex_replace(str, htmlReg, "");

    //  第二步：解码HTML实体
    res = decodeHtmlEntities(res);

    //  第三步：去除多余空白
    regex spaceReg("\\s+");
    res = regex_replace(res, spaceReg, " ");

    //  去掉首尾空格
    size_t start = res.find_first_not_of(" ");
    size_t end = res.find_last_not_of(" ");
    if (start == string::npos)
        return "";

    return res.substr(start, end - start + 1);
}

// 辅助函数：获取XML结点文本，处理空节点
static string getNodeText(XMLElement *node)
{
    if (node == nullptr)
    {
        return "";
    }

    const char *text = node->GetText();
    return text ? clearHtmlTag(text) : "";
}

void PageProcessor::parseRss(const string &filename)
{
    XMLDocument doc;
    XMLError error = doc.LoadFile(filename.c_str());
    if (error != XML_SUCCESS)
    {
        LOG_ERROR("文件解析失败: {}", filename);
        return;
    }

    XMLElement *rssRoot = doc.RootElement();
    if (!rssRoot)
    {
        LOG_ERROR("root 为空: {}", filename);
        return;
    }

    XMLElement *channelNode = rssRoot->FirstChildElement("channel");
    if (!channelNode)
    {
        LOG_ERROR("channel 节点不存在: {}", filename);
        return;
    }

    XMLElement *item = channelNode->FirstChildElement("item");

    while (item)
    {
        Document rssItem;

        rssItem.title = getNodeText(item->FirstChildElement("title"));

        rssItem.link = getNodeText(item->FirstChildElement("link"));

        string content = getNodeText(item->FirstChildElement("content"));
        if (content.empty())
        {
            content = getNodeText(item->FirstChildElement("description"));
        }
        if (content.empty())
        {
            LOG_WARN("item 无 content 和 description，跳过: title={}", rssItem.title);
            item = item->NextSiblingElement("item");
            continue; // 跳过该 item
        }

        rssItem.content = content;
        rssItem.id = documents_.size() + 1;
        documents_.push_back(rssItem);

        item = item->NextSiblingElement("item");
    }

    LOG_INFO("总共解析:{}条记录", documents_.size());
}

// 解析dir目录下的xml文件，提取文档，放入documents中
void PageProcessor::extract_documents(const string &dir)
{
    LOG_INFO("解析{}下xml文件", dir);
    vector<string> filenames = DirectoryScanner::scan(dir);
    LOG_INFO("扫描到{}个xml文件", filenames.size());

    for (const auto &filename : filenames)
    {
        parseRss(filename);
    }

    // XMLError eResult = doc.SaveFile("./outfile/webPage.dat");
    // if (eResult != XML_SUCCESS)
    // {
    //     LOG_ERROR("./outfile/webPage.dat 保存失败");
    //     return;
    // }
}
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

// 依据SimHash算法对documents_去重
void PageProcessor::deduplicate_documents()
{
    LOG_INFO("========== 开始 SimHash 去重 ==========");
    LOG_INFO("去重前文档数量: {}", documents_.size());
    vector<Document> new_docs; // 用于存储新文档  考虑到迭代器失效问题  所以只能这样做
    set<uint64_t> hashs;
    for (auto &doc : documents_)
    {
        //  title + content 拼接分词
        uint64_t hash;
        string text = doc.title + " " + doc.content;
        bool is_similar = false;
        hasher_.make(text, 6, hash);
        // 从集合找相似的
        for (auto h : hashs)
        {
            if (hamming_distance(h, hash) <= 3)
            {
                is_similar = true;
                break;
            }
        }
        if (is_similar) // 如果相似就跳过
        {
            continue;
        }

        // 不相似
        doc.id = new_docs.size() + 1;
        new_docs.push_back(doc);
        hashs.emplace(hash);
    }
    documents_ = std::move(new_docs);

    LOG_INFO("去重后文档数量: {}", documents_.size());
    LOG_INFO("========== SimHash 去重结束 ==========");
}

extern string filterWord_ch(const string &word);

// 构建网页库和网页偏移库
void PageProcessor::build_pages_and_offsets(const string &pages, const string &offsets)
{

    LOG_INFO("========== 构建网页库 ==========");
    XMLDocument doc;
    XMLElement *root = doc.NewElement("rss");
    doc.InsertEndChild(root);

    for (const auto &document : documents_)
    {
        XMLElement *item = doc.NewElement("doc");
        root->InsertEndChild(item);

        XMLElement *id = doc.NewElement("id");
        id->SetText(document.id);
        item->InsertEndChild(id);

        if (!document.link.empty())
        {
            XMLElement *link = doc.NewElement("link");
            link->SetText(document.link.c_str());
            item->InsertEndChild(link);
        }

        if (!document.title.empty())
        {
            XMLElement *title = doc.NewElement("title");
            title->SetText(document.title.c_str());
            item->InsertEndChild(title);
        }

        if (!document.content.empty())
        {
            XMLElement *content = doc.NewElement("content");
            content->SetText(document.content.c_str());
            item->InsertEndChild(content);
        }
    }
    XMLError eResult = doc.SaveFile(pages.c_str());
    if (eResult != XML_SUCCESS)
    {
        LOG_ERROR("./outfile/webPage.dat 保存失败");
        return;
    }
    LOG_INFO("========== 构建网页库已完成 ==========");

    LOG_INFO("========== 构建网页偏移库 ==========");

    ifstream ifs(pages);

    if (!ifs.is_open())
    {
        LOG_ERROR("打开网页库失败:{}", pages);
        return;
    }

    // 一次性读入全部内容
    string fileContent((istreambuf_iterator<char>(ifs)), istreambuf_iterator<char>());

    ifs.close();

    // 创建或打开偏移库文件
    ofstream ofs(offsets);

    if (!ofs.is_open())
    {
        LOG_ERROR("创建网页偏移库失败{}", offsets);
        return;
    }

    // 逐个找<doc> ...</doc>的偏移和长度
    const string startTag = "<doc>";
    const string endTag = "</doc>";
    size_t pos = 0;
    int count = 0;

    while ((pos = fileContent.find(startTag, pos)) != std::string::npos)
    {
        size_t endpos = fileContent.find(endTag, pos);

        if (endpos == std::string::npos)
        {
            LOG_ERROR("创建网页偏移库失败{},找不到</doc>", offsets);
            break;
        }

        endpos += endTag.size();

        size_t length = endpos - pos;

        ofs << ++count << " " << pos << " " << length << "\n";

        LOG_DEBUG("doc id:{} offset:{} length:{}", count, pos, length);
        pos = endpos;
    }

    ofs.close();

    LOG_INFO("偏移库保存成功: {} 共{}条", offsets, count);
    LOG_INFO("========== 构建网页偏移库完成 ==========");
}

// 构建倒排索引库 <关键字> <文档 id> <关键字在文档中的权重>
void PageProcessor::build_inverted_index(const string &filename)
{
    LOG_INFO("========== 构建倒排索引库 ==========");

    // 维护每个文档中每个词出现的次数
    vector<unordered_map<string, int>> forward_index(documents_.size());

    for (int i = 0; i < documents_.size(); i++)
    {
        const auto &doc = documents_[i];
        string text = doc.title + " " + doc.content;

        // 分词
        vector<string> words;
        tokenizer_.CutForSearch(text, words, true);

        for (auto &word : words)
        {
            if (word.empty() || word == " ")
                continue;
            if (stopWords_.count(word))
                continue;

            // 中文字符 UTF-8 占 3 字节，2个字符 = 6字节
            // 英文字符 1字节，2个字符 = 2字节
            // 所以 < 2字节的一定只有1个字符
            if (word.size() < 2) // ASCII单字符直接跳过
                continue;
            // 中文单字（3字节）也跳过
            if (word.size() < 4 && (unsigned char)word[0] >= 0x80)
                continue;

            word = filterWord_ch(word);
            if (word.empty())
                continue;
            forward_index[i][word]++;
        }
    }
    LOG_INFO("处理分词完成");

    // 总文档该词出现的次数
    unordered_map<string, int> df;
    for (int i = 0; i < forward_index.size(); i++)
    {
        for (const auto &[word, count] : forward_index[i])
        {
            df[word]++;
        }
    }

    // 计算TF-IDF
    int N = documents_.size(); // 总文档数

    vector<unordered_map<string, double>> tfidf_index(documents_.size());

    for (int i = 0; i < documents_.size(); i++)
    {
        // 当前文档的总词数
        int total_words = 0;
        for (const auto &[word, count] : forward_index[i])
        {
            total_words += count;
        }
        if (total_words == 0)
            continue;

        for (const auto &[word, count] : forward_index[i])
        {
            double tf = (double)count / total_words;

            double idf = log((double)N / (df[word] + 1));
            tfidf_index[i][word] = tf * idf;
        }
    }

    // 归一化
    for (int i = 0; i < tfidf_index.size(); i++)
    {
        double norm = 0.0;
        for (const auto &[word, weight] : tfidf_index[i])
        {
            norm += weight * weight;
        }
        norm = sqrt(norm);

        if (norm == 0.0)
            continue;

        for (auto &[word, weight] : tfidf_index[i])
        {
            weight /= norm;
        }
    }
    //

    for (int i = 0; i < documents_.size(); i++)
    {
        int docId = documents_[i].id;
        for (const auto &[word, weight] : tfidf_index[i])
        {
            invertedIndex_[word].emplace_back(docId, weight);
        }
    }

    // 最后统一排序
    for (auto &[word, doc_list] : invertedIndex_)
    {
        sort(doc_list.begin(), doc_list.end(),
             [](const pair<int, double> &a, const pair<int, double> &b) { return a.second > b.second; });
    }

    ofstream ofs(filename);

    if (!ofs.is_open())
    {
        LOG_ERROR("创建倒排索引库文件失败:{}", filename);
        return;
    }

    for (const auto &[word, doc_list] : invertedIndex_)
    {
        ofs << word << " ";
        for (const auto &[id, weight] : doc_list)
        {
            ofs << id << " " << weight << " ";
        }
        ofs << "\n";
    }

    ofs.close();

    LOG_INFO("倒排索引库保存成功: {} 共{}个词", filename, invertedIndex_.size());
    LOG_INFO("========== 构建倒排索引库完成 ==========");
}

int main()
{
    initLogger();

    PageProcessor p;
    p.process("./data/corpus/webpages");
    shutdownLogger();

    return 0;
}