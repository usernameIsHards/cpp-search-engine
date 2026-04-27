#include <iostream>

#include "DirectoryScanner.h"
#include "common.h"

using namespace std;

// const string &EN_dir = "./data/corpus/EN";
// const string &CN_dir = "./data/corpus/CN";
// const string &stopwords_dir = "./data/stopwords";

vector<string> DirectoryScanner::scan(const string &dir)
{
    // 遍历目录dir，获取目录里面的所有文件名
    DIR *dirp = opendir(dir.c_str());
    assert(dirp != nullptr && "opendir failed");

       vector<string> filenames;
    // 读取目录流
    struct dirent *pdirent;
    while (pdirent = readdir(dirp))
    {
        if (!strcmp(pdirent->d_name, ".") || !strcmp(pdirent->d_name, ".."))
            continue;
        filenames.push_back(dir + "/" + pdirent->d_name);
    }
    closedir(dirp);

    return filenames;
}

#ifdef DIRTEST
int main()
{
    vector<string> filenames = DirectoryScanner::scan(CN_dir);
    for (auto &s : filenames)
    {
        cout << s << endl;
    }

    return 0;
}
#endif