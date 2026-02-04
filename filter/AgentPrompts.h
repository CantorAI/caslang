#pragma once
#include <iostream>
#include <vector>

struct PromptEntry {
    const char* role;
    const char* content;
};

// Embed the prompt text
const char* kCasSystemPrompt = 
#include "Prompts/CAS_Prompt.hpp"
;

const PromptEntry kPrompts[] = {
    {
        "system", // Role
        kCasSystemPrompt // Content
    },
};
const size_t kPromptsCount = sizeof(kPrompts) / sizeof(kPrompts[0]);
