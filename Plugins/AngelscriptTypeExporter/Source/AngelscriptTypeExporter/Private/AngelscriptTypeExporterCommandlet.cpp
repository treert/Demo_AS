#include "AngelscriptTypeExporterCommandlet.h"
#include "TypeDeclarationExporter.h"

#include "AngelscriptManager.h"
#include "AngelscriptCodeModule.h"

DEFINE_LOG_CATEGORY_STATIC(LogAngelscriptTypeExporter, Display, All);

UAngelscriptTypeExporterCommandlet::UAngelscriptTypeExporterCommandlet()
{
	IsClient = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UAngelscriptTypeExporterCommandlet::Main(const FString& Params)
{
	// The AngelscriptLoader module normally initializes the engine at startup,
	// but guard for commandlet load-order edge cases by initializing idempotently.
	if (!FAngelscriptManager::IsInitialized())
		FAngelscriptCodeModule::InitializeAngelscript();

	if (!FAngelscriptManager::IsInitialized())
	{
		UE_LOG(LogAngelscriptTypeExporter, Error, TEXT("Angelscript engine failed to initialize - cannot export types."));
		return 1;
	}

	UE_LOG(LogAngelscriptTypeExporter, Display, TEXT("Exporting Angelscript type declarations..."));
	const FTypeExportResult Result = FTypeDeclarationExporter::ExportAll();
	UE_LOG(LogAngelscriptTypeExporter, Display, TEXT("%s"), *Result.Summary);

	for (const FString& File : Result.WrittenFiles)
		UE_LOG(LogAngelscriptTypeExporter, Display, TEXT("  %s"), *File);

	return Result.WrittenFiles.Num() != 0 ? 0 : 1;
}
