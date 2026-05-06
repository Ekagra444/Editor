#include "commands.h"
#include "sidebar.h"
#include "editor.h"
#include "types.h"
#include <stdio.h>
#include <string.h>

// Rebuild the root tree from scratch (used after any filesystem mutation)
static void refresh_tree(void) {
    free_tree(root);
    root          = create_node(".", ".", 1);
    root->expanded = 1;
    build_tree(root);
}

void execute_command(const char *cmd) {
    if (strncmp(cmd, ":new ", 5) == 0) {
        const char *filename = cmd + 5;
        FILE *fp = fopen(filename, "w");
        if (fp) fclose(fp);
        refresh_tree();
    }
    else if (strncmp(cmd, ":del ", 5) == 0) {
        remove(cmd + 5);
        refresh_tree();
    }
    else if (strncmp(cmd, ":ren ", 5) == 0) {
        char oldname[128], newname[128];
        if (sscanf(cmd + 5, "%127s %127s", oldname, newname) == 2)
            rename(oldname, newname);
        refresh_tree();
    }
    else if (strncmp(cmd, ":open ", 6) == 0) {
        open_file_in_tab(cmd + 6);
    }
    else {
        printf("Unknown command: %s\n", cmd);
    }
}
