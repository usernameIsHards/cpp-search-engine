#include "DirectoryScanner.h"
#include "KeywordProcessor.h"

#include <fstream>
#include <map>
#include <ostream>
#include <set>

#include <utfcpp/utf8.h>

using namespace std;

int main()
{
;
    KeyWordProcessor k;
    k.process("./data/corpus/CN/", "./data/corpus/EN/");

    return 0;
}