// Project-wide Dear ImGui configuration, pulled in through IMGUI_USER_CONFIG.
//
// The test-engine hooks are compiled into imgui.cpp for every target, not just
// the e2e binary, because there is exactly one imgui library and the hooks
// resolve at link time. The cost in the shipped exe is a few empty calls per
// item.
#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_ENABLE_TEST_ENGINE
#define IMGUI_TEST_ENGINE_ENABLE_COROUTINE_STDTHREAD_IMPL 1
#define IMGUI_TEST_ENGINE_ENABLE_CAPTURE 0
#define IMGUI_TEST_ENGINE_ENABLE_IMPLOT 0
#include "imgui_te_imconfig.h"
