#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "AngelscriptTypeExporterCommandlet.generated.h"

/**
 * Headless entry point for the type declaration exporter.
 *
 *   UnrealEditor-Cmd.exe <Project.uproject> -run=AngelscriptTypeExporter -unattended -nopause -nosplash
 *
 * Writes the same output as the editor menu command
 * (Tools > Export Angelscript Type Declarations) to Saved/AS-Cache/.
 */
UCLASS()
class UAngelscriptTypeExporterCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UAngelscriptTypeExporterCommandlet();

	virtual int32 Main(const FString& Params) override;
};
