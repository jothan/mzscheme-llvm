/*
  MzScheme to LLVM 2.6 wrapper
  Copyright (C) 2010  Jonathan Bastien-Filiatrault

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/* FIXME: This is broken, we need to define this according to llvm-config. */
#define _DEBUG
#define _GNU_SOURCE
#define __STDC_LIMIT_MACROS
#define __STDC_CONSTANT_MACROS

#include <assert.h>
#include <stdbool.h>
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
static inline Scheme_Object* cptr_make(void *cptr, const char *ctag)
{
    Scheme_Object* tag;
    Scheme_Object* ret;

    tag = scheme_make_symbol(ctag);
    ret = scheme_make_cptr(cptr, tag);

    return ret;
}

static inline bool cptr_check(Scheme_Object *cptr, const char *tag1)
{
    if(SCHEME_CPTRP(cptr) && SCHEME_SYMBOLP(SCHEME_CPTR_TYPE(cptr))) {
	return strcmp(tag1, SCHEME_SYM_VAL(SCHEME_CPTR_TYPE(cptr))) == 0;
    } else {
	return false;
    }
}


/*
  Type operations
*/

/*
  Get a reference to an integer type of an arbitrary size.
  argv[0]: int size in bits
*/
static Scheme_Object* type_int(int argc, Scheme_Object **argv)
{
    assert(SCHEME_INTP(argv[0]));

    return cptr_make(LLVMIntType(SCHEME_INT_VAL(argv[0])), "llvm-type");
}

/*
  Get references to common integer types.
*/

static Scheme_Object* type_int1(int argc, Scheme_Object **argv)
{
    return cptr_make(LLVMInt1Type(), "llvm-type");
}

static Scheme_Object* type_int8(int argc, Scheme_Object **argv)
{
    return cptr_make(LLVMInt8Type(), "llvm-type");
}

static Scheme_Object* type_int16(int argc, Scheme_Object **argv)
{
    return cptr_make(LLVMInt16Type(), "llvm-type");
}

static Scheme_Object* type_int32(int argc, Scheme_Object **argv)
{
    return cptr_make(LLVMInt32Type(), "llvm-type");
}

static Scheme_Object* type_int64(int argc, Scheme_Object **argv)
{
    return cptr_make(LLVMInt64Type(), "llvm-type");
}

/*
  Get the width of an integer type.
  argv[0]: Integer type
*/
static Scheme_Object* type_int_width(int argc, Scheme_Object **argv)
{
    assert(cptr_check(argv[0], "llvm-type"));

    return scheme_make_integer_value(LLVMGetIntTypeWidth(SCHEME_CPTR_VAL(argv[0])));
}

/*
  Get references to floating point types
*/

static Scheme_Object* type_float(int argc, Scheme_Object **argv)
{
    return cptr_make(LLVMFloatType(), "llvm-type");
}

static Scheme_Object* type_double(int argc, Scheme_Object **argv)
{
    return cptr_make(LLVMDoubleType(), "llvm-type");
}

static Scheme_Object* type_x86fp80(int argc, Scheme_Object **argv)
{
    return cptr_make(LLVMX86FP80Type(), "llvm-type");
}

static Scheme_Object* type_fp128(int argc, Scheme_Object **argv)
{
    return cptr_make(LLVMFP128Type(), "llvm-type");
}

static Scheme_Object* type_ppcfp128(int argc, Scheme_Object **argv)
{
    return cptr_make(LLVMPPCFP128Type(), "llvm-type");
}

/*
  Module operations
*/

/*
  LLVM module destructor, invoked by mzscheme's garbage collector.
*/
static void module_destroy(void *p, void *data)
{
    LLVMModuleRef mod;
    assert(cptr_check(p, "llvm-module"));

    mod = SCHEME_CPTR_VAL(p);
    assert(mod);

    fprintf(stderr, "Destroying module <%p> !\n", mod);
    fflush(stderr);

    LLVMDisposeModule(mod);
}

/*
  Load a LLVM module from a bitcode file.
  argv[0]: path to bitcode file
*/
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

    ret = cptr_make(mod, "llvm-module");
    scheme_add_finalizer(ret, module_destroy, scheme_void);

    return ret;
}

/*
  Create a new LLVM module.
  argv[0]: Module name
*/
static Scheme_Object* module_new(int argc, Scheme_Object **argv)
{
    Scheme_Object *ret;
    Scheme_Object *byte_name;
    Scheme_Object *tag;
    LLVMModuleRef mod;

    assert(SCHEME_CHAR_STRINGP(argv[0]));
    byte_name = scheme_char_string_to_byte_string(argv[0]);

    mod = LLVMModuleCreateWithName(SCHEME_BYTE_STR_VAL(byte_name));

    ret = cptr_make(mod, "llvm-module");
    scheme_add_finalizer(ret, module_destroy, scheme_void);

    return ret;
}

/*
  Dump a LLVM module to stderr.
  argv[0]: LLVM module
*/
static Scheme_Object* module_dump(int argc, Scheme_Object **argv)
{
    LLVMModuleRef mod;
    assert(cptr_check(argv[0], "llvm-module"));
    mod = SCHEME_CPTR_VAL(argv[0]);
    assert(mod);

    LLVMDumpModule(mod);

    return scheme_void;
}

/*
  Scheme initialization
*/

static inline void register_function(Scheme_Env *module,
				     const char *name,
				     Scheme_Prim *pfunc,
				     int arg_min,
				     int arg_max)
{
    Scheme_Object *func;

    func = scheme_make_prim_w_arity(pfunc, name, arg_min, arg_max);
    scheme_add_global(name, func, module);
}

struct module_function {
    const char *name;
    Scheme_Prim *code;
    int arg_min;
    int arg_max;
};

static const struct module_function functions[] = {
    /* Type operations */
    {"llvm-type-int",       type_int,       1, 1},
    {"llvm-type-int1",      type_int1,      0, 0},
    {"llvm-type-int8",      type_int8,      0, 0},
    {"llvm-type-int16",     type_int16,     0, 0},
    {"llvm-type-int32",     type_int32,     0, 0},
    {"llvm-type-int64",     type_int64,     0, 0},
    {"llvm-type-int-width", type_int_width, 1, 1},
    {"llvm-type-float",     type_float,     0, 0},
    {"llvm-type-double",    type_double,    0, 0},
    {"llvm-type-x86fp80",   type_x86fp80,   0, 0},
    {"llvm-type-fp128",     type_fp128,     0, 0},
    {"llvm-type-ppcfp128",  type_ppcfp128,  0, 0},
    /* Module operations */
    {"llvm-module-load", module_load, 1, 1},
    {"llvm-module-new",  module_new,  1, 1},
    {"llvm-module-dump", module_dump, 1, 1},
    {NULL, NULL, 0, 0},
};


Scheme_Object* scheme_initialize(Scheme_Env* env)
{
    Scheme_Env *module=NULL;
    Scheme_Object *name=NULL;
    Scheme_Object *tmp=NULL;
    const struct module_function *f;

    name = scheme_intern_symbol("llvm_private");
    module = scheme_primitive_module(name, env);

    for(f = functions; f->name != NULL; f = f XFORM_TRUST_PLUS 1) {
	register_function(module, f->name, f->code, f->arg_min, f->arg_max);
    }

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
