// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma  once

#include "BlueprintCompilerCppBackendBase.h"

struct FEmitterLocalContext;

/** Default implementation of BlueprintCompilerCppBackend. Code generated for function body usually contains a big "switch". */
class FBlueprintCompilerCppBackend : public FBlueprintCompilerCppBackendBase
{
protected:
	bool bUseExecutionGroup;
	bool bUseFlowStack;
	bool bUseGotoState;
public:
	FBlueprintCompilerCppBackend()
		: FBlueprintCompilerCppBackendBase()
		, bUseExecutionGroup(false)
		, bUseFlowStack(false)
		, bUseGotoState(false)
	{}

protected:
	virtual void InnerFunctionImplementation(FKismetFunctionContext& FunctionContext, FEmitterLocalContext& EmitterContext, int32 ExecutionGroup) override;

	// creates local linear execution list ,returns if the execution group can be handled without switch
	bool SortNodesInUberGraphExecutionGroup(FKismetFunctionContext &FunctionContext, UEdGraphNode* TheOnlyEntryPoint, int32 ExecutionGroup, TArray<UEdGraphNode*> &LocalLinearExecutionList);
	// return if the execution group can be handled without switch
	bool PrepareToUseExecutionGroupWithoutGoto(FKismetFunctionContext &FunctionContext, int32 ExecutionGroup, UEdGraphNode* &TheOnlyEntryPoint);
	void EmitAllStatements(FKismetFunctionContext &FunctionContext, int32 ExecutionGroup, FEmitterLocalContext &EmitterContext, const TArray<UEdGraphNode*>& LinearExecutionList);
	void EmitStatement(FBlueprintCompiledStatement &Statement, FEmitterLocalContext &EmitterContext, FKismetFunctionContext& FunctionContext);

protected:
	void EmitCallStatment(FEmitterLocalContext& EmitterContext, FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement);
	void EmitCallDelegateStatment(FEmitterLocalContext& EmitterContext, FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement);
	void EmitAssignmentStatment(FEmitterLocalContext& EmitterContext, FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement);
	void EmitCastObjToInterfaceStatement(FEmitterLocalContext& EmitterContext, FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement);
	void EmitCastBetweenInterfacesStatement(FEmitterLocalContext& EmitterContext, FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement);
	void EmitCastInterfaceToObjStatement(FEmitterLocalContext& EmitterContext, FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement);
	void EmitDynamicCastStatement(FEmitterLocalContext& EmitterContext, FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement);
	void EmitObjectToBoolStatement(FEmitterLocalContext& EmitterContext, FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement);
	void EmitAddMulticastDelegateStatement(FEmitterLocalContext& EmitterContext, FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement);
	void EmitBindDelegateStatement(FEmitterLocalContext& EmitterContext, FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement);
	void EmitClearMulticastDelegateStatement(FEmitterLocalContext& EmitterContext, FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement);
	void EmitCreateArrayStatement(FEmitterLocalContext& EmitterContext, FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement);
	void EmitGotoStatement(FEmitterLocalContext& EmitterContext, FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement);
	void EmitPushStateStatement(FEmitterLocalContext& EmitterContext, FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement);
	void EmitEndOfThreadStatement(FEmitterLocalContext& EmitterContext, FKismetFunctionContext& FunctionContext);
	void EmitRemoveMulticastDelegateStatement(FEmitterLocalContext& EmitterContext, FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement);
	void EmitMetaCastStatement(FEmitterLocalContext& EmitterContext, FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement);

protected:
	FString LatentFunctionInfoTermToText(FEmitterLocalContext& EmitterContext, FBPTerminal* Term, FBlueprintCompiledStatement* TargetLabel);
	FString EmitMethodInputParameterList(FEmitterLocalContext& EmitterContext, FBlueprintCompiledStatement& Statement);
	FString EmitSwitchValueStatmentInner(FEmitterLocalContext& EmitterContext, FBlueprintCompiledStatement& Statement);
	FString EmitCallStatmentInner(FEmitterLocalContext& EmitterContext, FBlueprintCompiledStatement& Statement, bool bInline, FString PostFix);

public:
	FString TermToText(FEmitterLocalContext& EmitterContext, const FBPTerminal* Term, bool bUseSafeContext = true, bool bGetter = true);
};