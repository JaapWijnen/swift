//===--- GenStruct.h - Swift IR generation for classes ------------*- C++ -*-===//
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
//  This file provides the private interface to the class-emission code.
//
//===----------------------------------------------------------------------===//

#ifndef SWIFT_IRGEN_GENCLASS_H
#define SWIFT_IRGEN_GENCLASS_H

namespace llvm {
  class Constant;
  class Value;
  class Function;
}

namespace swift {
  class CanType;
  class ClassDecl;
  class ExtensionDecl;
  class ProtocolDecl;
  class SILType;
  class Type;
  class VarDecl;

namespace irgen {
  class HeapLayout;
  class IRGenFunction;
  class IRGenModule;
  class OwnedAddress;
  
  enum class ReferenceCounting : unsigned char;
  enum class IsaEncoding : unsigned char;
  
  OwnedAddress projectPhysicalClassMemberAddress(IRGenFunction &IGF,
                                                 llvm::Value *base,
                                                 SILType baseType,
                                                 VarDecl *field);

  llvm::Constant *emitClassPrivateData(IRGenModule &IGM, ClassDecl *theClass);
  llvm::Constant *emitCategoryData(IRGenModule &IGM, ExtensionDecl *ext);
  llvm::Constant *emitObjCProtocolData(IRGenModule &IGM, ProtocolDecl *ext);

  /// Emit an allocation of a class.
  llvm::Value *emitClassAllocation(IRGenFunction &IGF, SILType selfType,
                                   bool objc);

  /// Emit an allocation of a class using a metadata value.
  llvm::Value *emitClassAllocationDynamic(IRGenFunction &IGF, 
                                          llvm::Value *metadata,
                                          SILType selfType,
                                          bool objc);

  /// Emit class deallocation.
  void emitClassDeallocation(IRGenFunction &IGF, SILType selfType,
                             llvm::Value *selfValue);

  /// Emit the constant fragile instance size of the class, or null if the class
  /// does not have fixed layout. For resilient classes this does not
  /// correspond to the runtime alignment of instances of the class.
  llvm::Constant *tryEmitClassConstantFragileInstanceSize(IRGenModule &IGM,
                                                   ClassDecl *Class);
  /// Emit the constant fragile instance alignment mask of the class, or null if
  /// the class does not have fixed layout. For resilient classes this does not
  /// correspond to the runtime alignment of instances of the class.
  llvm::Constant *tryEmitClassConstantFragileInstanceAlignMask(IRGenModule &IGM,
                                                        ClassDecl *Class);

  /// Emit the constant fragile byte offset for the field in the class, or null
  /// if the field does not have fixed layout. For resilient classes this does
  /// not correspond to the runtime offset of the field.
  llvm::Constant *tryEmitClassConstantFragileFieldOffset(IRGenModule &IGM,
                                                         ClassDecl *theClass,
                                                         VarDecl *field);
  
  /// True if the value is of class type, or of a type that is bridged to class
  /// type in the ObjC world.
  bool hasObjCClassRepresentation(IRGenModule &IGM, Type t);
  
  /// What reference counting mechanism does a class use?
  ReferenceCounting getReferenceCountingForClass(IRGenModule &IGM,
                                                 ClassDecl *theClass);
  
  /// What isa-encoding mechanism does a type use?
  IsaEncoding getIsaEncodingForType(IRGenModule &IGM, CanType type);
} // end namespace irgen
} // end namespace swift

#endif
