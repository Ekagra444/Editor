#include "sidebar.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>

FileNode *create_node(const char *name, const char *path, int is_dir) {
    FileNode *n  = malloc(sizeof(FileNode));
    if (!n) return NULL;
    strncpy(n->name, name, sizeof(n->name) - 1);
    n->name[sizeof(n->name) - 1] = '\0';
    strncpy(n->path, path, sizeof(n->path) - 1);
    n->path[sizeof(n->path) - 1] = '\0';
    n->is_dir      = is_dir;
    n->child_count = 0;
    n->expanded    = 0;
    return n;
}

void build_tree(FileNode *parent) {
    // Free any previously loaded children to prevent duplicates on re-expand
    for (int i = 0; i < parent->child_count; i++) {
        free_tree(parent->children[i]);
        parent->children[i] = NULL;
    }
    parent->child_count = 0;

    DIR *dir = opendir(parent->path);
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) continue;
        if (parent->child_count >= 128) break;

        char full[512];
        snprintf(full, sizeof(full), "%s/%s", parent->path, entry->d_name);

        struct stat st;
        if (stat(full, &st) == -1) continue;

        int       is_dir = S_ISDIR(st.st_mode);
        FileNode *child  = create_node(entry->d_name, full, is_dir);
        if (child) parent->children[parent->child_count++] = child;
    }
    closedir(dir);
}

void build_visible(FileNode *node, int depth) {
    visible[visible_count++] = (VisibleNode){node, depth};
    if (node->is_dir && node->expanded) {
        for (int i = 0; i < node->child_count; i++)
            build_visible(node->children[i], depth + 1);
    }
}

// Recursively free all nodes — fixes the memory leak
void free_tree(FileNode *node) {
    if (!node) return;
    for (int i = 0; i < node->child_count; i++)
        free_tree(node->children[i]);
    free(node);
}
