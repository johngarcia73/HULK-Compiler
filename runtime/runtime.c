#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// --- Simple Memory Tracker ---
static void** _alloc_registry = NULL;
static size_t _alloc_count = 0;
static size_t _alloc_capacity = 0;

// Tracked allocation
void* _hulk_alloc(size_t size) {
    void* ptr = malloc(size);
    if (!ptr) return NULL;
    
    if (_alloc_count >= _alloc_capacity) {
        _alloc_capacity = _alloc_capacity == 0 ? 128 : _alloc_capacity * 2;
        _alloc_registry = (void**)realloc(_alloc_registry, _alloc_capacity * sizeof(void*));
    }
    
    _alloc_registry[_alloc_count++] = ptr;
    return ptr;
}

// Cleanup function to be called at the end of the program
void _hulk_cleanup() {
    for (size_t i = 0; i < _alloc_count; i++) {
        free(_alloc_registry[i]);
    }
    free(_alloc_registry);
    _alloc_registry = NULL;
    _alloc_count = 0;
    _alloc_capacity = 0;
}
// -----------------------------
#include <stdint.h>
#include <string.h>

// Assuming length is a 32-bit word (uint32_t)
int _string_compair(const char *a, const char *b) {
    if (a == b) return 1; // Same address? O(1) success
    if (!a || !b) return 0;

    // Cast to get the length word stored at the beginning
    uint32_t len_a = *(uint32_t*)a;
    uint32_t len_b = *(uint32_t*)b;

    if (len_a != len_b) return 0; // Different length? O(1) failure

    // Compare actual data (skipping the 4-byte length header)
    return memcmp(a + 4, b + 4, len_a) == 0;
}

// Hulk string concatenation runtime wrapper
char* _string_concat(const char* s1, const char* s2) {
    uint32_t len1 = s1 ? *(const uint32_t*)s1 : 0;
    uint32_t len2 = s2 ? *(const uint32_t*)s2 : 0;
    uint32_t total_len = len1 + len2;
    
    // Allocate 4 bytes for length, + total_len for characters, + 1 for null terminator
    char* result = (char*)_hulk_alloc(4 + total_len + 1);
    
    if (result) {
        // Write the new 32-bit length
        *(uint32_t*)result = total_len;
        
        // Copy the characters from s1 and s2 (jumping over the 4-byte length prefix)
        if (len1 > 0) memcpy(result + 4, s1 + 4, len1);
        if (len2 > 0) memcpy(result + 4 + len1, s2 + 4, len2);
        
        // Add null terminator so printf works securely in `_print`
        result[4 + total_len] = '\0';
    }
    
    return result;
}
int _print(char* string_with_length) {
    char* raw_string = string_with_length+4;
    if (raw_string) {
        printf("%s\n", raw_string);
        return 0;
    }
    return -1;
}