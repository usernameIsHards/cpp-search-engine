#pragma once
#include <string>
#include <vector>

class DirectoryScanner
{
  public:
    // 遍历目录dir，获取目录里面的所有文件名
    static std::vector<std::string> scan(const std::string &dir);

  private:
    DirectoryScanner() = delete;
};