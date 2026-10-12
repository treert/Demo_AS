// Copyright Epic Games, Inc. All Rights Reserved.

#include "MyEditorTools.h"
#include "MyEditorToolsStyle.h"
#include "MyEditorToolsCommands.h"
#include "ExportUEClassInfoUtils.h"
#include "Misc/MessageDialog.h"
#include "ToolMenus.h"

static const FName MyEditorToolsTabName("MyEditorTools");

#define LOCTEXT_NAMESPACE "FMyEditorToolsModule"

void FMyEditorToolsModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	FMyEditorToolsStyle::Initialize();
	FMyEditorToolsStyle::ReloadTextures();

	FMyEditorToolsCommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FMyEditorToolsCommands::Get().PluginAction,
		FExecuteAction::CreateRaw(this, &FMyEditorToolsModule::PluginButtonClicked),
		FCanExecuteAction());

	PluginCommands->MapAction(
		FMyEditorToolsCommands::Get().ExportUEClassInfo,
		FExecuteAction::CreateRaw(this, &FMyEditorToolsModule::ExportUEClassInfoClicked),
		FCanExecuteAction());

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FMyEditorToolsModule::RegisterMenus));
}

void FMyEditorToolsModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);

	if (UToolMenus* ToolMenus = UToolMenus::TryGet())
	{
		ToolMenus->RemoveMenu("LevelEditor.MainMenu.MyTools");
	}

	FMyEditorToolsStyle::Shutdown();

	FMyEditorToolsCommands::Unregister();
}

void FMyEditorToolsModule::PluginButtonClicked()
{
	// Put your "OnButtonClicked" stuff here
	FText DialogText = FText::Format(
							LOCTEXT("PluginButtonDialogText", "Add code to {0} in {1} to override this button's actions"),
							FText::FromString(TEXT("FMyEditorToolsModule::PluginButtonClicked()")),
							FText::FromString(TEXT("MyEditorTools.cpp"))
					   );
	FMessageDialog::Open(EAppMsgType::Ok, DialogText);
}

void FMyEditorToolsModule::ExportUEClassInfoClicked()
{
	FExportUEClassInfoUtils::Export();
}

void FMyEditorToolsModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		// 在主菜单栏添加 MyTools 顶级入口（引擎 Build/Select 菜单同款写法）
		UToolMenu* MenuBar = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu");
		{
			FToolMenuSection& Section = MenuBar->FindOrAddSection(NAME_None);
			FToolMenuEntry& MyToolsEntry = Section.AddSubMenu(
				"MyTools",
				LOCTEXT("MyToolsMenu", "MyTools"),
				LOCTEXT("MyToolsMenu_ToolTip", "My Editor Tools"),
				FNewToolMenuChoice()
			);
			MyToolsEntry.InsertPosition = FToolMenuInsert("Help", EToolMenuInsertType::After);
		}

		// MyTools 子菜单内容
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.MyTools");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("MyEditorTools");
			Section.AddMenuEntryWithCommandList(FMyEditorToolsCommands::Get().PluginAction, PluginCommands);
			Section.AddMenuEntryWithCommandList(FMyEditorToolsCommands::Get().ExportUEClassInfo, PluginCommands);
		}
	}

	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("PluginTools");
			{
				FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FMyEditorToolsCommands::Get().PluginAction));
				Entry.SetCommandList(PluginCommands);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FMyEditorToolsModule, MyEditorTools)