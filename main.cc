#include "DirectoryScanner.h"
#include "KeywordProcessor.h"

#include <fstream>
#include <map>
#include <ostream>
#include <set>

#include "Logger.h"
using namespace std;

int main()
{
    initLogger();

    LOG_INFO("项目开始运行");
    KeyWordProcessor k;

    k.process("./data/corpus/CN/", "./data/corpus/EN/");

    LOG_INFO("项目结束运行");
    return 0;
}