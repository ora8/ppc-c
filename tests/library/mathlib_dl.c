/* loads the library MathLib at run time (dlopen), calls it and unloads it */
#include <dlfcn.h>
#include <stdio.h>

int main(int argc, char **argv) {
    void *h = dlopen(argv[1], RTLD_NOW);
    if (!h) { printf("dlopen: %s\n", dlerror()); return 1; }
    int (*add)(int, int) = (int (*)(int, int))dlsym(h, "Add");
    int (*shout)(const char *) = (int (*)(const char *))dlsym(h, "Shout");
    printf("Add: %d\n", add(40, 2));
    printf("Shout: %d\n", shout("one"));
    printf("Shout: %d\n", shout("two"));
    printf("R_Add visible: %s\n", dlsym(h, "R_Add") ? "yes" : "no");
    fflush(stdout);
    dlclose(h);
    printf("unloaded\n");
    return 0;
}
