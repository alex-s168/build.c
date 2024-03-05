#ifndef CC 
# define CC        "gcc"
#endif 

#ifndef CC_ARGS 
# define CC_ARGS   ""
#endif 

#ifndef CXX
# define CXX       "g++"
#endif 

#ifndef CXX_ARGS
# define CXX_ARGS  ""
#endif

#ifndef AR 
# define AR        "ar"
#endif 

#ifndef MKDIR
# ifdef _WIN32
#  define MKDIR    "mkdir"
# else 
#  define MKDIR    "mkdir -p"
# endif 
#endif

#ifndef LD_ARGS
# define LD_ARGS   ""
#endif

#ifndef VERBOSE
# define VERBOSE   0
#endif 

/* ========================================================================= */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <unistd.h>

#define error(msg) fprintf(stderr, "%s", msg)

enum CompileType {
    CT_C,
    CT_CXX,
    CT_NOC,
    CT_DEP,
    CT_DIR,
    CT_RUN,
};

enum CompileResult {
    CR_OK = 0,
    CR_FAIL = 1,
    CR_FAIL_2 = 2,
};

struct CompileData {
    char *srcFile;
    char *outFile;
    enum CompileType type;
};

void *compileThread(void *arg);
enum CompileResult linkTask(struct CompileData *objs, size_t len, char *out);
enum CompileResult link_exe(struct CompileData *objs, size_t len, char *out);
enum CompileResult compile(struct CompileData *objs, size_t len);
enum CompileResult verifyDependencies(struct CompileData *objs, size_t len);
enum CompileResult allRun(struct CompileData *objs, size_t len);

bool exists(char *file) {
    return access(file, F_OK) == 0;
}

#define SP(typeIn, file) { .type = typeIn, .srcFile = file, .outFile = "build/" file ".o" }
#define DEP(file) { .type = CT_DEP, .srcFile = "", .outFile = file }
#define DIR(dir)  { .type = CT_DIR, .srcFile = "", .outFile = dir }
#define RUN(exe)  { .type = CT_RUN, .srcFile = exe,  .outFile = "" }

#define START   enum CompileResult res
#define DO(cmd) res = cmd; if (res != CR_OK) return res;
#define END     return CR_OK;
#define LEN(li) (sizeof(li) / sizeof(li[0]))
#define LI(li)  li, LEN(li)

enum CompileResult test_impl(char *outFile, size_t id, struct CompileData *data, size_t dataLen);

#define test(indir, infile, id, type, ...) \
    static struct CompileData d_##id[] = { \
        DIR("build/"), \
        DIR("build/" indir), \
        SP(type, infile), \
        RUN("build/" infile ".exe"), \
        __VA_ARGS__, \
    }; \
    res = test_impl("build/" infile ".exe", id, LI(d_##id)); \
    if (res != CR_OK) return res;

#if VERBOSE
int system_impl(const char *command) {
    printf("$ %s\n", command);
    return system(command);
}

#define system system_impl
#endif

struct Target {
    char *name;
    enum CompileResult (*run)();
};

int build_main(int argc, char **argv,
               struct Target *targets, size_t targets_len) {

    if (argc != 2)
	    goto invalid_target;

    char *target = argv[1];

    bool found = false;
    for (size_t i = 0; i < targets_len; i ++) {
    	struct Target tg = targets[i];
	    if (strcmp(target, tg.name) == 0) {
            found = true;
	        enum CompileResult r = tg.run();
	        if (r == CR_FAIL) {
		        error("FAIL\n");
		        return 1;
	        } else if (r == CR_FAIL_2) {
		        error("FAIL_2\n");
	    	    return 1;
	        }
	        break;
	    }
    }

    if (!found)
	    goto invalid_target;

    return 0;

    invalid_target:
        error("Invalid target! Targets:\n");
	    for (size_t i = 0; i < targets_len; i ++)
	        fprintf(stderr, "- %s\n", targets[i].name);
        return 2;
}

void *compileThread(void *arg) {
    struct CompileData *data = arg;
    if (data->type == CT_C) {
        char *args = malloc(strlen(data->srcFile) + strlen(data->outFile) +
                            sizeof(CC) + 9 + sizeof(CC_ARGS));
        sprintf(args, "%s -c %s -o %s " CC_ARGS, CC, data->srcFile, data->outFile);

        int res = system(args);
        free(args);
        if (res != 0) {
            printf("%i\n", res);
            return (void *) CR_FAIL;
        }
    } else if (data->type == CT_CXX) {
        char *args = malloc(strlen(data->srcFile) + strlen(data->outFile) +
                            sizeof(CXX) + 9 + sizeof(CXX_ARGS));
        sprintf(args, "%s -c %s -o %s " CXX_ARGS, CXX, data->srcFile, data->outFile);

        int res = system(args);
        free(args);
        if (res != 0) {
            printf("%i\n", res);
            return (void *) CR_FAIL;
        }
    }
    return (void *) CR_OK;
}

enum CompileResult linkTask(struct CompileData *objs, size_t len, char *out) {
    size_t sl = strlen(out) + 6 + sizeof(AR);
    for (size_t i = 0; i < len; i ++) {
        sl += strlen(objs[i].outFile) + 1;
    }
    char *cmd = malloc(sl);
    cmd[0] = '\0';
    sprintf(cmd, "%s rcs %s ", AR, out);

    // yes, we allocate some useless bytes but we just don't care

    for (size_t i = 0; i < len; i ++) {
        struct CompileData cd = objs[i];
        if (cd.type == CT_DIR)
            continue;

        strcat(cmd, cd.outFile);
        strcat(cmd, " ");
    }

    int res = system(cmd);
    free(cmd);
    if (res == 0)
        return CR_OK;
    return CR_FAIL;
}

enum CompileResult link_exe(struct CompileData *objs, size_t len, char *out) {
    size_t sl = strlen(out) + 5 + sizeof(CC) + sizeof(LD_ARGS);
    for (size_t i = 0; i < len; i ++) {
        sl += strlen(objs[i].outFile) + 1;
    }
    char *cmd = malloc(sl);
    cmd[0] = '\0';
    sprintf(cmd, "%s -o %s " LD_ARGS " ", CC, out);

    for (size_t i = 0; i < len; i ++) {
        struct CompileData cd = objs[i];
        if (cd.type == CT_DIR)
            continue;

        strcat(cmd, cd.outFile);
        strcat(cmd, " ");
    }

    int res = system(cmd);
    free(cmd);
    if (res == 0)
        return CR_OK;
    return CR_FAIL;
}

enum CompileResult compile(struct CompileData *objs, size_t len) {
    pthread_t *threads = malloc(sizeof(pthread_t) * len);

    for (size_t i = 0; i < len; i ++) {
        struct CompileData *data = &objs[i];        

        pthread_create(&threads[i], NULL, compileThread, data);

        if (data->type == CT_DIR) {
            char *args = malloc(strlen(data->outFile) + sizeof(MKDIR) + 1);
            sprintf(args, "%s %s", MKDIR, data->outFile);
            (void) system(args);
            free(args);
        }
    }

    for (size_t i = 0; i < len; i ++) {
        void *resr;
        pthread_join(threads[i], &resr);
        enum CompileResult res = (enum CompileResult) (intptr_t) resr;

        if (res != CR_OK) {
            for (size_t j = 0; j < len; j ++) {
                if (i == j)
                    continue;

                pthread_kill(threads[j], SIGABRT);
            }
            free(threads);
            return CR_FAIL;
        }
    }

    free(threads);
    return CR_OK;
}

enum CompileResult verifyDependencies(struct CompileData *objs, size_t len) {
    size_t err = 0;
    for (size_t i = 0; i < len; i ++) {
        if (objs[i].type != CT_DEP)
            continue;

        char *f = objs[i].outFile;
        if (!exists(f)) {
            error("Missing dependency:\n ");
            error(f);
            error("\n");
            err ++;
        }
    }
    if (err > 0)
        return CR_FAIL;
    return CR_OK;
}

enum CompileResult allRun(struct CompileData *objs, size_t len) {
    for (size_t i = 0; i < len; i ++) {
        struct CompileData *data = &objs[i];
        if (data->type != CT_RUN)
            continue;

        char *f = data->srcFile;
        if (!exists(f)) {
            error("Missing executeable:\n");
            error(f);
            error("\n");
            return CR_FAIL_2;
        }

        int res = system(f);
        if (res != 0)
            return CR_FAIL;
    }
    return CR_OK;
}

enum CompileResult test_impl(char *outFile, size_t id, struct CompileData *data, size_t dataLen) {
    {
       enum CompileResult r = verifyDependencies(data, dataLen);
       if (r != CR_OK)
           goto fail;
    }

    {
        enum CompileResult r = compile(data, dataLen);
        if (r != CR_OK)
            goto fail;
    }

    {
        enum CompileResult r = link_exe(data, dataLen, outFile);
        if (r != CR_OK)
            goto fail;
    }

    {
        enum CompileResult r = allRun(data, dataLen);
        if (r != CR_OK)
            goto fail;
    }

    printf("Test %zu: OK\n", id);
    return CR_OK;

fail:
    printf("Test %zu: FAIL\n", id);
    return CR_FAIL;
}


