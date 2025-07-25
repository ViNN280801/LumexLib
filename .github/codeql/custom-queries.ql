/**
 * @name Custom Security Queries for LumexLib
 * @description Additional security checks for C++ library code
 * @kind problem
 * @id cpp/lumex-security-checks
 * @problem.severity warning
 */

import cpp
import semmle.code.cpp.dataflow.DataFlow
import semmle.code.cpp.security.BufferWrite

/**
 * Query 1: Detect potential buffer overflows in string operations
 */
from FunctionCall call, Function target
where 
    call.getTarget() = target and
    target.getName().regexpMatch("(strcpy|strcat|sprintf|gets)") and
    not exists(File file | 
        file = call.getFile() and 
        file.getRelativePath().regexpMatch(".*/test.*") 
    )
select call, "Potentially unsafe string function: " + target.getName()

/**
 * Query 2: Check for uninitialized pointers in constructors
 */
from Constructor c, Field f, MemberInitializer init
where 
    f.getDeclaringType() = c.getDeclaringType() and
    f.getType() instanceof PointerType and
    not exists(MemberInitializer mi | 
        mi.getTarget() = f and 
        mi.getEnclosingFunction() = c
    ) and
    not exists(File file | 
        file = c.getFile() and 
        file.getRelativePath().regexpMatch(".*/test.*") 
    )
select c, "Constructor does not initialize pointer field: " + f.getName()

/**
 * Query 3: Detect potential memory leaks (malloc without free)
 */
from FunctionCall malloc, Variable v
where 
    malloc.getTarget().getName() = "malloc" and
    exists(AssignExpr assign | 
        assign.getRValue() = malloc and
        assign.getLValue() = v.getAnAccess()
    ) and
    not exists(FunctionCall free | 
        free.getTarget().getName() = "free" and
        free.getArgument(0) = v.getAnAccess()
    ) and
    not exists(File file | 
        file = malloc.getFile() and 
        file.getRelativePath().regexpMatch(".*/test.*") 
    )
select malloc, "Potential memory leak: malloc without corresponding free"

/**
 * Query 4: Check for unsafe cast operations
 */
from Cast cast
where 
    cast.getExpr().getType().getSize() > cast.getType().getSize() and
    not exists(File file | 
        file = cast.getFile() and 
        file.getRelativePath().regexpMatch(".*/test.*") 
    )
select cast, "Unsafe cast from larger to smaller type"

/**
 * Query 5: Detect potential integer overflow in arithmetic operations
 */
from BinaryArithmeticOperation op, Type t
where 
    op.getOperator() = "+" and
    t = op.getType() and
    t instanceof IntegralType and
    not exists(File file | 
        file = op.getFile() and 
        file.getRelativePath().regexpMatch(".*/test.*") 
    )
select op, "Potential integer overflow in arithmetic operation"

/**
 * Query 6: Check for format string vulnerabilities
 */
from FunctionCall call, Expr formatArg
where 
    call.getTarget().getName().regexpMatch("(printf|fprintf|sprintf|snprintf)") and
    formatArg = call.getArgument(0) and
    not formatArg instanceof StringLiteral and
    not exists(File file | 
        file = call.getFile() and 
        file.getRelativePath().regexpMatch(".*/test.*") 
    )
select call, "Potential format string vulnerability"

/**
 * Query 7: Detect use of deprecated or dangerous functions
 */
from FunctionCall call, Function target
where 
    call.getTarget() = target and
    target.getName().regexpMatch("(gets|strcpy|strcat|sprintf|vsprintf)") and
    not exists(File file | 
        file = call.getFile() and 
        file.getRelativePath().regexpMatch(".*/test.*") 
    )
select call, "Use of dangerous function: " + target.getName() + " - consider safer alternatives"

/**
 * Query 8: Check for missing bounds checking in array access
 */
from ArrayExpr arrayAccess, Variable index
where 
    arrayAccess.getArrayOffset() = index.getAnAccess() and
    not exists(RelationalOperation rel | 
        rel.getAnOperand() = index.getAnAccess()
    ) and
    not exists(File file | 
        file = arrayAccess.getFile() and 
        file.getRelativePath().regexpMatch(".*/test.*") 
    )
select arrayAccess, "Array access without bounds checking" 