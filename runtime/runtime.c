#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <gc.h>
typedef enum{
    HULK_TYPE_PTR=0, //null is included here
    HULK_TYPE_NUM=1,
    HULK_TYPE_BOOL=2,
    HULK_TYPE_STRING=3,
} TypeFlag;


//################ GARGABE COLLECTOR RUNTIME #####################
void _hulk_gc_init(){
    printf("DEBUG:Creating GC heap...\n");
    GC_init();
}
void* _hulk_alloc(size_t size) {
    void  *address = GC_malloc(size);
    printf("DEBUG: Allocated %zu bytes on address %p \n", size, address);
    return address;
}
//################ BUILT-IN LENGUAJE FUNCTIONS #####################
// Assuming length is a 32-bit word (uint32_t)
uint32_t _string_compair(const char *a, const char *b) {
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
char* _print(char* string_with_length) {
    char* raw_string = string_with_length+4;
    if (raw_string) {
        printf("%s\n", raw_string);
        return string_with_length;
    }
    return string_with_length;
}

static char* _create_hulk_string(const char* buffer, int len) {
    char* result = (char*)_hulk_alloc(4 + len + 1);
    if (result!=0) {
        *(uint32_t*)result = (uint32_t)len;
        memcpy(result + 4, buffer, len);
        result[4 + len] = '\0';
    }
    else
    {    
        printf("WARNING: Failed to create Hulk string\n");
    }
    return result;
}

char* _d_to_string(double value, TypeFlag type) {
    char buffer[64];
    int len = snprintf(buffer, sizeof(buffer), "%g", value);
    return _create_hulk_string(buffer, len);
}

char* _w_to_string(uint32_t value, TypeFlag type) {
    if (type == HULK_TYPE_STRING) return (char*)value;
    if (type == HULK_TYPE_BOOL) {
        const char* s = value ? "true" : "false";
        return _create_hulk_string(s, (int)strlen(s));
    }
    if (type == HULK_TYPE_PTR && value == 0) {
        return _create_hulk_string("null", 4);
    }
    char buffer[32];
    int len = snprintf(buffer, sizeof(buffer), "%u", value);
    return _create_hulk_string(buffer, len);
}

char* _l_to_string(uint64_t value, TypeFlag type) {
    if (type == HULK_TYPE_STRING) return (char*)value;
    if (type == HULK_TYPE_BOOL) {
        const char* s = value ? "true" : "false";
        return _create_hulk_string(s, (int)strlen(s));
    }
    if (type == HULK_TYPE_PTR && value == 0) {
        return _create_hulk_string("null", 4);
    }
    char buffer[32];
    int len = snprintf(buffer, sizeof(buffer), "%llu", (unsigned long long)value);
    return _create_hulk_string(buffer, len);
}

char* _s_to_string(float value, TypeFlag type) {
    char buffer[64];
    int len = snprintf(buffer, sizeof(buffer), "%g", (double)value);
    return _create_hulk_string(buffer, len);
}
