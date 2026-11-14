#pragma once
#include "TriggerEngine.hpp"
#include <map>
// Shared headless rule parser and typed globals: editor and simulation validate
// the same declaration and reference schema on Windows and Linux.
std::map<std::string,int> parseTriggerVariables(const RTSProject& project);
std::vector<TriggerEngine::Rule> parseTriggerRules(const RTSProject& project);
