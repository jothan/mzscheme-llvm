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

#include <llvm-c/Core.h>
#include <llvm-c/BitReader.h>

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>


#include "escheme.h"

static const char *license =
    "MzScheme to LLVM 2.6 wrapper\n"
    "Copyright (C) 2010  Jonathan Bastien-Filiatrault\n"
    "\n"
    "This program is free software: you can redistribute it and/or modify\n"
    "it under the terms of the GNU General Public License as published by\n"
    "the Free Software Foundation, either version 3 of the License, or\n"
    "(at your option) any later version.\n"
    "\n"
    "This program is distributed in the hope that it will be useful, but\n"
    "WITHOUT ANY WARRANTY; without even the implied warranty of\n"
    "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU\n"
    "General Public License for more details.\n"
    "\n"
    "You should have received a copy of the GNU General Public License\n"
    "along with this program.  If not, see <http://www.gnu.org/licenses/>.\n";

static const char *disclaimer =
    "MzScheme to LLVM 2.6 wrapper\n"
    "Copyright (C) 2010  Jonathan Bastien-Filiatrault\n"
    "This program comes with ABSOLUTELY NO WARRANTY;\n"
    "for details type (display llvm-wrapper-nowarranty)\n"
    "This is free software, and you are welcome to redistribute it\n"
    "under certain conditions; type (display llvm-wrapper-license)\n"
    "for details.\n";

static const char *nowarranty =
    "THERE IS NO WARRANTY FOR THE PROGRAM, TO THE EXTENT PERMITTED BY\n"
    "APPLICABLE LAW.  EXCEPT WHEN OTHERWISE STATED IN WRITING THE COPYRIGHT\n"
    "HOLDERS AND/OR OTHER PARTIES PROVIDE THE PROGRAM \"AS IS\" WITHOUT WARRANTY\n"
    "OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING, BUT NOT LIMITED TO,\n"
    "THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR\n"
    "PURPOSE.  THE ENTIRE RISK AS TO THE QUALITY AND PERFORMANCE OF THE PROGRAM\n"
    "IS WITH YOU.  SHOULD THE PROGRAM PROVE DEFECTIVE, YOU ASSUME THE COST OF\n"
    "ALL NECESSARY SERVICING, REPAIR OR CORRECTION.\n";


/*
  General utility functions
*/
static inline Scheme_Object* cptr_make(void *cptr, const char *ctag)
{
    Scheme_Object* tag;
    Scheme_Object* ret;

    tag = scheme_make_symbol(ctag);
    ret = scheme_make_external_cptr(cptr, tag);

    return ret;
}

static inline bool cptr_check(const Scheme_Object *cptr, const char *tag1)
{
    if(SCHEME_CPTRP(cptr) && SCHEME_SYMBOLP(SCHEME_CPTR_TYPE(cptr))) {
	return strcmp(tag1, SCHEME_SYM_VAL(SCHEME_CPTR_TYPE(cptr))) == 0;
    } else {
	return false;
    }
}

static inline bool value_check(const Scheme_Object *cptr)
{
    return cptr_check(cptr, "llvm-value") && SCHEME_CPTR_VAL(cptr) != NULL;
}


/*
  Mutation-agnostic pair accessors.
*/

/* The CAR and CDR functions return a const pointer to avoid the use
   of the returned value as an l-value. */
static inline const Scheme_Object* PAIR_CAR(const Scheme_Object *obj)
{
    if(SCHEME_PAIRP(obj)) {
	return SCHEME_CAR(obj);
    } else if(SCHEME_MPAIRP(obj)) {
	return SCHEME_MCAR(obj);
    } else {
	abort();
	return NULL;
    }
}

static inline const Scheme_Object* PAIR_CDR(const Scheme_Object *obj)
{
    if(SCHEME_PAIRP(obj)) {
	return SCHEME_CDR(obj);
    } else if(SCHEME_MPAIRP(obj)) {
	return SCHEME_MCDR(obj);
    } else {
	abort();
	return NULL;
    }
}

static inline bool PAIRP(const Scheme_Object *obj)
{
    return SCHEME_PAIRP(obj) || SCHEME_MPAIRP(obj);
}

static int list_length(const Scheme_Object *obj)
{
    const Scheme_Object *p;
    int len;

    for(p=obj, len=0; ; p = PAIR_CDR(p)) {
	if(PAIRP(p)) {
	    len += 1;
	} else if(SCHEME_NULLP(p)) {
	    return len;
	} else {
	    return -1;
	}
    }
}

/*
  Type operations
*/

/*
  Get a reference to the void type.
*/
static Scheme_Object* type_void(int argc, Scheme_Object **argv)
{
    return cptr_make(LLVMVoidType(), "llvm-type");
}
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
  Return a pointer type
  argv[0]: Type that is pointed to
*/
static Scheme_Object* type_pointer(int argc, Scheme_Object **argv)
{
    assert(cptr_check(argv[0], "llvm-type"));

    return cptr_make(LLVMPointerType(SCHEME_CPTR_VAL(argv[0]), 0), "llvm-type");
}

/*
  Operations on function types
*/

/*
  Get a reference to a function type
  argv[0]: Return type
  argv[1]: List of parameter types
  argv[2]: Boolean to indicate if the function is vararg (optional)
*/
static Scheme_Object* type_function(int argc, Scheme_Object **argv)
{
    Scheme_Object *vararg=scheme_false;
    int list_len;
    int i;
    LLVMTypeRef *param_types;
    LLVMTypeRef ret;
    const Scheme_Object *param;
    const Scheme_Object *val;
    if(argc == 3) {
	vararg = argv[2];
    }
    assert(cptr_check(argv[0], "llvm-type"));
    assert(SCHEME_NULLP(argv[1]) || PAIRP(argv[1]));

    list_len = list_length(argv[1]);
    assert(list_len >= 0);
    param_types = alloca(list_len * sizeof(LLVMTypeRef));

    param  = argv[1];
    for(i=0; i < list_len; i++) {
	assert(PAIRP(param));
	val = PAIR_CAR(param);
	assert(cptr_check(val, "llvm-type"));

	param_types[i] = SCHEME_CPTR_VAL(val);

	param = PAIR_CDR(param);
    }

    ret = LLVMFunctionType(SCHEME_CPTR_VAL(argv[0]),
			   param_types, list_len,
			   SCHEME_TRUEP(vararg));
    return cptr_make(ret, "llvm-type");
}

/*
  Value operations
*/

/* Constant scalar values */

/*
  Return a null value.
  argv[0]: Type of value to construct
*/
static Scheme_Object* const_null(int argc, Scheme_Object **argv)
{
    assert(cptr_check(argv[0], "llvm-type"));

    return cptr_make(LLVMConstNull(SCHEME_CPTR_VAL(argv[0])), "llvm-value");
}

/*
  Return a value filled with binary "1"s.
  argv[0]: Type of value to construct
*/
static Scheme_Object* const_allones(int argc, Scheme_Object **argv)
{
    assert(cptr_check(argv[0], "llvm-type"));

    return cptr_make(LLVMConstAllOnes(SCHEME_CPTR_VAL(argv[0])), "llvm-value");
}

/*
  Build an integer value.
  argv[0]: Type of value to construct
  argv[1]: Fixnum representation of value
  argv[2]: A boolean to sign extend or not (optional)
*/
static Scheme_Object* const_int(int argc, Scheme_Object **argv)
{
    Scheme_Object *sext = scheme_false;
    assert(cptr_check(argv[0], "llvm-type"));
    assert(SCHEME_INTP(argv[1]));

    if(argc == 3) {
	sext = argv[2];
    }

    return cptr_make(LLVMConstInt(SCHEME_CPTR_VAL(argv[0]),
				  SCHEME_INT_VAL(argv[1]),
				  SCHEME_TRUEP(sext)), "llvm-value");
}

/*
  Build an floating point value.
  argv[0]: Type of value to construct
  argv[1]: double representation of value
*/
static Scheme_Object* const_real(int argc, Scheme_Object **argv)
{
    assert(cptr_check(argv[0], "llvm-type"));
    assert(SCHEME_DBLP(argv[1]));

    return cptr_make(LLVMConstReal(SCHEME_CPTR_VAL(argv[0]),
				   SCHEME_DBL_VAL(argv[1])), "llvm-value");
}

/* Constant binary operations */

/*
  Combine two constant operands.
  argv[0]: Value of operand 1
  argv[1]: Value of operand 2
*/
#define VALUE_BIN_OP(NAME, FUNCTION) \
static Scheme_Object* NAME(int argc, Scheme_Object **argv) \
{ \
    assert(value_check(argv[0]));     \
    assert(value_check(argv[1])); \
\
    return cptr_make(FUNCTION(SCHEME_CPTR_VAL(argv[0]), \
			      SCHEME_CPTR_VAL(argv[1])), "llvm-value"); \
}

VALUE_BIN_OP(const_add, LLVMConstAdd)
VALUE_BIN_OP(const_nsw_add, LLVMConstNSWAdd)
VALUE_BIN_OP(const_fadd, LLVMConstFAdd)
VALUE_BIN_OP(const_sub, LLVMConstSub)
VALUE_BIN_OP(const_fsub, LLVMConstFSub)
VALUE_BIN_OP(const_mul, LLVMConstMul)
VALUE_BIN_OP(const_fmul, LLVMConstFMul)
VALUE_BIN_OP(const_udiv, LLVMConstUDiv)
VALUE_BIN_OP(const_sdiv, LLVMConstSDiv)
VALUE_BIN_OP(const_exact_sdiv, LLVMConstExactSDiv)
VALUE_BIN_OP(const_fdiv, LLVMConstFDiv)
VALUE_BIN_OP(const_urem, LLVMConstURem)
VALUE_BIN_OP(const_srem, LLVMConstSRem)
VALUE_BIN_OP(const_frem, LLVMConstFRem)
VALUE_BIN_OP(const_and, LLVMConstAnd)
VALUE_BIN_OP(const_or, LLVMConstOr)
VALUE_BIN_OP(const_xor, LLVMConstXor)
VALUE_BIN_OP(const_shl, LLVMConstShl)
VALUE_BIN_OP(const_lshr, LLVMConstLShr)
VALUE_BIN_OP(const_ashr, LLVMConstAShr)

/* Constant conversion operations */

/*
  Convert a value to the specified type.
  argv[0]: Value to be converted
  argv[1]: Destination type
*/
#define VALUE_CONV_BIN_OP(NAME, FUNCTION) \
static Scheme_Object* NAME(int argc, Scheme_Object **argv) \
{ \
    assert(value_check(argv[0])); \
    assert(cptr_check(argv[1], "llvm-type")); \
\
    return cptr_make(FUNCTION(SCHEME_CPTR_VAL(argv[0]), \
			      SCHEME_CPTR_VAL(argv[1])), "llvm-value"); \
}

VALUE_CONV_BIN_OP(const_trunc, LLVMConstTrunc)
VALUE_CONV_BIN_OP(const_sext, LLVMConstSExt)
VALUE_CONV_BIN_OP(const_zext, LLVMConstZExt)
VALUE_CONV_BIN_OP(const_fptrunc, LLVMConstFPTrunc)
VALUE_CONV_BIN_OP(const_fpext, LLVMConstFPExt)
VALUE_CONV_BIN_OP(const_uitofp, LLVMConstUIToFP)
VALUE_CONV_BIN_OP(const_sitofp, LLVMConstSIToFP)
VALUE_CONV_BIN_OP(const_fptoui, LLVMConstFPToUI)
VALUE_CONV_BIN_OP(const_fptosi, LLVMConstFPToSI)
VALUE_CONV_BIN_OP(const_ptrtoint, LLVMConstPtrToInt)
VALUE_CONV_BIN_OP(const_inttoptr, LLVMConstIntToPtr)
VALUE_CONV_BIN_OP(const_bitcast, LLVMConstBitCast)
VALUE_CONV_BIN_OP(const_zext_or_bitcast, LLVMConstZExtOrBitCast)
VALUE_CONV_BIN_OP(const_sext_or_bitcast, LLVMConstSExtOrBitCast)
VALUE_CONV_BIN_OP(const_trunc_or_bitcast, LLVMConstTruncOrBitCast)
VALUE_CONV_BIN_OP(const_pointercast, LLVMConstPointerCast)
VALUE_CONV_BIN_OP(const_fpcast, LLVMConstFPCast)


/*
  Dump a value to stderr.
  argv[0]: Value to dump
*/
static Scheme_Object* value_dump(int argc, Scheme_Object **argv)
{
    assert(value_check(argv[0]));

    LLVMDumpValue(SCHEME_CPTR_VAL(argv[0]));

    return scheme_void;
}

/*
  Builder operations
*/

/*
  LLVM builder destructor, invoked by mzscheme's garbage collector.
*/
static void builder_destroy(void *p, void *data)
{
    LLVMBuilderRef builder;
    assert(cptr_check(p, "llvm-builder"));

    builder = SCHEME_CPTR_VAL(p);
    assert(builder);

    LLVMDisposeBuilder(builder);
}

/*
  Create a new LLVM builder.
*/
static Scheme_Object* builder_new(int argc, Scheme_Object **argv)
{
    Scheme_Object *ret;
    LLVMBuilderRef builder;

    builder = LLVMCreateBuilder();
    assert(builder);

    ret = cptr_make(builder, "llvm-builder");
    scheme_add_finalizer(ret, builder_destroy, scheme_void);

    return ret;
}

/*
  Position a builder to the end of a basic block
  argv[0]: Builder
  argv[1]: Basic block
*/
static Scheme_Object* builder_pos_at_end(int argc, Scheme_Object **argv)
{
    assert(cptr_check(argv[0], "llvm-builder"));
    assert(cptr_check(argv[1], "llvm-bb"));
    assert(SCHEME_CPTR_VAL(argv[1]));

    LLVMPositionBuilderAtEnd(SCHEME_CPTR_VAL(argv[0]),
			     SCHEME_CPTR_VAL(argv[1]));

    return scheme_void;
}

/*
  Clear the builder's position
  argv[0]: Builder
*/
static Scheme_Object* builder_pos_clear(int argc, Scheme_Object **argv)
{
    assert(cptr_check(argv[0], "llvm-builder"));

    LLVMClearInsertionPosition(SCHEME_CPTR_VAL(argv[0]));

    return scheme_void;
}

/*
  Insert a value with a builder
  argv[0]: Builder
  argv[1]: Value
  argv[2]: Value name (optional)
*/
static Scheme_Object* builder_insert(int argc, Scheme_Object **argv)
{
    Scheme_Object *val_name;

    assert(cptr_check(argv[0], "llvm-builder"));
    assert(value_check(argv[1]));

    if(argc == 2) {
	LLVMInsertIntoBuilder(SCHEME_CPTR_VAL(argv[0]),
			      SCHEME_CPTR_VAL(argv[1]));
    } else if(argc == 3) {
	assert(SCHEME_CHAR_STRINGP(argv[2]));
	val_name = scheme_char_string_to_byte_string(argv[2]);

	LLVMInsertIntoBuilderWithName(SCHEME_CPTR_VAL(argv[0]),
				      SCHEME_CPTR_VAL(argv[1]),
				      SCHEME_BYTE_STR_VAL(val_name));
    }

    return scheme_void;
}

/*
  Build a void return
*/
static Scheme_Object* build_ret_void(int argc, Scheme_Object **argv)
{
    assert(cptr_check(argv[0], "llvm-builder"));

    LLVMBuildRetVoid(SCHEME_CPTR_VAL(argv[0]));

    return scheme_void;
}

/*
  Build a return instruction
*/
static Scheme_Object* build_ret(int argc, Scheme_Object **argv)
{
    assert(cptr_check(argv[0], "llvm-builder"));
    assert(value_check(argv[1]));

    LLVMBuildRet(SCHEME_CPTR_VAL(argv[0]),
		 SCHEME_CPTR_VAL(argv[1]));

    return scheme_void;
}

/*
  Basic block operations
*/

/*
  Get the entry basic block of a function.
  argv[0]: Function
*/
static Scheme_Object* bb_function_entry(int argc, Scheme_Object **argv)
{
    assert(value_check(argv[0]));

    return cptr_make(LLVMGetEntryBasicBlock(SCHEME_CPTR_VAL(argv[0])), "llvm-bb");
}

/*
  Append a basic block to a function.
  argv[0]: Function
  argv[1]: Basic block name
*/
static Scheme_Object* bb_append(int argc, Scheme_Object **argv)
{
    Scheme_Object *func_name;
    assert(value_check(argv[0]));
    assert(SCHEME_CHAR_STRINGP(argv[1]));

    func_name = scheme_char_string_to_byte_string(argv[1]);

    return cptr_make(LLVMAppendBasicBlock(SCHEME_CPTR_VAL(argv[0]),
					  SCHEME_BYTE_STR_VAL(func_name)), "llvm-bb");
}

/*
  Delete a basic block.
  argv[0]: Basic block
*/
static Scheme_Object* bb_delete(int argc, Scheme_Object **argv)
{
    assert(cptr_check(argv[0], "llvm-bb"));
    assert(SCHEME_CPTR_VAL(argv[0]));

    LLVMDeleteBasicBlock(SCHEME_CPTR_VAL(argv[0]));

    SCHEME_CPTR_VAL(argv[0]) = NULL;

    return scheme_void;
}


/*
  Function operations
*/

/*
  Add a function to a module.
  argv[0]: Destination module
  argv[1]: Function name
  argv[2]: Function type
*/
static Scheme_Object* function_add(int argc, Scheme_Object **argv)
{
    Scheme_Object *func_name;
    assert(cptr_check(argv[0], "llvm-module"));
    assert(SCHEME_CHAR_STRINGP(argv[1]));
    assert(cptr_check(argv[2], "llvm-type"));

    func_name = scheme_char_string_to_byte_string(argv[1]);

    return cptr_make(LLVMAddFunction(SCHEME_CPTR_VAL(argv[0]),
				     SCHEME_BYTE_STR_VAL(func_name),
				     SCHEME_CPTR_VAL(argv[2])), "llvm-value");
}

/*
  Delete a function from a module.
  argv[0]: Function value
*/
static Scheme_Object* function_delete(int argc, Scheme_Object **argv)
{
    assert(value_check(argv[0]));

    LLVMDeleteFunction(SCHEME_CPTR_VAL(argv[0]));

    // This operation invalidates the pointer.
    SCHEME_CPTR_VAL(argv[0]) = NULL;

    return scheme_void;
}

/*
  Global variable operations
*/

/*
  Add a global to a module
  argv[0]: Module
  argv[1]: Global type
  argv[2]: Global name
*/
static Scheme_Object* global_add(int argc, Scheme_Object **argv)
{
    Scheme_Object *global_name;
    const char *name = "";

    assert(cptr_check(argv[0], "llvm-module"));
    assert(cptr_check(argv[1], "llvm-type"));

    if(argc == 3) {
	assert(SCHEME_CHAR_STRINGP(argv[2]));
	global_name = scheme_char_string_to_byte_string(argv[2]);
	name = SCHEME_BYTE_STR_VAL(global_name);
    }

    return cptr_make(LLVMAddGlobal(SCHEME_CPTR_VAL(argv[0]),
				   SCHEME_CPTR_VAL(argv[1]),
				   name), "llvm-value");
}

/*
  Delete a global variable from a module
  argv[0]: Global value
*/
static Scheme_Object* global_delete(int argc, Scheme_Object **argv)
{
    assert(value_check(argv[0]));

    LLVMDeleteGlobal(SCHEME_CPTR_VAL(argv[0]));

    // This operation invalidates the pointer.
    SCHEME_CPTR_VAL(argv[0]) = NULL;

    return scheme_void;
}

/*
  Set a global variable's initializer
  argv[0]: Global value
  argv[1]: Constant initializer value
*/
static Scheme_Object* global_set_init(int argc, Scheme_Object **argv)
{
    assert(value_check(argv[0]));
    assert(value_check(argv[1]));

    LLVMSetInitializer(SCHEME_CPTR_VAL(argv[0]), SCHEME_CPTR_VAL(argv[1]));

    return scheme_void;
}

/*
  Get a global variable's initializer
  argv[0]: Global value
*/
static Scheme_Object* global_get_init(int argc, Scheme_Object **argv)
{
    assert(value_check(argv[0]));

    return cptr_make(LLVMGetInitializer(SCHEME_CPTR_VAL(argv[0])),
		     "llvm-value");
}

/*
  Set the constant flag on a global value
  argv[0]: Global value
  argv[1]: Boolean determining if this is a constant
*/
static Scheme_Object* global_set_constant(int argc, Scheme_Object **argv)
{
    assert(value_check(argv[0]));

    LLVMSetGlobalConstant(SCHEME_CPTR_VAL(argv[0]), SCHEME_TRUEP(argv[1]));

    return scheme_void;
}

/*
  Test if a global value is constant
  argv[0]: Global value
*/
static Scheme_Object* global_is_constant(int argc, Scheme_Object **argv)
{
    assert(value_check(argv[0]));

    return LLVMIsGlobalConstant(SCHEME_CPTR_VAL(argv[0])) ? scheme_true : scheme_false;
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
    Scheme_Object *scm_error;
    LLVMMemoryBufferRef buf;
    LLVMModuleRef mod;
    char *error;
    int fail;

    assert(SCHEME_CHAR_STRINGP(argv[0]));
    path = scheme_char_string_to_path(argv[0]);

    // FIXME: we need to consult the current scheme security guard.
    fail = LLVMCreateMemoryBufferWithContentsOfFile(SCHEME_PATH_VAL(path), &buf, &error);
    if(fail) {
	scm_error = scheme_make_utf8_string(error);
	LLVMDisposeMessage(error);

	scheme_signal_error("Could not load LLVM module \"%Q\": %Q.\n", argv[0], scm_error);
    }
    assert(buf);

    fail = LLVMParseBitcode(buf, &mod, &error);
    LLVMDisposeMemoryBuffer(buf);
    if(fail) {
	// Error message is optional
	if(error) {
	    scm_error = scheme_make_utf8_string(error);
	    LLVMDisposeMessage(error);
	} else {
	    scm_error = scheme_make_utf8_string("unknown error");
	}

	scheme_signal_error("Could not parse LLVM module \"%Q\": %Q.\n", argv[0], scm_error);
    }
    assert(mod);

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
    LLVMModuleRef mod;

    assert(SCHEME_CHAR_STRINGP(argv[0]));
    byte_name = scheme_char_string_to_byte_string(argv[0]);

    mod = LLVMModuleCreateWithName(SCHEME_BYTE_STR_VAL(byte_name));
    assert(mod);

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
    // Type operations
    {"llvm-type-void",      type_void,      0, 0},
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
    {"llvm-type-pointer",   type_pointer,   1, 1},
    {"llvm-type-function",  type_function,  2, 3},
    // Value operations
    {"llvm-const-null",        const_null,        1, 1}, // Constant scalar values
    {"llvm-const-allones",     const_allones,     1, 1},
    {"llvm-const-int",         const_int,         2, 3},
    {"llvm-const-real",        const_real,        2, 2},
    {"llvm-const-add",         const_add,         2, 2}, // Constant binary operations
    {"llvm-const-nsw-add",     const_nsw_add,     2, 2},
    {"llvm-const-fadd",        const_fadd,        2, 2},
    {"llvm-const-sub",         const_sub,         2, 2},
    {"llvm-const-fsub",        const_fsub,        2, 2},
    {"llvm-const-mul",         const_mul,         2, 2},
    {"llvm-const-fmul",        const_fmul,        2, 2},
    {"llvm-const-udiv",        const_udiv,        2, 2},
    {"llvm-const-sdiv",        const_sdiv,        2, 2},
    {"llvm-const-exact-sdiv",  const_exact_sdiv,  2, 2},
    {"llvm-const-fdiv",        const_fdiv,        2, 2},
    {"llvm-const-urem",        const_urem,        2, 2},
    {"llvm-const-srem",        const_srem,        2, 2},
    {"llvm-const-frem",        const_frem,        2, 2},
    {"llvm-const-add",         const_and,         2, 2},
    {"llvm-const-or",          const_or,          2, 2},
    {"llvm-const-xor",         const_xor,         2, 2},
    {"llvm-const-shl",         const_shl,         2, 2},
    {"llvm-const-lshr",        const_lshr,        2, 2},
    {"llvm-const-ashr",        const_ashr,        2, 2},
    {"llvm-const-trunc",       const_trunc,       2, 2}, // Constant conversion operations
    {"llvm-const-sext",        const_sext,        2, 2},
    {"llvm-const-zext",        const_zext,        2, 2},
    {"llvm-const-fptrunc",     const_fptrunc,     2, 2},
    {"llvm-const-fpext",       const_fpext,       2, 2},
    {"llvm-const-uitofp",      const_uitofp,      2, 2},
    {"llvm-const-sitofp",      const_sitofp,      2, 2},
    {"llvm-const-fptoui",      const_fptoui,      2, 2},
    {"llvm-const-fptosi",      const_fptosi,      2, 2},
    {"llvm-const-ptrtoint",    const_ptrtoint,    2, 2},
    {"llvm-const-inttoptr",    const_inttoptr,    2, 2},
    {"llvm-const-bitcast",     const_bitcast,     2, 2},
    {"llvm-const-pointercast", const_pointercast, 2, 2},
    {"llvm-const-fpcast",      const_fpcast,      2, 2},
    {"llvm-const-zext-or-bitcast",  const_zext_or_bitcast,  2, 2},
    {"llvm-const-sext-or-bitcast",  const_sext_or_bitcast,  2, 2},
    {"llvm-const-trunc-or-bitcast", const_trunc_or_bitcast, 2, 2},
    {"llvm-value-dump",       value_dump,       1, 1},
    // Builder operations
    {"llvm-builder-new",         builder_new,        0, 0},
    {"llvm-builder-pos-at-end!", builder_pos_at_end, 2, 2},
    {"llvm-builder-pos-clear!",  builder_pos_clear,  1, 1},
    {"llvm-builder-insert!",     builder_insert,     2, 3},
    {"llvm-build-ret-void",      build_ret_void,     1, 1},
    {"llvm-build-ret",           build_ret,          2, 2},
    // Basic block operations
    {"llvm-bb-function-entry", bb_function_entry, 1, 1},
    {"llvm-bb-append!",        bb_append,         2, 2},
    {"llvm-bb-delete!",        bb_delete,         1, 1},
    // Function operations
    {"llvm-function-add!",    function_add,    3, 3},
    {"llvm-function-delete!", function_delete, 1, 1},
    // Global variable operations
    {"llvm-global-add!",          global_add,          2, 3},
    {"llvm-global-delete!",       global_delete,       1, 1},
    {"llvm-global-set-init!",     global_set_init,     2, 2},
    {"llvm-global-get-init",      global_get_init,     1, 1},
    {"llvm-global-set-constant!", global_set_constant, 2, 2},
    {"llvm-global-constant?",     global_is_constant,  1, 1},
    // Module operations
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

    name = scheme_intern_symbol("llvm-basic");
    module = scheme_primitive_module(name, env);

    for(f = functions; f->name != NULL; f = f XFORM_TRUST_PLUS 1) {
	register_function(module, f->name, f->code, f->arg_min, f->arg_max);
    }

    tmp = scheme_make_utf8_string(license);
    scheme_add_global("llvm-wrapper-license", tmp, module);
    tmp = scheme_make_utf8_string(disclaimer);
    scheme_add_global("llvm-wrapper-disclaimer", tmp, module);
    tmp = scheme_make_utf8_string(nowarranty);
    scheme_add_global("llvm-wrapper-nowarranty", tmp, module);

    scheme_finish_primitive_module(module);

    return scheme_void;
}

Scheme_Object* scheme_reload(Scheme_Env* env)
{
    return scheme_initialize(env);
}

Scheme_Object* scheme_module_name(void)
{
    return scheme_intern_symbol("llvm-basic");
}
