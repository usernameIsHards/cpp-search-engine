#include "PageProcessor.h"
#include "DirectoryScanner.h"
#include "KeywordProcessor.h"
#include "Logger.h"
#include <regex>
#include <set>
#include <tinyxml2.h>
using namespace std;
using namespace tinyxml2;
// PageProcessor::PageProcessor()
// {
// }

void PageProcessor::process(const string &dir)
{
    extract_documents(dir);
    deduplicate_documents();

    build_pages_and_offsets("./outfile/webPage.dat", "");
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
string clearHtmlTag(const string &str)
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
string getNodeText(XMLElement *node)
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

    ofstream ofs(offsets);

    if (!ofs.is_open())
    {
        LOG_ERROR("创建网页偏移库失败{}", offsets);
        return;
    }

    int page_fd = open(pages.c_str(), O_RDONLY);

    if (page_fd == -1)
    {
        LOG_ERROR("打开网页库失败{}", pages);
        return;
    }

    

}

// 构建倒排索引库
void PageProcessor::build_inverted_index(const string &filename)
{
}

int main()
{
    initLogger();

    PageProcessor p;
    p.process("./data/corpus/webpages");
    shutdownLogger();

    return 0;
}