// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"
#include "MyEditorToolsStyle.h"

class FMyEditorToolsCommands : public TCommands<FMyEditorToolsCommands>
{
public:

	FMyEditorToolsCommands()
		: TCommands<FMyEditorToolsCommands>(TEXT("MyEditorTools"), NSLOCTEXT("Contexts", "MyEditorTools", "MyEditorTools Plugin"), NAME_None, FMyEditorToolsStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > PluginAction;
};
