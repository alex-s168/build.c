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

#ifndef VERBOSE
# define VERBOSE   0
#endif 

#ifndef SELF_PATH
# define SELF_PATH "build_c/"
#endif 

#if !defined(SERIAL_COMP)
# define SERIAL_COMP 0
#endif 

/* ========================================================================= */

#define _CRT_SECURE_NO_WARNINGS

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

#include "utils.h"

#if !SERIAL_COMP
# define _UTHREAD_H_IMPL
# include "uthread.h" 
# include "mutex.h"
#endif

#ifdef _WIN32
# include <io.h>
#else 
# include <signal.h>
# include <unistd.h>
#endif 

#define MINIRENT_IMPLEMENTATION
#include "minirent.h"

#define SLOWDB_IMPL
#include "slowdb/inc/slowdb.h"

#define error(msg, ...) fprintf(stderr, msg, ##__VA_ARGS__)

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
#define NOLD_DEP(file) { .type = CT_NOC, .srcFile = "", .outFile = file }
#define LDARG(a)  { .type = CT_LDARG, .srcFile = "", .outFile = a }
#define CCARG(a)  { .type = CT_CCARG, .srcFile = "", .outFile = a }
#define DIR(dir)  { .type = CT_DIR, .srcFile = "", .outFile = dir }
#define RUN(exe)  { .type = CT_RUN, .srcFile = exe,  .outFile = "" }

#define START   enum CompileResult res = CR_OK;
#define DO(cmd) res = cmd; if (res != CR_OK) return res;
#define END     return CR_OK;
#define LEN(li) (sizeof(li) / sizeof(li[0]))
#define LI(li)  li, LEN(li)
#define VLI(li) li.data, li.len 
#define DO_IGNORE(cmd)  (void) cmd;

typedef struct {
    struct CompileData *data;
    size_t len;
} VaList;

static VaList newVaList(void) {
    return (VaList) { NULL, 0 };
}

static void vaListFree(VaList vl) {
    free(vl.data);
}

static VaList vaListConcat(VaList a, VaList b)
{
    VaList res = newVaList();
    res.data = (struct CompileData *) malloc(sizeof(struct CompileData) * (a.len + b.len));
    res.len = a.len + b.len;
    memcpy(res.data, a.data, sizeof(struct CompileData) * a.len);
    memcpy(res.data + a.len, b.data, sizeof(struct CompileData) * b.len);
    return res;
}

#define ASVAR(arr) ((VaList) { LI(arr) })

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

slowdb* changedDb = NULL;
#if !SERIAL_COMP
mutex_t changedMut;
#endif

#ifdef _WIN32 

int file_mod_time(const char * path, time_t * out)
{
    char buf[512];
    if (!GetFullPathNameA(path, sizeof(buf), buf, NULL))
        return 1;

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ,  NULL,  OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (file == INVALID_HANDLE_VALUE)
        return 1;

    FILETIME ftMod;
    if (!GetFileTime(file, NULL, NULL, &ftMod)) {
        CloseHandle(file);
        return 1;
    }

    CloseHandle(file);

    FILETIME lftMod;
    FileTimeToLocalFileTime(&ftMod, &lftMod);

    SYSTEMTIME st;
    FileTimeToSystemTime(&lftMod, &st);

    struct tm tm;
    tm.tm_sec = st.wSecond;
    tm.tm_min = st.wMinute;
    tm.tm_hour = st.wHour;
    tm.tm_mday = st.wDay;
    tm.tm_mon = st.wMonth - 1;
    tm.tm_year = st.wYear - 1900;
    tm.tm_isdst = -1;

    *out = mktime(&tm);
    return 0;
}

#else

#include <sys/stat.h>
int file_mod_time(const char * path, time_t * out)
{
    FILE* f = fopen(path, "r");
    if (f == NULL) {
        DIR* d = opendir(path);
        if (d == NULL) return 1;
        int fd = dirfd(d);
        struct stat st;
        fstat(fd, &st);
        *out = st.st_mtime;
        closedir(d);

        return 0;
    }

    struct stat st;
    fstat(fileno(f), &st);
    *out = st.st_mtime;
    fclose(f);
    return 0;
}

#endif 

int cdb_get(const char * key, time_t * out) {
#if !SERIAL_COMP
    mutex_lock(&changedMut);
#endif 

    void * ptr = slowdb_get(changedDb,
            (const unsigned char *) key, strlen(key) + 1,
            NULL);

    if (ptr != NULL) {
        struct tm * t = (struct tm *) ptr;
        *out = mktime(t);
        free(ptr);
    }

#if !SERIAL_COMP
    mutex_unlock(&changedMut);
#endif 

    return ptr == NULL;
}

/**  
 * example:
 * \code 
 * if (cdb_notHaveAndSet("_pip_genericlexer")) {
 *     system("pip install genericlexer");
 * }
 * \endcode 
 */ 
bool cdb_notHaveAndSet(const char * key) { 
#if !SERIAL_COMP
    mutex_lock(&changedMut);
#endif 

    void * ptr = slowdb_get(changedDb,
            (const unsigned char *) key, strlen(key) + 1,
            NULL);

    if (ptr == NULL) {
        unsigned char p = 0;
        slowdb_put(changedDb,
                (const unsigned char *) key, strlen(key) + 1,
                &p, sizeof(p));
    }

#if !SERIAL_COMP
    mutex_unlock(&changedMut);
#endif 

    free(ptr);

    return ptr == NULL;
}

char *cdb_getStrHeap(const char * key) {
#if !SERIAL_COMP
    mutex_lock(&changedMut);
#endif 

    int len;
    void * ptr = slowdb_get(changedDb,
            (const unsigned char *) key, strlen(key) + 1,
            &len);

#if !SERIAL_COMP
    mutex_unlock(&changedMut);
#endif 

    return (char *) ptr;
}

/** ONLY CAN BE CALLED ONCE PER KEY EVER */ 
void cdb_setStrInitial(const char * key, const char * value) {
#if !SERIAL_COMP
    mutex_lock(&changedMut);
#endif 

    slowdb_put(changedDb,
            (const unsigned char *) key, strlen(key) + 1,
            (const unsigned char *) value, strlen(value) + 1);

#if !SERIAL_COMP
    mutex_unlock(&changedMut);
#endif 
}

void cdb_set(const char * key, time_t const* time) {
    struct tm * t = localtime(time);

#if !SERIAL_COMP
    mutex_lock(&changedMut);
#endif 

    slowdb_replaceOrPut(changedDb,
            (const unsigned char *) key, strlen(key) + 1,
            (const unsigned char *) t, sizeof(struct tm));

#if !SERIAL_COMP
    mutex_unlock(&changedMut);
#endif 
}

void byte_to_hex(char* out, uint8_t byte, bool upper) {
	static char tab_up[16] = { '0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F' };
	static char tab_down[16] = { '0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f' };
	char* tab = upper ? tab_up : tab_down;

	out[0] = tab[byte & 0xF];
	out[1] = tab[(byte >> 4) & 0xF];
}

void print_bytes_hex(FILE* out, uint8_t const* src, size_t srclen, bool upper, bool commas) {
	char by[2];
	for (size_t i = 0; i < srclen; i ++) {
		if (i > 0) {
			if (commas) fputc(',', out);
			fputc(' ', out);
		}

		byte_to_hex(by, src[i], upper);
		fwrite(by, sizeof(char), 2, out);
	}
}

bool file_changed(const char *path) {
    time_t cached;
    if (cdb_get(path, &cached)) {
        if (file_mod_time(path, &cached)) {
            error("file_mod_time(\"%s\") failed\n", path);
        } else {
            cdb_set(path, &cached);
        }
        return true;
    }

    time_t mod;
    if (file_mod_time(path, &mod)) {
        error("file_mod_time(\"%s\") failed\n", path);
        return true;
    }

/*
	printf("cached match data of %s: %i:\n", path, cached == mod);
	printf("  ["); print_bytes_hex(stdout, (uint8_t*) &cached, sizeof(cached), false, true); printf("]\n");
	printf("  ["); print_bytes_hex(stdout, (uint8_t*) &mod, sizeof(mod), false, true); printf("]\n");
*/

    if (cached == mod)
        return false;

    cdb_set(path, &mod);
    return true;
}

bool source_changed(struct CompileData *items, size_t len) {
    bool any = false;
    for (size_t i = 0; i < len; i ++)
        if (items[i].srcFile[0] != '\0')
            if (file_changed(items[i].srcFile))
                any = true;
    return any;
}

const char * findPython(void) {
    static const char * ptr = NULL;
    if (ptr) return ptr;

    ptr = cdb_getStrHeap("_py_path");
    if (ptr) return ptr;

    if (exists("venv/bin/python3"))
        ptr = "venv/bin/python3";
    else if (exists("venv/Scripts/python"))
        ptr = "venv/Scripts/python";
    else if (system("python --help &>/dev/null") == 0)
        ptr = "python";
    else if (system("python3 --help &>/dev/null") == 0)
        ptr = "python3";
    else {
        error("python not found! tried \"python\" and \"python3\"! Set \"python\" variable or install python!\n");   
        return NULL;
    }

    cdb_setStrInitial("_py_path", ptr);

    return ptr;
}

/** 
 * Example:
 * \code 
 * if (withPipPackage("aaaf")) {
 *     // do stuff with pkg
 * }
 * /endcode 
 */
bool withPipPackage(const char * pkg) {
    char pkgb[64];
    sprintf(pkgb, "_py_mod_%s", pkg);

    if (cdb_notHaveAndSet(pkgb)) {
        const char * py = findPython();
        if (!py) return false;

        char buf[512];
        sprintf(buf, "%s -m pip install %s", py, pkg);

        if (system(buf) != 0)
            return false;
    }

    return true;
}

#define ONLY_IF(block) { bool skip = true; block; if (skip) return CR_OK; }
#define NOT_FILE(file)   if (skip && !exists(file)) { skip = false; }
#define CHANGED(file)    if (skip && file_changed(file)) { skip = false; }
#define SRC_CHANGED(i,l) if (skip && source_changed(i,l)) { skip = false; }

#define subproject(args, path, outpath) \
    shell(CC" -DSERIAL_COMP=" STR(SERIAL_COMP) " -DCC=\"\\\"" CC "\\\"\" -DCXX=\"\\\"" CXX "\\\"\" -DCC_ARGS=\"\\\"" CC_ARGS "\\\"\" -DCXX_ARGS=\"\\\"" CXX_ARGS "\\\"\" -DAR=\"\\\"" AR "\\\"\" -DLD_ARGS=\"\\\"" LD_ARGS "\\\"\" -DVERBOSE=" STR(VERBOSE) " " CC_ARGS " " LD_ARGS " " path " -o " outpath " " args)

#define ssx(dir, args, block) { DO(subproject(args, dir "build.c", dir "build.exe")); const char *subproject = dir; block; }
#define ss(dir, block) ssx(dir, "", block)

bool skipLinkFor(enum CompileType t) {
    return t == CT_RUN || t == CT_DIR || t == CT_NOC || t == CT_CCARG;
}

bool skipCompileFor(enum CompileType t) {
    return t == CT_RUN || t == CT_DIR || t == CT_NOC || t == CT_LDARG;
}

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
        char *targetss = argv[1];

        int cod = 0;

        changedDb = slowdb_open("build.slowdb");
        assert(changedDb);
#if !SERIAL_COMP
        mutex_create(&changedMut);
#endif

        SPLITERATE(targetss, ",", target) {
            bool found = false;
            for (size_t i = 0; i < targets_len; i++) {
                struct Target tg = targets[i];
                if (strcmp(target, tg.name) != 0)
                    continue;
                found = true;

                enum CompileResult r = tg.run();

                if (r == CR_FAIL) {
                    error("FAIL\n");
                    cod = 1;
                    break;
                } else if (r == CR_FAIL_2) {
                    error("FAIL_2\n");
                    cod = 1; 
                    break;
                }
            }
            if (!found) {
                error("Invalid target %s! Targets:\n", target);
                for (size_t i = 0; i < targets_len; i ++)
                    fprintf(stderr, "- %s\n", targets[i].name);
                cod = 2;
                break;
            }
        }

#if !SERIAL_COMP
        mutex_destroy(&changedMut);
#endif 
        slowdb_close(changedDb);

        return cod;
    }

    error("Usage: %s target1,target2,...", argv[0]);
    error("Available Targets:\n");
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
        const char * python = findPython();
        char* cmd = malloc(3 + strlen(python) + sizeof(SELF_PATH) + strlen(data->srcFile) + 7);
        sprintf(cmd, "%s " SELF_PATH "cdef.py %s", python, data->srcFile);
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
        if (skipLinkFor(cd.type))
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
        if (skipLinkFor(cd.type))
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

int makeDir(const char *path) {
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
            (void) makeDir(data->outFile);
        }
    }

#if SERIAL_COMP 
    for (size_t i = 0; i < len; i ++) {
        struct CompileData *data = &objs[i];

        if (skipCompileFor(data->type)) continue;

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
    uthread_t *threads = malloc(sizeof(uthread_t) * len);
    struct CompileThreadData *datas = malloc(sizeof(struct CompileThreadData) * len);

    for (size_t i = 0; i < len; i ++) {
        struct CompileData *data = &objs[i];        

        if (skipCompileFor(data->type)) continue;
        datas[i].data = data;
        datas[i].ccArgs = extraArgs;
        datas[i].ccArgsLen = extraArgsLen;
        uthread_create(&threads[i], compileThread, &datas[i]);
    }

    for (size_t i = 0; i < len; i ++) {
        if (skipCompileFor(objs[i].type)) continue;

        void *resr;
        uthread_join(&threads[i], &resr);
        enum CompileResult res = (enum CompileResult) (intptr_t) resr;

        if (res != CR_OK) {
            for (size_t j = 0; j < len; j ++) {
                if (i == j)
                    continue;

                if (skipCompileFor(objs[j].type)) continue;
                uthread_kill(&threads[j]);
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
            error("%s", f);
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
            error("%s", f);
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

		char ext = dp->d_name[strlen(dp->d_name)-1];
		if (ext == 'h')
			continue;

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

#define automain(targets) \
    int main(int argc, char **argv) { \
        return build_main(argc, argv, targets, (sizeof(targets) / sizeof(targets[0]))); \
    }
