#pragma once
#include "TriggerEngine.hpp"
// Shared headless trigger rule parser: one schema for runtime and authoring.
std::vector<TriggerEngine::Rule> parseTriggerRules(const RTSProject& project);
