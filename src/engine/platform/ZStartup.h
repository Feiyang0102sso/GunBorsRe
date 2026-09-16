#pragma once
#include "engine/core/ZPaths.h"
#include <string>
#include <vector>

/** Windows UTF-16 argument adaptation shared by both launchers. */
class ZUtf8Arguments {
public:
    ZUtf8Arguments(int count, wchar_t **values);
    int Count() const { return static_cast<int>(m_pointers.size()); }
    char **Data() { return m_pointers.data(); }
private:
    std::vector<std::string> m_values;
    std::vector<char *> m_pointers;
};
void OpenProductLog(const char *productName);
