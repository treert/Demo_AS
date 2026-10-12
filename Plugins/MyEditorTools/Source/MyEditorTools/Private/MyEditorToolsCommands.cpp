// Copyright Epic Games, Inc. All Rights Reserved.

#include "MyEditorToolsCommands.h"

#define LOCTEXT_NAMESPACE "FMyEditorToolsModule"

void FMyEditorToolsCommands::RegisterCommands()
{
	UI_COMMAND(PluginAction, "MyEditorTools", "Execute MyEditorTools action", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ExportUEClassInfo, "Export UE Class Info", "Export UClass/UEnum/UScriptStruct info to YAML (UEAnnotation)", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
