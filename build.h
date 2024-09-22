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
# define MKDIR    "mkdir -p"
#endif

#ifndef RMDIR
# define RMDIR    "rm -rf"
#endif

#ifndef RMFILE
# define RMFILE   "rm -f" 
#endif 

#ifndef LD_ARGS
# define LD_ARGS   ""
#endif

#ifndef PYTHON
# define PYTHON    "python3"
#endif

#ifndef VERBOSE
# define VERBOSE   0
#endif 

#ifndef SELF_PATH
# define SELF_PATH "build_c/"
#endif

#if defined(_WIN32) && !defined(SERIAL_COMP)
# define SERIAL_COMP 1
#endif 

/* ========================================================================= */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#if !SERIAL_COMP
# include <pthread.h>
#endif

#ifdef _WIN32
# include <io.h>
#else 
# include <signal.h>
# include <unistd.h>
#endif 

#define MINIRENT_IMPLEMENTATION
#include "minirent.h"

#define error(msg) fprintf(stderr, "%s", msg)

enum CompileType {
    CT_C,
    CT_CXX,
    CT_NOC,
    CT_DEP,
    CT_CDEF,
    CT_DIR,
    CT_RUN,
    CT_LDARG,
    CT_CCARG,
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

struct CompileThreadData {
    struct CompileData * data;

    const char * ccArgs;
    size_t       ccArgsLen;
};

void *compileThread(void *arg);
enum CompileResult linkTask(struct CompileData *objs, size_t len, char *out);
enum CompileResult link_exe(struct CompileData *objs, size_t len, char *out);
enum CompileResult compile(struct CompileData *objs, size_t len);
enum CompileResult verifyDependencies(struct CompileData *objs, size_t len);
enum CompileResult allRun(struct CompileData *objs, size_t len);

bool exists(const char *file) {
#ifdef _WIN32 
    DWORD attrib = GetFileAttributes(file);
    return attrib != INVALID_FILE_ATTRIBUTES;
#else 
    return access((char *)file, F_OK) == 0;
#endif 
}

#define SP(typeIn, file) { .type = typeIn, .srcFile = file, .outFile = "build/" file ".o" }
#define DEP(file) { .type = CT_DEP, .srcFile = "", .outFile = file }
#define LDARG(a)  { .type = CT_LDARG, .srcFile = "", .outFile = a }
#define CCARG(a)  { .type = CT_CCARG, .srcFile = "", .outFile = a }
#define DIR(dir)  { .type = CT_DIR, .srcFile = "", .outFile = dir }
#define RUN(exe)  { .type = CT_RUN, .srcFile = exe,  .outFile = "" }

#define START   enum CompileResult res = CR_OK;
#define DO(cmd) res = cmd; if (res != CR_OK) return res;
#define END     return CR_OK;
#define LEN(li) (sizeof(li) / sizeof(li[0]))
#define LI(li)  li, LEN(li)
#define DO_IGNORE(cmd)  (void) cmd;

enum CompileResult test_impl(char *outFile, size_t id, struct CompileData *data, size_t dataLen);

#define START_TESTING enum CompileResult res = CR_OK; enum CompileResult resTemp;
#define END_TESTING   return res;

#define test(indir, infile, id, type, ...) \
    static struct CompileData d_##id[] = { \
        DIR("build/"), \
        DIR("build/" indir), \
        SP(type, infile), \
        RUN("build/" infile ".exe"), \
        __VA_ARGS__, \
    }; \
    resTemp = test_impl("build/" infile ".exe", id, LI(d_##id)); \
    if (resTemp != CR_OK) res = resTemp;

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// borrowed from https://stackoverflow.com/a/2741071
size_t str_escape(char *dst, const char *src, size_t dstLen)
{
    const char complexCharMap[] = "abtnvfr";

    size_t i;
    size_t srcLen = strlen(src);    
    size_t dstIdx = 0;

    // If caller wants to determine required length (supplying NULL for dst)
    // then we set dstLen to SIZE_MAX and pretend the buffer is the largest
    // possible, but we never write to it. Caller can also provide dstLen
    // as 0 if no limit is wanted.
    if (dst == NULL || dstLen == 0) dstLen = SIZE_MAX;

    for (i = 0; i < srcLen && dstIdx < dstLen; i++)
    {
        size_t complexIdx = 0;

        switch (src[i])
        {
            case '\'':
            case '\"':
            case '\\':
                if (dst && dstIdx <= dstLen - 2)
                {
                    dst[dstIdx++] = '\\';
                    dst[dstIdx++] = src[i];
                }
                else dstIdx += 2;
                break;

            case '\r': complexIdx++;
            case '\f': complexIdx++;
            case '\v': complexIdx++;
            case '\n': complexIdx++;
            case '\t': complexIdx++;
            case '\b': complexIdx++;
            case '\a':
                if (dst && dstIdx <= dstLen - 2)
                {
                    dst[dstIdx++] = '\\';
                    dst[dstIdx++] = complexCharMap[complexIdx];
                }
                else dstIdx += 2;
                break;

            default:
                if (isprint(src[i]))
                {
                    // simply copy the character
                    if (dst)
                        dst[dstIdx++] = src[i];
                    else
                        dstIdx++;
                }
                else
                {
                    // produce octal escape sequence
                    if (dst && dstIdx <= dstLen - 4)
                    {
                        dst[dstIdx++] = '\\';
                        dst[dstIdx++] = ((src[i] & 0300) >> 6) + '0';
                        dst[dstIdx++] = ((src[i] & 0070) >> 3) + '0';
                        dst[dstIdx++] = ((src[i] & 0007) >> 0) + '0';
                    }
                    else
                    {
                        dstIdx += 4;
                    }
                }
        }
    }

    if (dst && dstIdx <= dstLen)
        dst[dstIdx] = '\0';

    return dstIdx;
}

int system_impl(const char *command) {
#ifdef _WIN32
    // inefficient af but too lazy
    size_t escaped_max_len = strlen(command) * 2 + 1;
    char *escaped = malloc(escaped_max_len);
    str_escape(escaped, command, escaped_max_len);

    char *newcmd = malloc(strlen(escaped) + 1 + 10);
    sprintf(newcmd, "bash -c \"%s\"", escaped);
    free(escaped);
# if VERBOSE
    printf("$ %s\n", newcmd);
# endif
    int res = system(newcmd);
    free(newcmd);
#else
# if VERBOSE
    printf("$ %s\n", command);
# endif
    int res = system(command);
#endif

    return res;
}

#define system system_impl

#define shell(cmd) (system(cmd) == 0 ? CR_OK : CR_FAIL)

#define STR2(v) #v
#define STR(v) STR2(v)

bool file_changed(const char *path) {
    static char cmd[256];
    sprintf(cmd, SELF_PATH "/changed.sh %s", path);
    return (bool) system(cmd);
}

bool source_changed(struct CompileData *items, size_t len) {
    for (size_t i = 0; i < len; i ++)
        if (items[i].srcFile[0] != '\0')
            if (file_changed(items[i].srcFile))
                return true;
    return false;
}

#define ONLY_IF(block) { bool skip = true; block; if (skip) return CR_OK; }
#define NOT_FILE(file)   if (skip && !exists(file)) { skip = false; }
#define CHANGED(file)    if (skip && file_changed(file)) { skip = false; }
#define SRC_CHANGED(i,l) if (skip && source_changed(i,l)) { skip = false; }

#define subproject(path, outpath) \
    shell(CC" -DSERIAL_COMP=" SERIAL_COMP " -DCC=\"\\\"" CC "\\\"\" -DCXX=\"\\\"" CXX "\\\"\" -DCC_ARGS=\"\\\"" CC_ARGS "\\\"\" -DCXX_ARGS=\"\\\"" CXX_ARGS "\\\"\" -DAR=\"\\\"" AR "\\\"\" -DLD_ARGS=\"\\\"" LD_ARGS "\\\"\" -DVERBOSE=" STR(VERBOSE) " " CC_ARGS " " path " -o " outpath)

#define ss(dir, block) { DO(subproject(dir "build.c", dir "build.exe")); const char *subproject = dir; block; }

enum CompileResult ss_task_impl(const char *subproj, const char *task) {
    static char mid[] = "build.exe ";
    size_t len = strlen(subproj) + sizeof(mid) + strlen(task) + 3 + 20;
    char *cmd = malloc(len);

    sprintf(cmd, "cd %s && ./%s%s", subproj, mid, task);

    enum CompileResult res = shell(cmd);
    free(cmd);
    return res;
}

#define ss_task(task) ss_task_impl(subproject, task)

int rmdir(const char *path) {
    char *cmd = malloc(strlen(path) + 2 + sizeof(RMDIR));
    sprintf(cmd, "%s %s", RMDIR, path);
    int res = system(cmd);
    free(cmd);
    return res;
}

int rmfile(const char *path) {
    char *cmd = malloc(strlen(path) + 2 + sizeof(RMFILE));
    sprintf(cmd, "%s %s", RMFILE, path);
    int res = system(cmd);
    free(cmd);
    return res;
}

struct Target {
    char *name;
    enum CompileResult (*run)();
};

int build_main(int argc, char **argv,
               struct Target *targets, size_t targets_len) {

    if (argc == 2) {
        char *target = argv[1];

        for (size_t i = 0; i < targets_len; i++) {
            struct Target tg = targets[i];
            if (strcmp(target, tg.name) != 0)
                continue;

            enum CompileResult r = tg.run();
            if (r == CR_FAIL) {
                error("FAIL\n");
                return 1;
            } else if (r == CR_FAIL_2) {
                error("FAIL_2\n");
                return 1;
            }
            return 0;
        }
    }

    error("Invalid target! Targets:\n");
    for (size_t i = 0; i < targets_len; i ++)
        fprintf(stderr, "- %s\n", targets[i].name);
    return 2;
}

void *compileThread(void *arg) {
    struct CompileThreadData * ctd = arg;
    struct CompileData *data = ctd->data;
    if (data->type == CT_C) {
        char *args = malloc(strlen(data->srcFile) + strlen(data->outFile) +
                            sizeof(CC) + 9 + sizeof(CC_ARGS) + ctd->ccArgsLen);
        sprintf(args, "%s -c %s -o %s " CC_ARGS " %s", CC, data->srcFile, data->outFile, ctd->ccArgs);

        int res = system(args);
        free(args);
        if (res != 0) {
            printf("%i\n", res);
            return (void *) CR_FAIL;
        }
    } else if (data->type == CT_CXX) {
        char *args = malloc(strlen(data->srcFile) + strlen(data->outFile) +
                            sizeof(CXX) + 9 + sizeof(CXX_ARGS) + ctd->ccArgsLen);
        sprintf(args, "%s -c %s -o %s " CXX_ARGS " %s", CXX, data->srcFile, data->outFile, ctd->ccArgs);

        int res = system(args);
        free(args);
        if (res != 0) {
            printf("%i\n", res);
            return (void *) CR_FAIL;
        }
    } else if (data->type == CT_CDEF) {
        char* cmd = malloc(2 + sizeof(PYTHON) + sizeof(SELF_PATH) + strlen(data->srcFile) + 7);
        sprintf(cmd, PYTHON " " SELF_PATH "cdef.py %s", data->srcFile);
        int res = system(cmd);
        free(cmd);
        if (res != 0) {
            return (void*) CR_FAIL;
        }

        size_t ofl = strlen(data->outFile);
        char * cfile = malloc(ofl + 1);
        memcpy(cfile, data->outFile, ofl + 1);
        cfile[ofl - 1] = 'c';
        
        data->srcFile = cfile;
        data->type = CT_C;
        enum CompileResult r = (intptr_t) compileThread(arg);
        free(cfile);
        if (r != CR_OK)
            return (void*) r; 
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
        if (cd.type == CT_DIR || cd.type == CT_CCARG)
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
        if (cd.type == CT_DIR || cd.type == CT_CCARG)
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

int mkdir(const char *path) {
#ifdef _WIN32
    if (exists(path))
        return 0;
#endif
    char *args = malloc(strlen(path) + sizeof(MKDIR) + 1);
    sprintf(args, "%s %s", MKDIR, path);
    int res = system(args);
    free(args);
    return res;
}

enum CompileResult compile(struct CompileData *objs, size_t len) {
    size_t extraArgsLen = 0;
    for (size_t i = 0; i < len; i ++) {
        if (objs[i].type == CT_CCARG) {
            extraArgsLen += strlen(objs[i].outFile) + 1;
        }
    }

    char * extraArgs = malloc(extraArgsLen + 1);
    extraArgs[0] = 0;
    for (size_t i = 0; i < len; i ++) {
        if (objs[i].type == CT_CCARG) {
            if (i > 0) {
                strcat(extraArgs, " ");
            }
            strcat(extraArgs, objs[i].outFile);
        }
    }

    for (size_t i = 0; i < len; i ++) {
        struct CompileData *data = &objs[i];

        if (data->type == CT_DIR) {
            (void) mkdir(data->outFile);
        }
    }

#if SERIAL_COMP 
    for (size_t i = 0; i < len; i ++) {
        struct CompileData *data = &objs[i];

        if (data->type == CT_CCARG || data->type == CT_DIR) continue;

        struct CompileThreadData ctd;
        ctd.data = data;
        ctd.ccArgs = extraArgs;
        ctd.ccArgsLen = extraArgsLen;
        enum CompileResult res = (enum CompileResult) (intptr_t) compileThread(&ctd);
        if (res != CR_OK)
            return res;
    }
    free(extraArgs);
    return CR_OK;
#else 
    pthread_t *threads = malloc(sizeof(pthread_t) * len);
    struct CompileThreadData *datas = malloc(sizeof(struct CompileThreadData) * len);

    for (size_t i = 0; i < len; i ++) {
        struct CompileData *data = &objs[i];        

        if (data->type == CT_DIR || data->type == CT_CCARG) continue;
        datas[i].data = data;
        datas[i].ccArgs = extraArgs;
        datas[i].ccArgsLen = extraArgsLen;
        pthread_create(&threads[i], NULL, compileThread, &datas[i]);
    }

    for (size_t i = 0; i < len; i ++) {
        if (objs[i].type == CT_DIR || objs[i].type == CT_CCARG) continue;

        void *resr;
        pthread_join(threads[i], &resr);
        enum CompileResult res = (enum CompileResult) (intptr_t) resr;

        if (res != CR_OK) {
            for (size_t j = 0; j < len; j ++) {
                if (i == j)
                    continue;

                if (objs[j].type == CT_DIR || objs[j].type == CT_CCARG) continue;
                pthread_kill(threads[j], SIGABRT);
            }

            free(threads);
            free(extraArgs);
            free(datas);
            return CR_FAIL;
        }
    }

    free(threads);
    free(extraArgs);
    free(datas);
    return CR_OK;
#endif 
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
            error("Missing executable:\n");
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

enum CompileResult test_impl_ex(char *outFile, const char *name, struct CompileData *data, size_t dataLen) {
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

    printf("%s: OK\n", name);
    return CR_OK;

fail:
    printf("%s: FAIL\n", name);
    return CR_FAIL;
}

enum CompileResult test_impl(char *outFile, size_t id, struct CompileData *data, size_t dataLen) {
    char buf[64];
    sprintf(buf, "Test %zu", id);
    return test_impl_ex(outFile, buf, data, dataLen);
}

enum CompileResult test_dir(const char *dirp, struct CompileData *data, size_t dataLen) {
    START_TESTING;

    DIR *dir = opendir(dirp);

    struct dirent *dp = NULL;
    while ((dp = readdir(dir))) {
        if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, "..")) {
            continue;
        }

        static char infile[256];
        sprintf(infile, "%s/%s", dirp, dp->d_name);

        static char outfile[256];
        sprintf(outfile, "build/%s/%s.exe", dirp, dp->d_name);

        static char objfile[256];
        sprintf(objfile, "build/%s/%s.o", dirp, dp->d_name);

        struct CompileData * dataex = malloc(sizeof(struct CompileData) * (dataLen + 2));
        memcpy(dataex + 2, data, sizeof(struct CompileData) * dataLen);
        dataex[0] = (struct CompileData) { .type = CT_C, .srcFile = infile, .outFile = objfile };
        dataex[1] = (struct CompileData) RUN(outfile);

        resTemp = test_impl_ex(outfile, dp->d_name, dataex, dataLen + 2);
        free(dataex);
        if (resTemp != CR_OK) res = resTemp;
    }

    (void) closedir(dir);

    END_TESTING;
}
