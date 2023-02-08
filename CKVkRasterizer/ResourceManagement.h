#ifndef RESOURCEMANAGEMENT_H
#define RESOURCEMANAGEMENT_H

#include <string>

unsigned int get_resource_size(const char* type, const char* name);
void* get_resource_data(const char* type, const char* name);
std::string load_resource(const char* type, const char* name);
std::string load_resource_or_file(const std::string& path);

#endif