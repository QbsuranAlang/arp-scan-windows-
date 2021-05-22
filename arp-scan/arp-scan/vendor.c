#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "vendor.h"

static int starts_with(const char* str, const char* beginning) {
    while (*str && *beginning && *str == *beginning) ++str, ++beginning;
    return !*beginning;
}

static int skip_linebreak(const char* str, int pos) {
    if (str[pos] == '\r' && str[pos + 1] == '\n') return 2;
    if (str[pos] == '\n') return 1;
    return 0;
}

static char* copy_substring(const char* str, int i, int len) {
    char* substr;
    
    substr = calloc(len + 1, 1);
    memcpy(substr, str + i, len);

    return substr;
}

static void push_entry(struct vendor_list* list, struct vendor_entry* entry) {
    if (list->capacity < list->nentries + 1) {
        struct vendor_entry* entries = calloc(list->capacity + 100, sizeof(struct vendor_entry));
        memcpy(entries, list->entries, list->nentries * sizeof(struct vendor_entry));
        free(list->entries);
        list->entries = entries;
        list->capacity += 100;
    }
    memcpy(list->entries + list->nentries, entry, sizeof(struct vendor_entry));
    list->nentries += 1;
}

void vendor_list_init(struct vendor_list* list, const char* vendorfile_path) {
    FILE* f;
    char* block;
    int blocksz;
    int startpos, macpos, endpos;
    int i;
    struct vendor_entry entry;

    list->entries = NULL;
    list->nentries = 0;
    list->capacity = 0;

    f = fopen(vendorfile_path, "r");

    if (f == NULL) {
        fprintf(stderr, "failed to open mac-vendor.txt\n");
        exit(1);
    }

    fseek(f, 0, SEEK_END);
    blocksz = ftell(f);
    fseek(f, 0, SEEK_SET);
    block = calloc(blocksz, 1);
    fread(block, 1, blocksz, f);

    startpos = 0;
    macpos = 0;
    endpos = 0;

    for (i = 0; i < blocksz;) {
        int skip;

        if (block[i] == '\t') {
            ++i;
            macpos = i;
            continue;
        }
        
        skip = skip_linebreak(block, i);
        if (skip) {
            endpos = i;
            entry.prefix = copy_substring(block, startpos, macpos - 1 - startpos);
            entry.vendor = copy_substring(block, macpos, endpos - macpos);
            push_entry(list, &entry);
            i += skip;
            startpos = i;
            continue;
        }

        ++i;
    }
    // push last
    endpos = i;
    entry.prefix = copy_substring(block, startpos, macpos - 1 - startpos);
    entry.vendor = copy_substring(block, macpos, endpos - macpos);
    push_entry(list, &entry);

    free(block);
}

void vendor_list_free(struct vendor_list* list) {
    size_t i;

    for (i = 0; i < list->nentries; ++i) {
        struct vendor_entry* entry = &list->entries[i];
        free(entry->prefix);
        free(entry->vendor);
    }
}

const char* vendor_list_lookup(struct vendor_list* list, const char* macaddr) {
    size_t i;

    for (i = 0; i < list->nentries; ++i) {
        const struct vendor_entry* entry = &list->entries[i];

        if (starts_with(macaddr, entry->prefix)) {
            return entry->vendor;
        }
    }

    return "Unknown";
}