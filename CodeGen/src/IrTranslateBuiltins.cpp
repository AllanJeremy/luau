// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "IrTranslateBuiltins.h"

#include "Luau/Bytecode.h"
#include "Luau/IrBuilder.h"

#include "lstate.h"

// TODO: should be possible to handle fastcalls in contexts where nresults is -1 by adding the adjustment instruction
// TODO: when nresults is less than our actual result count, we can skip computing/writing unused results

namespace Luau
{
namespace CodeGen
{

// Wrapper code for all builtins with a fixed signature and manual assembly lowering of the body

// (number, ...) -> number
BuiltinImplResult translateBuiltinNumberToNumber(
    IrBuilder& build, LuauBuiltinFunction bfid, int nparams, int ra, int arg, IrOp args, int nresults, IrOp fallback)
{
    if (nparams < 1 || nresults > 1)
        return {BuiltinImplType::None, -1};

    build.loadAndCheckTag(build.vmReg(arg), LUA_TNUMBER, fallback);
    build.inst(IrCmd::FASTCALL, build.constUint(bfid), build.vmReg(ra), build.vmReg(arg), args, build.constInt(nparams), build.constInt(nresults));

    // TODO: tag update might not be required, we place it here now because FASTCALL is not modeled in constant propagation yet
    build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TNUMBER));

    return {BuiltinImplType::UsesFallback, 1};
}

// (number, number, ...) -> number
BuiltinImplResult translateBuiltin2NumberToNumber(
    IrBuilder& build, LuauBuiltinFunction bfid, int nparams, int ra, int arg, IrOp args, int nresults, IrOp fallback)
{
    if (nparams < 2 || nresults > 1)
        return {BuiltinImplType::None, -1};

    build.loadAndCheckTag(build.vmReg(arg), LUA_TNUMBER, fallback);
    build.loadAndCheckTag(args, LUA_TNUMBER, fallback);
    build.inst(IrCmd::FASTCALL, build.constUint(bfid), build.vmReg(ra), build.vmReg(arg), args, build.constInt(nparams), build.constInt(nresults));

    // TODO:tag update might not be required, we place it here now because FASTCALL is not modeled in constant propagation yet
    build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TNUMBER));

    return {BuiltinImplType::UsesFallback, 1};
}

// (number, ...) -> (number, number)
BuiltinImplResult translateBuiltinNumberTo2Number(
    IrBuilder& build, LuauBuiltinFunction bfid, int nparams, int ra, int arg, IrOp args, int nresults, IrOp fallback)
{
    if (nparams < 1 || nresults > 2)
        return {BuiltinImplType::None, -1};

    build.loadAndCheckTag(build.vmReg(arg), LUA_TNUMBER, fallback);
    build.inst(IrCmd::FASTCALL, build.constUint(bfid), build.vmReg(ra), build.vmReg(arg), args, build.constInt(nparams), build.constInt(nresults));

    // TODO: some tag updates might not be required, we place them here now because FASTCALL is not modeled in constant propagation yet
    build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TNUMBER));
    build.inst(IrCmd::STORE_TAG, build.vmReg(ra + 1), build.constTag(LUA_TNUMBER));

    return {BuiltinImplType::UsesFallback, 2};
}

BuiltinImplResult translateBuiltinAssert(IrBuilder& build, int nparams, int ra, int arg, IrOp args, int nresults, IrOp fallback)
{
    if (nparams < 1 || nresults != 0)
        return {BuiltinImplType::None, -1};

    IrOp cont = build.block(IrBlockKind::Internal);

    // TODO: maybe adding a guard like CHECK_TRUTHY can be useful
    build.inst(IrCmd::JUMP_IF_FALSY, build.vmReg(arg), fallback, cont);
    build.beginBlock(cont);

    return {BuiltinImplType::UsesFallback, 0};
}

BuiltinImplResult translateBuiltinMathDeg(IrBuilder& build, int nparams, int ra, int arg, IrOp args, int nresults, IrOp fallback)
{
    if (nparams < 1 || nresults > 1)
        return {BuiltinImplType::None, -1};

    build.loadAndCheckTag(build.vmReg(arg), LUA_TNUMBER, fallback);

    const double rpd = (3.14159265358979323846 / 180.0);

    IrOp varg = build.inst(IrCmd::LOAD_DOUBLE, build.vmReg(arg));
    IrOp value = build.inst(IrCmd::DIV_NUM, varg, build.constDouble(rpd));
    build.inst(IrCmd::STORE_DOUBLE, build.vmReg(ra), value);

    if (ra != arg)
        build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TNUMBER));

    return {BuiltinImplType::UsesFallback, 1};
}

BuiltinImplResult translateBuiltinMathRad(IrBuilder& build, int nparams, int ra, int arg, IrOp args, int nresults, IrOp fallback)
{
    if (nparams < 1 || nresults > 1)
        return {BuiltinImplType::None, -1};

    build.loadAndCheckTag(build.vmReg(arg), LUA_TNUMBER, fallback);

    const double rpd = (3.14159265358979323846 / 180.0);

    IrOp varg = build.inst(IrCmd::LOAD_DOUBLE, build.vmReg(arg));
    IrOp value = build.inst(IrCmd::MUL_NUM, varg, build.constDouble(rpd));
    build.inst(IrCmd::STORE_DOUBLE, build.vmReg(ra), value);

    if (ra != arg)
        build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TNUMBER));

    return {BuiltinImplType::UsesFallback, 1};
}

BuiltinImplResult translateBuiltinMathLog(
    IrBuilder& build, LuauBuiltinFunction bfid, int nparams, int ra, int arg, IrOp args, int nresults, IrOp fallback)
{
    if (nparams < 1 || nresults > 1)
        return {BuiltinImplType::None, -1};

    build.loadAndCheckTag(build.vmReg(arg), LUA_TNUMBER, fallback);

    if (nparams != 1)
        build.loadAndCheckTag(args, LUA_TNUMBER, fallback);

    build.inst(IrCmd::FASTCALL, build.constUint(bfid), build.vmReg(ra), build.vmReg(arg), args, build.constInt(nparams), build.constInt(nresults));

    // TODO: tag update might not be required, we place it here now because FASTCALL is not modeled in constant propagation yet
    build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TNUMBER));

    return {BuiltinImplType::UsesFallback, 1};
}

BuiltinImplResult translateBuiltinMathMin(IrBuilder& build, int nparams, int ra, int arg, IrOp args, int nresults, IrOp fallback)
{
    // TODO: this can be extended for other number of arguments
    if (nparams != 2 || nresults > 1)
        return {BuiltinImplType::None, -1};

    build.loadAndCheckTag(build.vmReg(arg), LUA_TNUMBER, fallback);
    build.loadAndCheckTag(args, LUA_TNUMBER, fallback);

    IrOp varg1 = build.inst(IrCmd::LOAD_DOUBLE, build.vmReg(arg));
    IrOp varg2 = build.inst(IrCmd::LOAD_DOUBLE, args);

    IrOp res = build.inst(IrCmd::MIN_NUM, varg2, varg1); // Swapped arguments are required for consistency with VM builtins
    build.inst(IrCmd::STORE_DOUBLE, build.vmReg(ra), res);

    if (ra != arg)
        build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TNUMBER));

    return {BuiltinImplType::UsesFallback, 1};
}

BuiltinImplResult translateBuiltinMathMax(IrBuilder& build, int nparams, int ra, int arg, IrOp args, int nresults, IrOp fallback)
{
    // TODO: this can be extended for other number of arguments
    if (nparams != 2 || nresults > 1)
        return {BuiltinImplType::None, -1};

    build.loadAndCheckTag(build.vmReg(arg), LUA_TNUMBER, fallback);
    build.loadAndCheckTag(args, LUA_TNUMBER, fallback);

    IrOp varg1 = build.inst(IrCmd::LOAD_DOUBLE, build.vmReg(arg));
    IrOp varg2 = build.inst(IrCmd::LOAD_DOUBLE, args);

    IrOp res = build.inst(IrCmd::MAX_NUM, varg2, varg1); // Swapped arguments are required for consistency with VM builtins
    build.inst(IrCmd::STORE_DOUBLE, build.vmReg(ra), res);

    if (ra != arg)
        build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TNUMBER));

    return {BuiltinImplType::UsesFallback, 1};
}

BuiltinImplResult translateBuiltinMathClamp(IrBuilder& build, int nparams, int ra, int arg, IrOp args, int nresults, IrOp fallback)
{
    if (nparams < 3 || nresults > 1)
        return {BuiltinImplType::None, -1};

    IrOp block = build.block(IrBlockKind::Internal);

    LUAU_ASSERT(args.kind == IrOpKind::VmReg);

    build.loadAndCheckTag(build.vmReg(arg), LUA_TNUMBER, fallback);
    build.loadAndCheckTag(args, LUA_TNUMBER, fallback);
    build.loadAndCheckTag(build.vmReg(args.index + 1), LUA_TNUMBER, fallback);

    IrOp min = build.inst(IrCmd::LOAD_DOUBLE, args);
    IrOp max = build.inst(IrCmd::LOAD_DOUBLE, build.vmReg(args.index + 1));

    build.inst(IrCmd::JUMP_CMP_NUM, min, max, build.cond(IrCondition::NotLessEqual), fallback, block);
    build.beginBlock(block);

    IrOp v = build.inst(IrCmd::LOAD_DOUBLE, build.vmReg(arg));
    IrOp r = build.inst(IrCmd::MAX_NUM, min, v);
    IrOp clamped = build.inst(IrCmd::MIN_NUM, max, r);

    build.inst(IrCmd::STORE_DOUBLE, build.vmReg(ra), clamped);

    if (ra != arg)
        build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TNUMBER));

    return {BuiltinImplType::UsesFallback, 1};
}

BuiltinImplResult translateBuiltinType(IrBuilder& build, int nparams, int ra, int arg, IrOp args, int nresults, IrOp fallback)
{
    if (nparams < 1 || nresults > 1)
        return {BuiltinImplType::None, -1};

    build.inst(
        IrCmd::FASTCALL, build.constUint(LBF_TYPE), build.vmReg(ra), build.vmReg(arg), args, build.constInt(nparams), build.constInt(nresults));

    // TODO: tag update might not be required, we place it here now because FASTCALL is not modeled in constant propagation yet
    build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TSTRING));

    return {BuiltinImplType::UsesFallback, 1};
}

BuiltinImplResult translateBuiltinTypeof(IrBuilder& build, int nparams, int ra, int arg, IrOp args, int nresults, IrOp fallback)
{
    if (nparams < 1 || nresults > 1)
        return {BuiltinImplType::None, -1};

    build.inst(
        IrCmd::FASTCALL, build.constUint(LBF_TYPEOF), build.vmReg(ra), build.vmReg(arg), args, build.constInt(nparams), build.constInt(nresults));

    // TODO: tag update might not be required, we place it here now because FASTCALL is not modeled in constant propagation yet
    build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TSTRING));

    return {BuiltinImplType::UsesFallback, 1};
}

BuiltinImplResult translateBuiltin(IrBuilder& build, int bfid, int ra, int arg, IrOp args, int nparams, int nresults, IrOp fallback)
{
    switch (bfid)
    {
    case LBF_ASSERT:
        return translateBuiltinAssert(build, nparams, ra, arg, args, nresults, fallback);
    case LBF_MATH_DEG:
        return translateBuiltinMathDeg(build, nparams, ra, arg, args, nresults, fallback);
    case LBF_MATH_RAD:
        return translateBuiltinMathRad(build, nparams, ra, arg, args, nresults, fallback);
    case LBF_MATH_LOG:
        return translateBuiltinMathLog(build, LuauBuiltinFunction(bfid), nparams, ra, arg, args, nresults, fallback);
    case LBF_MATH_MIN:
        return translateBuiltinMathMin(build, nparams, ra, arg, args, nresults, fallback);
    case LBF_MATH_MAX:
        return translateBuiltinMathMax(build, nparams, ra, arg, args, nresults, fallback);
    case LBF_MATH_CLAMP:
        return translateBuiltinMathClamp(build, nparams, ra, arg, args, nresults, fallback);
    case LBF_MATH_FLOOR:
    case LBF_MATH_CEIL:
    case LBF_MATH_SQRT:
    case LBF_MATH_ABS:
    case LBF_MATH_EXP:
    case LBF_MATH_ASIN:
    case LBF_MATH_SIN:
    case LBF_MATH_SINH:
    case LBF_MATH_ACOS:
    case LBF_MATH_COS:
    case LBF_MATH_COSH:
    case LBF_MATH_ATAN:
    case LBF_MATH_TAN:
    case LBF_MATH_TANH:
    case LBF_MATH_LOG10:
    case LBF_MATH_ROUND:
    case LBF_MATH_SIGN:
        return translateBuiltinNumberToNumber(build, LuauBuiltinFunction(bfid), nparams, ra, arg, args, nresults, fallback);
    case LBF_MATH_FMOD:
    case LBF_MATH_POW:
    case LBF_MATH_ATAN2:
    case LBF_MATH_LDEXP:
        return translateBuiltin2NumberToNumber(build, LuauBuiltinFunction(bfid), nparams, ra, arg, args, nresults, fallback);
    case LBF_MATH_FREXP:
    case LBF_MATH_MODF:
        return translateBuiltinNumberTo2Number(build, LuauBuiltinFunction(bfid), nparams, ra, arg, args, nresults, fallback);
    case LBF_TYPE:
        return translateBuiltinType(build, nparams, ra, arg, args, nresults, fallback);
    case LBF_TYPEOF:
        return translateBuiltinTypeof(build, nparams, ra, arg, args, nresults, fallback);
    default:
        return {BuiltinImplType::None, -1};
    }
}

} // namespace CodeGen
} // namespace Luau
