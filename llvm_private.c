#define _DEBUG
#define _GNU_SOURCE
#define __STDC_LIMIT_MACROS
#define __STDC_CONSTANT_MACROS

#include <assert.h>
#include <stdio.h>

#include <llvm-c/Core.h>

#include "escheme.h"

typedef struct {
    const char *name;
    const Scheme_Object *value;
} global_def;

/*
  General utility functions
*/
static inline Scheme_Object* make_cptr(const void *cptr, const char *ctag)
{
    Scheme_Object* tag;
    Scheme_Object* ret;

    tag = scheme_make_symbol(ctag);
    ret = scheme_make_cptr(cptr, tag);

    return ret;
}

/*
  Type operations
*/

/*
  Module operations
*/

static void module_destroy(void *p, void *data)
{
    LLVMModuleRef mod;
    assert(SCHEME_CPTRP(p));

    mod = SCHEME_CPTR_VAL(p);
    assert(mod);

    fprintf(stderr, "Destroying module <%p> !\n", mod);
    fflush(stderr);

    LLVMDisposeModule(mod);
}

static Scheme_Object* module_load(int argc, Scheme_Object **argv)
{
    Scheme_Object *ret;
    Scheme_Object *path;
    Scheme_Object *tag;
    LLVMMemoryBufferRef buf;
    LLVMModuleRef mod;
    char *error;
    int fail;

    assert(SCHEME_CHAR_STRINGP(argv[0]));
    path = scheme_char_string_to_path(argv[0]);

    // FIXME: we need to consult the current scheme security guard.
    fail = LLVMCreateMemoryBufferWithContentsOfFile(SCHEME_PATH_VAL(path), &buf, &error);
    if(fail) {
	// FIXME: we may need to call LLVMDisposeMessage here.
	scheme_signal_error("Could not load LLVM module \"%Q\": %s.\n", argv[0], error);
    }
    assert(buf);

    fail = LLVMParseBitcode(buf, &mod, &error);
    LLVMDisposeMemoryBuffer(buf);
    if(fail) {
	// FIXME: we may need to call LLVMDisposeMessage here.
	scheme_signal_error("Could not read LLVM module \"%Q\": %s.\n", argv[0], error);
    }
    assert(mod);
    fprintf(stderr, "New module <%p> !\n", mod);

    ret = make_cptr(mod, "llvm-module");
    scheme_add_finalizer(ret, module_destroy, scheme_void);

    return ret;
}

static Scheme_Object* module_new(int argc, Scheme_Object **argv)
{
    Scheme_Object *ret;
    Scheme_Object *byte_name;
    Scheme_Object *tag;
    LLVMModuleRef mod;

    assert(SCHEME_CHAR_STRINGP(argv[0]));
    byte_name = scheme_char_string_to_byte_string(argv[0]);

    mod = LLVMModuleCreateWithName(SCHEME_BYTE_STR_VAL(byte_name));
    scheme_add_finalizer(ret, module_destroy, scheme_void);

    ret = make_cptr(mod, "llvm-module");

    return ret;
}

static Scheme_Object* module_dump(int argc, Scheme_Object **argv)
{
    LLVMModuleRef mod;
    assert(SCHEME_CPTRP(argv[0]));
    mod = SCHEME_CPTR_VAL(argv[0]);
    assert(mod);

    LLVMDumpModule(mod);

    return scheme_void;
}

/*
  Scheme initialization
*/

Scheme_Object* scheme_initialize(Scheme_Env* env)
{
    Scheme_Env *module=NULL;
    Scheme_Object *name=NULL;
    Scheme_Object *tmp=NULL;

    name = scheme_intern_symbol("llvm_private");
    module = scheme_primitive_module(name, env);

    /* Module operations */
    tmp = scheme_make_prim_w_arity(module_load, "llvm-module-load", 1, 1);
    scheme_add_global("llvm-module-load", tmp, module);
    tmp = scheme_make_prim_w_arity(module_new, "llvm-module-new", 1, 1);
    scheme_add_global("llvm-module-new", tmp, module);
    tmp = scheme_make_prim_w_arity(module_dump, "llvm-module-dump", 1, 1);
    scheme_add_global("llvm-module-dump", tmp, module);

    scheme_finish_primitive_module(module);


    return scheme_make_utf8_string("LLVM is go");
}

Scheme_Object* scheme_reload(Scheme_Env* env)
{
    return scheme_initialize(env);
}

Scheme_Object* scheme_module_name(void)
{
    return scheme_intern_symbol("llvm_private");
}
