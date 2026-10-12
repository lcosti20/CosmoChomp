// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CosmoChompPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class UUserWidget;
class UCosmoChompPauseMenu;

/**
 *  Basic PlayerController class for a third person game
 *  Manages input mappings
 */
UCLASS(abstract)
class ACosmoChompPlayerController : public APlayerController
{
	GENERATED_BODY()

public:

	ACosmoChompPlayerController();

	UFUNCTION(BlueprintCallable, Category = "Pause Menu")
	void TogglePauseMenu();

	/** Pauses the game and shows the pause menu */
	UFUNCTION(BlueprintCallable, Category = "Pause Menu")
	void OpenPauseMenu();

	/** Hides the pause menu and resumes the game */
	UFUNCTION(BlueprintCallable, Category = "Pause Menu")
	void ClosePauseMenu();

	UFUNCTION(BlueprintCallable, Category = "Pause Menu")
	void QuitGame();

protected:

	/** Opens/closes the pause menu; must have "Trigger when Paused" set so it can close it too */
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* PauseAction;

	UPROPERTY(EditAnywhere, Category = "Pause Menu")
	TSubclassOf<UCosmoChompPauseMenu> PauseMenuClass;

	UPROPERTY()
	TObjectPtr<UCosmoChompPauseMenu> PauseMenu;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category ="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;

};
