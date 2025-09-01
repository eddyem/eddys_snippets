#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <unistd.h>

// Hash table entry structure
typedef struct {
    uint32_t hash;
    const char *key;
    const char *value;
    size_t key_len;
} HashEntry;

// Substitution table structure
typedef struct {
    HashEntry *entries;
    size_t size;
    size_t capacity;
} SubstitutionTable;

// DJB2 hash function
uint32_t hash_function(const char *str, size_t len) {
    uint32_t hash = 5381;
    for (size_t i = 0; i < len; i++) {
        hash = ((hash << 5) + hash) + (uint8_t)str[i];
    }
    return hash;
}

// Initialize substitution table
void init_table(SubstitutionTable *table, size_t initial_capacity) {
    table->entries = malloc(initial_capacity * sizeof(HashEntry));
    table->size = 0;
    table->capacity = initial_capacity;
}

// Add a substitution pair to the table
void add_substitution(SubstitutionTable *table, const char *key, const char *value) {
    if (table->size >= table->capacity) {
        // Resize table if needed
        table->capacity *= 2;
        table->entries = realloc(table->entries, table->capacity * sizeof(HashEntry));
    }
    
    size_t key_len = strlen(key);
    uint32_t hash = hash_function(key, key_len);
    
    table->entries[table->size].hash = hash;
    table->entries[table->size].key = key;
    table->entries[table->size].value = value;
    table->entries[table->size].key_len = key_len;
    table->size++;
}

// Compare function for binary search
int compare_entries(const void *a, const void *b) {
    const HashEntry *entry_a = (const HashEntry *)a;
    const HashEntry *entry_b = (const HashEntry *)b;
    if (entry_a->hash < entry_b->hash) return -1;
    if (entry_a->hash > entry_b->hash) return 1;
    return 0;
}

// Sort the table by hash for binary search
void sort_table(SubstitutionTable *table) {
	printf("sort\n");
    qsort(table->entries, table->size, sizeof(HashEntry), compare_entries);
    for(int i = 0; i < table->size; ++i){
    	HashEntry *e = &table->entries[i];
    	printf("hash: %d, key: %s, val: %s, len: %zd\n", e->hash, e->key, e->value, e->key_len);
    }
}

// Find a substitution using binary search
const char *find_substitution(SubstitutionTable *table, const char *word, size_t word_len) {
    uint32_t hash = hash_function(word, word_len);
	write(1, word, word_len);
	fprintf(stderr, " - search this with hash %d\n", hash);
    // Binary search for the hash
    size_t low = 0, high = table->size - 1;
    while (low <= high) {
        size_t mid = (low + high) / 2;
        printf("\t\tmid=%zd, low=%zd, high=%zd\n", mid, low, high);
        if (table->entries[mid].hash == hash) {
            // Check all entries with the same hash for exact match
            size_t left = mid;
            while (left > 0 && table->entries[left - 1].hash == hash) left--;
            for (size_t i = left; i < table->size && table->entries[i].hash == hash; i++) {
                if (table->entries[i].key_len == word_len && 
                    strncmp(table->entries[i].key, word, word_len) == 0) {
                    printf("FOUND: %s\n", table->entries[i].value);
                    return table->entries[i].value;
                }
            }
            break;
        } else if (table->entries[mid].hash < hash) {
            low = mid + 1;
        } else {
            if(mid > 0) high = mid - 1;
            else break;
        }
    }
    return NULL; // Not found
}

// Check if a character is a word boundary
int is_word_boundary(char c) {
    return !isalnum(c) && c != '_';
}

// Perform substitution on the input string
char *substitute_words(const char *input, SubstitutionTable *table) {
    size_t input_len = strlen(input);
    // Allocate enough space (input length * 2 for worst-case expansion)
    char *result = malloc(input_len * 2 + 1);
    if (!result) return NULL;
    
    size_t result_idx = 0;
    const char *p = input;
    
    while (*p) {
        // Skip non-word characters
        if (is_word_boundary(*p)) {
            result[result_idx++] = *p++;
            continue;
        }
        
        // Find the current word boundaries
        const char *word_start = p;
        while (*p && !is_word_boundary(*p)) p++;
        size_t word_len = p - word_start;
        
        // Look up the word in the substitution table
        const char *replacement = find_substitution(table, word_start, word_len);
        
        if (replacement) {
            // Copy the replacement word
            size_t replacement_len = strlen(replacement);
            strcpy(result + result_idx, replacement);
            result_idx += replacement_len;
        } else {
            // Copy the original word
            strncpy(result + result_idx, word_start, word_len);
            result_idx += word_len;
        }
    }
    
    result[result_idx] = '\0';
    return result;
}

// Free the substitution table
void free_table(SubstitutionTable *table) {
    free(table->entries);
    table->entries = NULL;
    table->size = table->capacity = 0;
}

int main() {
    // Example usage
    SubstitutionTable table;
    init_table(&table, 10);
    
    // Add substitution pairs
    add_substitution(&table, "hello", "hi");
    add_substitution(&table, "world", "earth");
    add_substitution(&table, "foo", "bar");
    add_substitution(&table, "test", "example");
    
    // Sort the table for binary search
    sort_table(&table);
    
    const char *input = "hello world, this is a test foo bar!";
    char *output = substitute_words(input, &table);
    
    printf("Input:  %s\n", input);
    printf("Output: %s\n", output);
    
    free(output);
    free_table(&table);
    
    return 0;
}