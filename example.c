#include "build.h"

/* ========================================================================= */

struct CompileData target_lib_files[] = {
    DIR("build/"),

    SP(CT_C, "a.c"),
    SP(CT_C, "b.c"),
};

enum CompileResult target_lib() {
    START;
    DO(compile(LI(target_lib_files)));
    DO(linkTask(LI(target_lib_files), "build/lib.a"));
    END;
}

/* ========================================================================= */

enum CompileResult target_tests() {
    START;
    test("", "test.c", 0, CT_C,
            DEP("build/lib.a"));
    END;
}

/* ========================================================================= */

struct Target targets[] = {
	{ .name = "lib.a", .run = target_lib },
	{ .name = "tests", .run = target_tests },
};

#define TARGETS_LEN (sizeof(targets) / sizeof(targets[0]))

int main(int argc, char **argv) {
    return build_main(argc, argv, targets, TARGETS_LEN);
}
