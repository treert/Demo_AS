#include "TypeDeclarationExporter.h"

#include "AngelscriptManager.h"
#include "AngelscriptDocs.h"
#include "AngelscriptSettings.h"
#include "AngelscriptType.h"

#include "Misc/ScopedSlowTask.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/EngineVersion.h"
#include "Misc/App.h"
#include "HAL/PlatformFileManager.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

#include "StartAngelscriptHeaders.h"
#include "AngelscriptInclude.h"
#include "as_scriptfunction.h"
#include "as_objecttype.h"
#include "as_module.h"
#include "as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_EDITOR
static const FName NAME_AS_ScriptKeywords(TEXT("ScriptKeywords"));
static const TArray<FName> NAMES_AS_InformedMeta = {
	TEXT("DelegateBindType"),
	TEXT("DelegateFunctionParam"),
	TEXT("DelegateObjectParam"),
	TEXT("DelegateWildcardParam"),
};
#endif

// Bump when the .d.as format changes in a way the LSP must understand.
#define AS_DECL_CACHE_FORMAT_VERSION 1

namespace
{
	/** Mirrors FAngelscriptDebugServer::SendDebugDatabase's GetDecl: type decl + ref-suffixes. */
	FString MakeDecl(asCScriptEngine* ScriptEngine, int TypeId, asDWORD* Flags = nullptr)
	{
		if (TypeId == -1)
			return TEXT("?");

		const char* DeclRaw = ScriptEngine->GetTypeDeclaration(TypeId);
		FString Decl = ANSI_TO_TCHAR(DeclRaw);
		if (Flags != nullptr)
		{
			if ((*Flags & asTM_CONST) == 0)
			{
				if ((*Flags & asTM_INOUTREF) == asTM_INOUTREF)
					Decl += TEXT("&");
				else if ((*Flags & asTM_INOUTREF) == asTM_OUTREF)
					Decl += TEXT("&out");
				else if ((*Flags & asTM_INOUTREF) == asTM_INREF)
					Decl += TEXT("&in");
			}
		}
		return Decl;
	}

	FString MakeParameterName(const char* RawName, int32 Index)
	{
		if (RawName == nullptr || RawName[0] == '\0')
			return FString::Printf(TEXT("InArg%d"), Index);
		return ANSI_TO_TCHAR(RawName);
	}

	void AppendDocComment(TArray<FString>& Out, const FString& Doc, const FString& Indent)
	{
		if (Doc.Len() == 0)
			return;

		TArray<FString> DocLines;
		Doc.ParseIntoArrayLines(DocLines);
		for (const FString& Line : DocLines)
			Out.Add(Indent + TEXT("// ") + Line);
	}

	FString MakeGroupFileName(const FString& Group)
	{
		// Package names look like "/Script/Engine" - keep the last path component.
		FString Short = Group;
		int32 SlashIdx;
		if (Short.FindLastChar('/', SlashIdx))
			Short = Short.RightChop(SlashIdx + 1);
		// Namespace names may contain "::" - flatten to dots for the file name.
		return Short.Replace(TEXT("::"), TEXT(".")) + TEXT(".d.as");
	}

	/** Resolves which output group (engine module / namespace) a UClass-backed type belongs to. */
	FString ResolveClassGroup(asITypeInfo* ScriptType, const FString& TypeName)
	{
#if WITH_EDITOR
		UClass* ClassPtr = nullptr;

		auto ASType = FAngelscriptType::GetByAngelscriptTypeName(TypeName);
		if (ASType.IsValid())
			ClassPtr = ASType->GetClass(FAngelscriptTypeUsage::DefaultUsage);

		if (ClassPtr == nullptr && (ScriptType->GetFlags() & asOBJ_REF) != 0)
			ClassPtr = (UClass*)ScriptType->GetUserData();

		if (UStruct* Struct = Cast<UStruct>(ClassPtr))
		{
			if (UPackage* Package = Struct->GetPackage())
				return Package->GetName();
		}
#endif
		return TEXT("Core");
	}

	/**
	 * Namespaces group by their own name (e.g. Math.d.as), but component accessor
	 * namespaces (named after their class, containing Get/GetOrCreate/Create) merge
	 * into the module file of the class they belong to, instead of producing one
	 * tiny file per component class.
	 */
	FString ResolveNamespaceGroup(const FString& NsName)
	{
#if WITH_EDITOR
		if (NsName.Len() != 0)
		{
			auto ASType = FAngelscriptType::GetByAngelscriptTypeName(NsName);
			if (ASType.IsValid())
			{
				UClass* ClassPtr = ASType->GetClass(FAngelscriptTypeUsage::DefaultUsage);
				if (UStruct* Struct = Cast<UStruct>(ClassPtr))
				{
					if (UPackage* Package = Struct->GetPackage())
						return Package->GetName();
				}
			}
		}
#endif
		return NsName.Len() == 0 ? FString(TEXT("Global")) : NsName;
	}

	/**
	 * Builds the declaration lines for one function/method (including doc/tags/signature).
	 * Returns false if the function should be skipped entirely, mirroring the
	 * filters in FAngelscriptDebugServer::SendDebugDatabase's MakeFuncDesc.
	 */
	bool MakeFunctionLines(
		asCScriptEngine* ScriptEngine,
		asCScriptFunction* ScriptFunction,
		TArray<FString>& OutLines,
		bool bIsConstructor,
		const FString& OverrideName,
		const FString& Indent,
		int32& OutMemberCount)
	{
		const int32 ArgCount = ScriptFunction->GetParamCount();
		const int32 HiddenParam = ScriptFunction->hiddenArgumentIndex;
		asCObjectType* ObjType = ((asCScriptFunction*)ScriptFunction)->objectType;

		FString Name = ANSI_TO_TCHAR(ScriptFunction->GetName());
		if (Name.StartsWith(TEXT("__")))
			return false;

		if (OverrideName.Len() != 0)
			Name = OverrideName;

		asDWORD ReturnFlags;
		int32 ReturnTypeId = ScriptFunction->GetReturnTypeId(&ReturnFlags);
		FString ReturnType = MakeDecl(ScriptEngine, ReturnTypeId, &ReturnFlags);

		bool bIsBlueprintEvent = false;
		TArray<FString> TagLines;
		TArray<FString> DocLines;

		UFunction* UnrealFunction = FAngelscriptDocs::LookupAngelscriptFunction(ScriptFunction->GetId());
		if (UnrealFunction != nullptr)
		{
			if (UnrealFunction->HasAnyFunctionFlags(FUNC_BlueprintEvent))
			{
				TagLines.Add(Indent + TEXT("// @event"));
				bIsBlueprintEvent = true;
			}

			if (UnrealFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_BlueprintPure))
				TagLines.Add(Indent + TEXT("// @ufunction"));

			if (Name != UnrealFunction->GetName())
				TagLines.Add(Indent + TEXT("// @unrealname ") + UnrealFunction->GetName());

#if WITH_EDITOR
			const FString& Keywords = UnrealFunction->GetMetaData(NAME_AS_ScriptKeywords);
			if (Keywords.Len() != 0)
				TagLines.Add(Indent + TEXT("// @keywords ") + Keywords.Replace(TEXT(","), TEXT(";")));

			for (FName MetaTag : NAMES_AS_InformedMeta)
			{
				const FString& MetaValue = UnrealFunction->GetMetaData(MetaTag);
				if (!MetaValue.IsEmpty())
					TagLines.Add(Indent + FString::Printf(TEXT("// @meta %s=%s"), *MetaTag.ToString(), *MetaValue));
			}
#endif
		}
		else
		{
			const TMap<FString, FString>* ScriptMeta = FAngelscriptDocs::GetScriptFunctionMeta(ScriptFunction->GetId());
			if (ScriptMeta != nullptr)
			{
				for (auto& MetaTag : *ScriptMeta)
					TagLines.Add(Indent + FString::Printf(TEXT("// @meta %s=%s"), *MetaTag.Key, *MetaTag.Value));
			}
		}

		if (ScriptFunction->IsSetterAndOnlyValidDuringInitDefaults())
			TagLines.Add(Indent + TEXT("// @defaultsOnly"));

		if (ScriptFunction->traits.GetTrait(asTRAIT_NOT_CALLABLE))
			TagLines.Add(Indent + TEXT("// @notCallable"));

		// Property-accessor trait. Mirrors the debug database encoding: bound
		// functions are implicitly property accessors (engine config
		// bAllowImplicitPropertyAccessors = true), so the flag is only written
		// when false. The LSP pairs Get*/Set* methods into virtual properties
		// at index time (x.BaseValue <=> GetBaseValue()/SetBaseValue()).
		if (!ScriptFunction->IsProperty())
			TagLines.Add(Indent + TEXT("// @notProperty"));

		if (ScriptFunction->determinesOutputTypeArgumentIndex != -1)
		{
			int32 OutputTypeIndex = ScriptFunction->determinesOutputTypeArgumentIndex;
			if (ScriptFunction->hiddenArgumentIndex != -1 && ScriptFunction->hiddenArgumentIndex < OutputTypeIndex)
				OutputTypeIndex -= 1;
			TagLines.Add(Indent + FString::Printf(TEXT("// @outputTypeIndex %d"), OutputTypeIndex));
		}

		const FString& Doc = FAngelscriptDocs::GetUnrealDocumentation(ScriptFunction->GetId());
		AppendDocComment(DocLines, Doc, Indent);

		// Parameters
		TArray<FString> ParamTexts;
		for (int32 ArgIndex = 0; ArgIndex < ArgCount; ++ArgIndex)
		{
			if (ArgIndex == HiddenParam)
				continue;

			const char* ParamNameRaw;
			const char* ParamDefaultRaw;
			asDWORD ParamFlags;
			int32 ParamTypeId;

			ScriptFunction->GetParam(ArgIndex, &ParamTypeId, &ParamFlags, &ParamNameRaw, &ParamDefaultRaw);

			// Ignore parameters starting with __, these are internal only
			if (ParamNameRaw != nullptr && ParamNameRaw[0] == '_' && ParamNameRaw[1] == '_')
				continue;

			auto& ScriptParam = ((asCScriptFunction*)ScriptFunction)->parameterTypes[ArgIndex];
			if (ScriptParam.GetTokenType() == ttQuestion)
			{
				ParamTexts.Add(TEXT("? ") + MakeParameterName(ParamNameRaw, ParamTexts.Num()));
				continue;
			}

			if (ParamTypeId == -1)
				return false; // Cannot express this function

			FString Decl = MakeDecl(ScriptEngine, ParamTypeId, &ParamFlags);

			// Check if we should add an &in to the type (matches the debug server behavior)
			if ((ParamFlags & (asTM_INOUTREF | asTM_CONST)) == (asTM_INREF | asTM_CONST) && ObjType != nullptr && ObjType->templateSubTypes.GetLength() == 0)
				Decl = TEXT("const ") + Decl + TEXT("&in");

			if (bIsBlueprintEvent)
			{
				// Even if float is float64, blueprint events still take float32s,
				// expressed as 'float' - the calling code auto-extends to float64.
				if (ParamTypeId == asTYPEID_FLOAT32 && (ParamFlags & asTM_INOUTREF) == 0)
					Decl = TEXT("float");
			}

			FString ParamText = Decl + TEXT(" ") + MakeParameterName(ParamNameRaw, ParamTexts.Num());
			if (ParamDefaultRaw != nullptr)
			{
				FString ParamDefault = ANSI_TO_TCHAR(ParamDefaultRaw);
				if (ParamDefault.Len() != 0)
					ParamText += TEXT(" = ") + ParamDefault;
			}

			ParamTexts.Add(ParamText);
		}

		// Signature
		FString Signature;
		if (bIsConstructor)
		{
			Signature = Name + TEXT("(") + FString::Join(ParamTexts, TEXT(", ")) + TEXT(")");
		}
		else
		{
			Signature = ReturnType + TEXT(" ") + Name + TEXT("(") + FString::Join(ParamTexts, TEXT(", ")) + TEXT(")");
			if (ScriptFunction->GetObjectType() != nullptr && ScriptFunction->IsReadOnly())
				Signature += TEXT(" const");
		}

		const bool bIsProtected = ScriptFunction->GetObjectType() != nullptr && ScriptFunction->IsProtected();
		if (bIsProtected)
			Signature = TEXT("protected ") + Signature;

		OutLines.Append(DocLines);
		OutLines.Append(TagLines);
		OutLines.Add(Indent + Signature + TEXT(";"));

		OutMemberCount += 1;
		return true;
	}
}

FTypeExportResult FTypeDeclarationExporter::ExportAll()
{
	FTypeExportResult Result;

	auto* ScriptEngine = FAngelscriptManager::Get().Engine;
	if (ScriptEngine == nullptr)
	{
		Result.Summary = TEXT("Angelscript engine is not initialized.");
		return Result;
	}

	const FString OutputDir = FPaths::ProjectSavedDir() / TEXT("AS-Cache");
	{
		// Start from a clean directory so stale groups don't linger
		IFileManager::Get().DeleteDirectory(*OutputDir, false, true);
		IFileManager::Get().MakeDirectory(*OutputDir, true);
	}
	Result.OutputDir = OutputDir;

	TMap<FString, TArray<FString>> Groups;
	int32 TypeCount = 0;
	int32 MemberCount = 0;

	const int32 TotalTypes = ScriptEngine->GetObjectTypeCount() + ScriptEngine->GetEnumCount() + 16;
	FScopedSlowTask Task(TotalTypes, NSLOCTEXT("AngelscriptTypeExporter", "ExportingTypeDeclarations", "Exporting Angelscript type declarations (.d.as)..."));
	if (FSlateApplication::IsInitialized())
		Task.MakeDialog();

	// =========================================================================
	// Object types (classes / structs / templates)
	// Mirrors SendDebugDatabase's object type loop, plus a filter for
	// script-defined types (asOBJ_SCRIPT_OBJECT) - those are fully
	// recoverable from the .as sources by the LSP itself.
	// =========================================================================
	int32 TypeIndex = 0;
	const int32 ObjectTypeCount = ScriptEngine->GetObjectTypeCount();
	for (; TypeIndex < ObjectTypeCount; ++TypeIndex)
	{
		Task.EnterProgressFrame(1);

		auto* ScriptType = ScriptEngine->GetObjectTypeByIndex(TypeIndex);
		FString TypeName = ANSI_TO_TCHAR(ScriptType->GetName());
		int32 ClassTypeId = ScriptType->GetTypeId();

		if (TypeName.IsEmpty() || TypeName[0] == '_')
			continue;

		if ((ScriptType->GetFlags() & asOBJ_SCRIPT_OBJECT) != 0)
			continue; // script classes/structs - parsed from .as source by the LSP

		if ((ScriptType->GetFlags() & asOBJ_FUNCDEF) != 0)
			continue; // function definitions - delegates are expanded from .as source

		const FString Group = ResolveClassGroup(ScriptType, TypeName);
		TArray<FString>& Lines = Groups.FindOrAdd(Group);

		TArray<FString> Body;
		TArray<FString> HeadDoc;
		TArray<FString> HeadTags;
		TSet<FString> DocumentedProperties;

		// ---------------------------------------------------------------
		// Properties (mirrors the property loop, incl. generated accessors)
		// ---------------------------------------------------------------
		{
			const int32 PropertyCount = ScriptType->GetPropertyCount();
			for (int32 PropertyIndex = 0; PropertyIndex < PropertyCount; ++PropertyIndex)
			{
				const char* Name;
				int TypeId;
				int Offset;
				ScriptType->GetProperty(PropertyIndex, &Name, &TypeId, nullptr, nullptr, &Offset);
				asCObjectProperty* ObjProp = ((asCObjectType*)ScriptType)->properties[PropertyIndex];

#if WITH_EDITOR
				if (ObjProp->isDeprecated)
					continue;
#endif

				FString PropName = ANSI_TO_TCHAR(Name);
				FString Decl = MakeDecl(ScriptEngine, TypeId);

				TArray<FString> PropTags;
				if (ObjProp->isProtected)
					Decl = TEXT("protected ") + Decl;

				// Debug-server flags: 0x1 = plain read/write (default), 0x2 = editable in defaults only
				const bool bWriteableOrReadable = ObjProp->Writeable() || ObjProp->Readable();
				if (!bWriteableOrReadable && ObjProp->Editable())
					PropTags.Add(TEXT("    // @editable"));

				TArray<FString> PropDoc;
				const FString& PropDocumentation =
					FAngelscriptDocs::GetUnrealDocumentationForProperty(ClassTypeId, Offset);
				AppendDocComment(PropDoc, PropDocumentation, TEXT("    "));

				Body.Append(PropDoc);
				Body.Append(PropTags);
				Body.Add(FString::Printf(TEXT("    %s %s;"), *Decl, *PropName));

				DocumentedProperties.Add(PropName);
				MemberCount += 1;
			}
		}

		// ---------------------------------------------------------------
		// Methods + generated accessors turned into properties
		// ---------------------------------------------------------------
		{
			const int32 MethodCount = ScriptType->GetMethodCount();
			for (int32 MethodIndex = 0; MethodIndex < MethodCount; ++MethodIndex)
			{
				auto* ScriptFunction = (asCScriptFunction*)ScriptType->GetMethodByIndex(MethodIndex);
				if (ScriptFunction == nullptr)
					continue;
				if (ScriptFunction->traits.GetTrait(asTRAIT_DEPRECATED))
					continue;

				if (ScriptFunction->traits.GetTrait(asTRAIT_GENERATED_FUNCTION))
				{
					// Generated Get/Set accessors are documented as properties,
					// to be transparent and match what is in C++.
					FString PropertyName = ANSI_TO_TCHAR(ScriptFunction->GetName());
					int TypeId = -1;

					if (PropertyName.StartsWith(TEXT("Get")))
					{
						PropertyName = PropertyName.Mid(3);
						TypeId = ScriptEngine->GetTypeIdFromDataType(ScriptFunction->returnType);
					}
					else if (PropertyName.StartsWith(TEXT("Set")))
					{
						PropertyName = PropertyName.Mid(3);
						TypeId = ScriptEngine->GetTypeIdFromDataType(ScriptFunction->parameterTypes[0]);
					}

					if (TypeId == -1)
						continue;

					// If the property is already documented, don't add it again
					if (DocumentedProperties.Contains(PropertyName))
						continue;

					FString Decl = MakeDecl(ScriptEngine, TypeId);

					TArray<FString> PropDoc;
					const FString& Doc = FAngelscriptDocs::GetUnrealDocumentation(ScriptFunction->GetId());
					AppendDocComment(PropDoc, Doc, TEXT("    "));

					Body.Append(PropDoc);
					Body.Add(FString::Printf(TEXT("    %s %s;"), *Decl, *PropertyName));

					MemberCount += 1;
				}
				else
				{
					MakeFunctionLines(ScriptEngine, ScriptFunction, Body, false, FString(), TEXT("    "), MemberCount);
				}
			}
		}

		// ---------------------------------------------------------------
		// Constructors / factories become constructor declarations in the type body
		// ---------------------------------------------------------------
		{
			const int32 BehCount = ScriptType->GetBehaviourCount();
			for (int32 BehIndex = 0; BehIndex < BehCount; ++BehIndex)
			{
				asEBehaviours BehType;
				auto* ScriptFunction = (asCScriptFunction*)ScriptType->GetBehaviourByIndex(BehIndex, &BehType);
				if (ScriptFunction == nullptr)
					continue;
				if (ScriptFunction->traits.GetTrait(asTRAIT_DEPRECATED))
					continue;

				if ((ScriptType->GetFlags() & asOBJ_REF) != 0)
				{
					if (BehType != asEBehaviours::asBEHAVE_FACTORY)
						continue;
				}
				else
				{
					if (BehType != asEBehaviours::asBEHAVE_CONSTRUCT)
						continue;
				}

				TArray<FString> CtorDoc;
				const FString& Doc = FAngelscriptDocs::GetUnrealDocumentation(ScriptFunction->GetId());
				AppendDocComment(CtorDoc, Doc, TEXT("    "));

				TArray<FString> CtorLines;
				if (MakeFunctionLines(ScriptEngine, ScriptFunction, CtorLines, true, TypeName, TEXT("    "), MemberCount))
				{
					Body.Append(CtorDoc);
					Body.Append(CtorLines);
				}
			}
		}

		// ---------------------------------------------------------------
		// Type header: template parameters, inheritance, doc, keywords
		// ---------------------------------------------------------------
		FString TypeHead;
		if ((ScriptType->GetFlags() & asOBJ_VALUE) != 0)
			TypeHead = TEXT("struct ");
		else
			TypeHead = TEXT("class ");

		TypeHead += TypeName;

		if (ScriptType->GetSubTypeCount() != 0)
		{
			// Template definition: emit the type parameter list, e.g. TArray<T>
			TArray<FString> SubTypeNames;
			for (int32 i = 0, Count = ScriptType->GetSubTypeCount(); i < Count; ++i)
				SubTypeNames.Add(ANSI_TO_TCHAR(ScriptType->GetSubType(i)->GetName()));
			TypeHead += TEXT("<") + FString::Join(SubTypeNames, TEXT(", ")) + TEXT(">");
		}

		auto ASType = FAngelscriptType::GetByAngelscriptTypeName(TypeName);
		if (ASType.IsValid())
		{
			UClass* ClassPtr = ASType->GetClass(FAngelscriptTypeUsage::DefaultUsage);
			if (ClassPtr != nullptr)
			{
				UClass* SuperPtr = ClassPtr->GetSuperClass();
				while (SuperPtr != nullptr)
				{
					auto SuperType = FAngelscriptType::GetByClass(SuperPtr);
					if (SuperType.IsValid())
					{
						TypeHead += TEXT(" : ") + SuperType->GetAngelscriptTypeName();
						break;
					}

					SuperPtr = SuperPtr->GetSuperClass();
				}
			}
		}

		if ((ScriptType->GetFlags() & asOBJ_TEMPLATE_SUBTYPE_COVARIANT) != 0)
			HeadTags.Add(TEXT("// @template_covariant"));
		if ((ScriptType->GetFlags() & asOBJ_TEMPLATE_INHERIT_SPECIALIZATIONS) != 0)
			HeadTags.Add(TEXT("// @template_inherit_specializations"));

#if WITH_EDITOR
		if ((ScriptType->GetFlags() & asOBJ_REF) != 0)
		{
			UClass* UnrealClass = (UClass*)ScriptType->GetUserData();
			if (UnrealClass)
			{
				const FString& Keywords = UnrealClass->GetMetaData(NAME_AS_ScriptKeywords);
				if (Keywords.Len() != 0)
					HeadTags.Add(TEXT("// @keywords ") + Keywords.Replace(TEXT(","), TEXT(";")));
			}
		}
#endif

		AppendDocComment(HeadDoc, FAngelscriptDocs::GetUnrealDocumentationForType(ClassTypeId), TEXT(""));

		if (Lines.Num() != 0)
			Lines.Add(TEXT(""));

		Lines.Append(HeadDoc);
		Lines.Append(HeadTags);
		Lines.Add(TypeHead);
		Lines.Add(TEXT("{"));
		Lines.Append(Body);
		Lines.Add(TEXT("}"));

		TypeCount += 1;
	}

	// =========================================================================
	// Template specialization functions: extra methods bound onto specific
	// template instances (e.g. TArray<FVector>-only methods).
	// Emitted as "@templateSpecialization class Name<Args>" blocks.
	// =========================================================================
	for (auto& TemplateSpecializationElem : ScriptEngine->registeredTemplateSpecializationFunctions)
	{
		Task.EnterProgressFrame(1);

		asCObjectType* ScriptType = TemplateSpecializationElem.Key;

		asCDataType DataType = asCDataType::CreateType(ScriptType, false);
		FString TypeName = ANSI_TO_TCHAR(DataType.Format(nullptr).AddressOf());
		if (TypeName.IsEmpty() || TypeName[0] == '_')
			continue;

		TArray<FString>& Lines = Groups.FindOrAdd(ResolveClassGroup(ScriptType, TypeName));

		TArray<FString> Body;
		for (asCScriptFunction* ScriptFunction : TemplateSpecializationElem.Value)
			MakeFunctionLines(ScriptEngine, ScriptFunction, Body, false, FString(), TEXT("    "), MemberCount);

		if (Body.Num() == 0)
			continue;

		if (Lines.Num() != 0)
			Lines.Add(TEXT(""));

		Lines.Add(TEXT("// @templateSpecialization"));
		Lines.Add(TEXT("class ") + TypeName);
		Lines.Add(TEXT("{"));
		Lines.Append(Body);
		Lines.Add(TEXT("}"));

		TypeCount += 1;
	}

	// =========================================================================
	// Global functions / global variables / enums, grouped by namespace
	// =========================================================================
	{
		struct FNamespaceContent
		{
			TArray<asCScriptFunction*> Functions;
			TArray<asCGlobalProperty*> Variables;
		};
		TMap<FString, FNamespaceContent> Namespaces;

		const int32 GlobalFuncCount = ScriptEngine->GetGlobalFunctionCount();
		for (int32 GlobalFuncIndex = 0; GlobalFuncIndex < GlobalFuncCount; ++GlobalFuncIndex)
		{
			asCScriptFunction* Func = (asCScriptFunction*)ScriptEngine->GetGlobalFunctionByIndex(GlobalFuncIndex);
			if (Func->traits.GetTrait(asTRAIT_DEPRECATED))
				continue;
			Namespaces.FindOrAdd(ANSI_TO_TCHAR(Func->GetNamespace())).Functions.Add(Func);
		}

		for (asUINT PropIndex = 0, PropCount = ScriptEngine->registeredGlobalProps.GetLength(); PropIndex < PropCount; ++PropIndex)
		{
			asCGlobalProperty* Prop = ScriptEngine->registeredGlobalProps[PropIndex];
			Namespaces.FindOrAdd(ANSI_TO_TCHAR(Prop->nameSpace->GetName())).Variables.Add(Prop);
		}

		const int32 EnumCount = ScriptEngine->GetEnumCount();
		for (int32 EnumIndex = 0; EnumIndex < EnumCount; ++EnumIndex)
		{
			Task.EnterProgressFrame(1);

			auto* EnumType = (asCTypeInfo*)ScriptEngine->GetEnumByIndex(EnumIndex);
			FString EnumName = ANSI_TO_TCHAR(EnumType->GetName());
			if (EnumName.IsEmpty() || EnumName[0] == '_')
				continue;

			UEnum* UnrealEnum = (UEnum*)EnumType->GetUserData();

			// Enums resolve into their own namespace (MyEnum::Value), matching
			// how the debug database exposes them and how scripts use them.
			TArray<FString>& Lines = Groups.FindOrAdd(ResolveClassGroup(EnumType, EnumName));

			TArray<FString> HeadDoc;
			AppendDocComment(HeadDoc, FAngelscriptDocs::GetUnrealDocumentationForType(EnumType->GetTypeId()), TEXT(""));

			if (Lines.Num() != 0)
				Lines.Add(TEXT(""));

			Lines.Append(HeadDoc);
			Lines.Add(TEXT("enum ") + EnumName);
			Lines.Add(TEXT("{"));

			for (int32 i = 0, Count = EnumType->GetEnumValueCount(); i < Count; ++i)
			{
				int Value;
				const char* ValueName = EnumType->GetEnumValueByIndex(i, &Value);

				FString ValueDoc;
#if WITH_EDITOR
				if (UnrealEnum != nullptr)
				{
					int32 ValueIndex = UnrealEnum->GetIndexByValue(Value);
					ValueDoc = UnrealEnum->GetMetaData(TEXT("ToolTip"), ValueIndex);
				}
#endif

				TArray<FString> ValueDocLines;
				AppendDocComment(ValueDocLines, ValueDoc, TEXT("    "));
				Lines.Append(ValueDocLines);
				Lines.Add(FString::Printf(TEXT("    %s = %d,"), ANSI_TO_TCHAR(ValueName), Value));
			}

			Lines.Add(TEXT("}"));

			TypeCount += 1;
		}

		// Emit namespaces (including the global, empty namespace)
		for (auto& NsPair : Namespaces)
		{
			const FString& NsName = NsPair.Key;
			FNamespaceContent& Content = NsPair.Value;

			TArray<FString> Body;
			for (asCScriptFunction* ScriptFunction : Content.Functions)
				MakeFunctionLines(ScriptEngine, ScriptFunction, Body, false, FString(), TEXT("    "), MemberCount);

			for (asCGlobalProperty* GlobalProperty : Content.Variables)
			{
				if (GlobalProperty->name[0] == '_')
					continue;

				FString Decl = MakeDecl(ScriptEngine, ScriptEngine->GetTypeIdFromDataType(GlobalProperty->type));
				FString GlobalName = ANSI_TO_TCHAR(GlobalProperty->name.AddressOf());

				TArray<FString> PropDoc;
				AppendDocComment(PropDoc,
					FAngelscriptDocs::GetDocumentationForGlobalVariable(GlobalProperty->id),
					TEXT("    "));

				Body.Append(PropDoc);
				Body.Add(FString::Printf(TEXT("    %s %s;"), *Decl, *GlobalName));
				MemberCount += 1;
			}

			if (Body.Num() == 0)
				continue;

			TArray<FString>& Lines = Groups.FindOrAdd(ResolveNamespaceGroup(NsName));
			if (Lines.Num() != 0)
				Lines.Add(TEXT(""));

			if (NsName.Len() != 0)
			{
				Lines.Add(TEXT("namespace ") + NsName);
				Lines.Add(TEXT("{"));
				Lines.Append(Body);
				Lines.Add(TEXT("}"));
			}
			else
			{
				Lines.Add(TEXT("// Global functions and variables (no namespace)"));
				Lines.Append(Body);
			}

			TypeCount += 1;
		}
	}

	// =========================================================================
	// Write files
	// =========================================================================
	{
		Groups.KeySort([](const FString& A, const FString& B) { return A < B; });

		TArray<FString> ManifestLines;
		ManifestLines.Add(FString::Printf(TEXT("AS_TYPE_CACHE v%d"), AS_DECL_CACHE_FORMAT_VERSION));
		ManifestLines.Add(FString::Printf(TEXT("engine_version=%s"), *FEngineVersion::Current().ToString()));
		ManifestLines.Add(FString::Printf(TEXT("float_is_float64=%s"),
			GetDefault<UAngelscriptSettings>()->bScriptFloatIsFloat64 ? TEXT("true") : TEXT("false")));
		ManifestLines.Add(FString::Printf(TEXT("exported_utc=%s"), *FDateTime::UtcNow().ToString()));
		ManifestLines.Add(FString::Printf(TEXT("type_count=%d"), TypeCount));
		ManifestLines.Add(FString::Printf(TEXT("member_count=%d"), MemberCount));

		TArray<FString> FileNames;
		for (auto& Pair : Groups)
		{
			const FString FileName = MakeGroupFileName(Pair.Key);
			const FString FilePath = OutputDir / FileName;

			TArray<FString> FileLines;
			FileLines.Add(TEXT("// ==== AngelscriptTypeDeclarations ===="));
			FileLines.Add(FString::Printf(TEXT("// @cache_format %d"), AS_DECL_CACHE_FORMAT_VERSION));
			FileLines.Add(FString::Printf(TEXT("// @group %s"), *Pair.Key));
			FileLines.Add(TEXT("// Generated by AngelscriptTypeExporter - do not edit."));
			FileLines.Add(TEXT(""));
			FileLines.Append(Pair.Value);

			if (FFileHelper::SaveStringArrayToFile(FileLines, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
			{
				FileNames.Add(FileName);
				Result.WrittenFiles.Add(FilePath);
			}
		}

		ManifestLines.Add(FString::Printf(TEXT("files=%s"), *FString::Join(FileNames, TEXT(","))));

		const FString ManifestPath = OutputDir / TEXT("_manifest.dctx");
		FFileHelper::SaveStringArrayToFile(ManifestLines, *ManifestPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	Result.TypeCount = TypeCount;
	Result.MemberCount = MemberCount;
	Result.Summary = FString::Printf(
		TEXT("Exported %d types / %d members to %d files in %s"),
		TypeCount, MemberCount, Result.WrittenFiles.Num(), *OutputDir);

	return Result;
}
