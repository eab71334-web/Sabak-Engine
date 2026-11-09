#include "script.h"
namespace sk { std::map<std::string,ScriptFactory>& scriptRegistry(){static std::map<std::string,ScriptFactory> r;return r;} }
