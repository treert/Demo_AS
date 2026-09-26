#pragma once

#include "CoreMinimal.h"

struct FTypeExportResult
{
	/** Number of exported types (classes/structs/enums/namespaces). */
	int32 TypeCount = 0;
	/** Number of exported methods/functions/properties. */
	int32 MemberCount = 0;
	/** Absolute paths of all written files. */
	TArray<FString> WrittenFiles;
	/** Absolute path of the output directory. */
	FString OutputDir;
	/** Human readable summary for notification/dialogs. */
	FString Summary;
};

/**
 * Exports the Angelscript engine's registered C++ bound types to .d.as
 * declaration files (AngelScript-syntax declarations, no bodies), grouped
 * one file per engine module / namespace.
 *
 * Mirrors FAngelscriptDebugServer::SendDebugDatabase as the behavioral
 * reference - the same source of types, the same filtering rules, the same
 * doc/keyword metadata - but emits .d.as text instead of JSON, and skips
 * script-defined types (asOBJ_SCRIPT_OBJECT) since those are fully
 * recoverable from the .as sources by the LSP itself.
 */
class FTypeDeclarationExporter
{
public:
	static FTypeExportResult ExportAll();
};
