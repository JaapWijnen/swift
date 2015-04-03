//===--- ArgumentSource.cpp - Latent value representation -------*- C++ -*-===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2015 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See http://swift.org/LICENSE.txt for license information
// See http://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
//
// A structure for holding a r-value or l-value
//
//===----------------------------------------------------------------------===//

#include "ArgumentSource.h"
#include "Initialization.h"

using namespace swift;
using namespace Lowering;

RValue &ArgumentSource::forceAndPeekRValue(SILGenFunction &gen) & {
  if (isRValue()) {
    return Storage.TheRV.Value;
  }

  auto expr = asKnownExpr();
  StoredKind = Kind::RValue;
  new (&Storage.TheRV.Value) RValue(gen.emitRValue(expr));
  Storage.TheRV.Loc = expr;
  return Storage.TheRV.Value;
}

void ArgumentSource::rewriteType(CanType newType) & {
  assert(!isLValue());
  if (isRValue()) {
    Storage.TheRV.Value.rewriteType(newType);
  } else {
    Expr *expr = Storage.TheExpr;
    if (expr->getType()->isEqual(newType)) return;
    llvm_unreachable("unimplemented! hope it doesn't happen");
  }
}

RValue ArgumentSource::getAsRValue(SILGenFunction &gen, SGFContext C) && {
  assert(!isLValue());
  if (isRValue())
    return std::move(*this).asKnownRValue();

  return gen.emitRValue(std::move(*this).asKnownExpr(), C);
}

ManagedValue ArgumentSource::getAsSingleValue(SILGenFunction &gen,
                                              SGFContext C) && {
  if (isRValue()) {
    auto loc = getKnownRValueLocation();
    return std::move(*this).asKnownRValue().getAsSingleValue(gen, loc);
  }
  if (isLValue()) {
    auto loc = getKnownLValueLocation();
    return gen.emitAddressOfLValue(loc, std::move(*this).asKnownLValue(),
                                   AccessKind::ReadWrite);
  }

  auto e = std::move(*this).asKnownExpr();
  if (e->getType()->is<InOutType>()) {
    return gen.emitAddressOfLValue(e, gen.emitLValue(e, AccessKind::ReadWrite),
                                   AccessKind::ReadWrite);
  } else {
    return gen.emitRValueAsSingleValue(e, C);
  }
}

void ArgumentSource::forwardInto(SILGenFunction &gen, Initialization *dest) && {
  assert(!isLValue());
  if (isRValue()) {
    auto loc = getKnownRValueLocation();
    return std::move(*this).asKnownRValue().forwardInto(gen, dest, loc);
  }

  auto e = std::move(*this).asKnownExpr();
  return gen.emitExprInto(e, dest);
}

ManagedValue ArgumentSource::materialize(SILGenFunction &gen) && {
  assert(!isLValue());
  if (isRValue()) {
    auto loc = getKnownRValueLocation();
    return std::move(*this).asKnownRValue().materialize(gen, loc);
  }

  auto expr = std::move(*this).asKnownExpr();
  auto temp = gen.emitTemporary(expr, gen.getTypeLowering(expr->getType()));
  gen.emitExprInto(expr, temp.get());
  return temp->getManagedAddress();
}

ManagedValue ArgumentSource::materialize(SILGenFunction &SGF,
                                         AbstractionPattern origFormalType,
                                         SILType destType) && {
  auto substFormalType = CanType(getSubstType()->getInOutObjectType());
  assert(!destType || destType.getObjectType() ==
               SGF.SGM.Types.getLoweredType(origFormalType,
                                            substFormalType).getObjectType());

  // Fast path: if the types match exactly, no abstraction difference
  // is possible and we can just materialize as normal.
  if (origFormalType.isExactType(substFormalType))
    return std::move(*this).materialize(SGF);

  auto &destTL =
    (destType ? SGF.getTypeLowering(destType)
              : SGF.getTypeLowering(origFormalType, substFormalType));
  if (!destType) destType = destTL.getLoweredType();

  // If there's no abstraction difference, we can just materialize as normal.
  if (destTL.getLoweredType() == SGF.getLoweredType(substFormalType)) {
    return std::move(*this).materialize(SGF);
  }

  // Emit a temporary at the given address.
  auto temp = SGF.emitTemporary(getLocation(), destTL);

  // Forward into it.
  std::move(*this).forwardInto(SGF, origFormalType, temp.get(), destTL);

  return temp->getManagedAddress();
}

void ArgumentSource::forwardInto(SILGenFunction &SGF,
                                 AbstractionPattern origFormalType,
                                 Initialization *dest,
                                 const TypeLowering &destTL) && {
  auto substFormalType = getSubstType();
  assert(destTL.getLoweredType() ==
                        SGF.getLoweredType(origFormalType, substFormalType));

  // If there are no abstraction changes, we can just forward
  // normally.
  if (origFormalType.isExactType(substFormalType) ||
      destTL.getLoweredType() == SGF.getLoweredType(substFormalType)) {
    std::move(*this).forwardInto(SGF, dest);
    return;
  }

  // Otherwise, emit as a single independent value.
  SILLocation loc = getLocation();
  ManagedValue inputValue = std::move(*this).getAsSingleValue(SGF);

  // Reabstract.
  ManagedValue outputValue =
    SGF.emitSubstToOrigValue(loc, inputValue,
                             origFormalType, substFormalType,
                             SGFContext(dest));
  if (outputValue.isInContext()) return;

  // Use RValue's forward-into-initialization code.  We have to lie to
  // RValue about the formal type (by using the lowered type) because
  // we're emitting into an abstracted value, which RValue doesn't
  // really handle.
  auto substLoweredType = destTL.getLoweredType().getSwiftRValueType();
  RValue(SGF, loc, substLoweredType, outputValue).forwardInto(SGF, dest, loc);
}
