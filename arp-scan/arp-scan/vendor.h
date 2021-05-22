#ifndef VENDOR_H_
#define VENDOR_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

struct vendor_entry {
    char* prefix;
    char* vendor;
};

struct vendor_list {
    struct vendor_entry* entries;
    size_t nentries;
    size_t capacity;
};

void vendor_list_init(struct vendor_list* list, const char* vendorfile_path);
void vendor_list_free(struct vendor_list* list);
const char* vendor_list_lookup(struct vendor_list* list, const char* macaddr);

#ifdef __cplusplus
}
#endif

#endif