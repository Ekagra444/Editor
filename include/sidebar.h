#ifndef SIDEBAR_H
#define SIDEBAR_H

#include "types.h"

FileNode *create_node(const char *name, const char *path, int is_dir);
void      build_tree(FileNode *parent);
void      build_visible(FileNode *node, int depth);
void      free_tree(FileNode *node);

#endif // SIDEBAR_H
