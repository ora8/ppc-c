/* ini.h - ppc.ini, the project file of ppm (and of ppc for the module paths of a project) */
#ifndef PPC_INI_H
#define PPC_INI_H
#include "util.h"

typedef struct {
    char *section, *key, *value;
    int line;
} IniEntry;

typedef struct {
    char *path;
    VEC(IniEntry) entries;
} Ini;

/* reads path; 0 if it worked, else *err says why (with the line) */
int ini_read(const char *path, Ini *ini, char **err);
/* the value of key in section (NULL: not there) */
const char *ini_get(const Ini *ini, const char *section, const char *key);
/* the parts of a value separated by any of seps (empty parts are dropped, spaces trimmed) */
typedef VEC(char *) StrVec;
StrVec ini_split(const char *value, const char *seps);

/* a project: the directory of its ppc.ini
     [project]  name, version, kind (program | library | modules), static (yes | no), main
     [dependencies]  name = path of another project
     [link]  shared-objects, static-libraries (separated by ',' or ';')
     [build]  options (separated by spaces) */
typedef struct {
    char *dir;      /* with a final '/' */
    char *ini_path;
    Ini ini;
    char *name;
    int is_library;
    int is_modules; /* kind = modules: only modules for other projects (no program or library) */
    int is_static;
    char *main;     /* the program or library file (NULL: none found) */
} Project;

int project_load(const char *ini_path, Project *p, char **err);
/* the src/ directories of the project and all its dependencies (transitively, each once) */
int project_module_dirs(const Project *p, StrVec *dirs, char **err);
/* the ppc.ini in dir or above (NULL: none) */
char *project_find(const char *dir);
/* the first two words of a source file without comments: 'program X', 'library X', 'module X' (NULL: none) */
int source_header(const char *path, char **word, char **name);

#endif
