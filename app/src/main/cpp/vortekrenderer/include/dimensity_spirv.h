#ifndef DIMENSITY_SPIRV_H
#define DIMENSITY_SPIRV_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include "spirv.h"

// A bounds preflight for the operands read by our workarounds, not a complete
// SPIR-V validator. Unknown/new instructions retain their encoded word count.
static inline uint32_t dgSpvInstructionLength(const uint32_t* code, size_t words, size_t index) {
    if (!code || index >= words) return 0;
    uint32_t length = code[index] >> SpvWordCountShift;
    if (!length || length > words - index) return 0;
    uint32_t opcode = code[index] & SpvOpCodeMask, minimum = 1;
    if (opcode >= SpvOpTypeVoid && opcode <= SpvOpTypeForwardPointer) minimum = 2;
    switch (opcode) {
        case SpvOpName: minimum = 3; break;
        case SpvOpEntryPoint: minimum = 4; break;
        case SpvOpFunction: minimum = 5; break;
        case SpvOpCapability: minimum = 2; break;
        case SpvOpVariable: case SpvOpTypePointer: case SpvOpTypeVector:
        case SpvOpTypeInt: case SpvOpConstant: case SpvOpLoad: minimum = 4; break;
        case SpvOpTypeFloat: case SpvOpConstantComposite: case SpvOpStore: minimum = 3; break;
        case SpvOpSelect: minimum = 6; break;
        case SpvOpDecorate:
            minimum = 3;
            if (length >= 3 && (code[index + 2] == SpvDecorationLocation || code[index + 2] == SpvDecorationBuiltIn)) minimum = 4;
            break;
        default: break;
    }
    return length >= minimum ? length : 0;
}

static inline bool dgSpvModuleBounds(const uint32_t* code, size_t bytes) {
    if (!code || bytes < 5 * sizeof(uint32_t) || bytes % sizeof(uint32_t) ||
            bytes > INT32_MAX || code[0] != SpvMagicNumber) return false;
    size_t words = bytes / sizeof(uint32_t);
    for (size_t i = 5; i < words;) {
        uint32_t length = dgSpvInstructionLength(code, words, i);
        if (!length) return false;
        i += length;
    }
    return true;
}

static inline bool dgSpvLiteralString(const uint32_t* words, size_t wordCount, char* output, size_t capacity) {
    if (!output || !capacity) return false;
    output[0] = 0;
    if (!words || wordCount > SIZE_MAX / sizeof(uint32_t)) return false;
    const char* start = (const char*)words;
    const char* end = memchr(start, 0, wordCount * sizeof(uint32_t));
    if (!end || (size_t)(end - start) >= capacity) return false;
    memcpy(output, start, end - start + 1);
    return true;
}
#endif
