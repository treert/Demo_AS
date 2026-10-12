// Copyright Epic Games, Inc. All Rights Reserved.

#include "MyEditorToolsStyle.h"
#include "MyEditorTools.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/SlateStyleRegistry.h"
#include "Slate/SlateGameResources.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleMacros.h"

#define RootToContentDir Style->RootToContentDir

TSharedPtr<FSlateStyleSet> FMyEditorToolsStyle::StyleInstance = nullptr;

void FMyEditorToolsStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FMyEditorToolsStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	ensure(StyleInstance.IsUnique());
	StyleInstance.Reset();
}

FName FMyEditorToolsStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("MyEditorToolsStyle"));
	return StyleSetName;
}


const FVector2D Icon16x16(16.0f, 16.0f);
const FVector2D Icon20x20(20.0f, 20.0f);

TSharedRef< FSlateStyleSet > FMyEditorToolsStyle::Create()
{
	TSharedRef< FSlateStyleSet > Style = MakeShareable(new FSlateStyleSet("MyEditorToolsStyle"));
	Style->SetContentRoot(IPluginManager::Get().FindPlugin("MyEditorTools")->GetBaseDir() / TEXT("Resources"));

	Style->Set("MyEditorTools.PluginAction", new IMAGE_BRUSH_SVG(TEXT("PlaceholderButtonIcon"), Icon20x20));
	return Style;
}

void FMyEditorToolsStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}

const ISlateStyle& FMyEditorToolsStyle::Get()
{
	return *StyleInstance;
}
