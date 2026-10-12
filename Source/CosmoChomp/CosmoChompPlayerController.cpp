// Copyright Epic Games, Inc. All Rights Reserved.


#include "CosmoChompPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "CosmoChomp.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UObject/ConstructorHelpers.h"
#include "CosmoChompPauseMenu.h"

ACosmoChompPlayerController::ACosmoChompPlayerController()
{
	// Mapped in IMC_Default (Escape, P, gamepad Start) by Tools/AssetPipeline
	static ConstructorHelpers::FObjectFinder<UInputAction> Pause(TEXT("/Game/Input/Actions/IA_Pause"));
	PauseAction = Pause.Object;
	PauseMenuClass = UCosmoChompPauseMenu::StaticClass();
}

void ACosmoChompPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// only spawn touch controls on local player controllers
	if (IsLocalPlayerController() && ShouldUseTouchControls())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogCosmoChomp, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}
}

void ACosmoChompPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (PauseAction)
		{
			EnhancedInputComponent->BindAction(PauseAction, ETriggerEvent::Started, this, &ACosmoChompPlayerController::TogglePauseMenu);
		}
	}

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Contexts
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}
}

bool ACosmoChompPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

void ACosmoChompPlayerController::TogglePauseMenu()
{
	if (PauseMenu && PauseMenu->IsInViewport())
	{
		ClosePauseMenu();
	}
	else
	{
		OpenPauseMenu();
	}
}

void ACosmoChompPlayerController::OpenPauseMenu()
{
	if (!IsLocalPlayerController() || !PauseMenuClass || (PauseMenu && PauseMenu->IsInViewport()))
	{
		return;
	}

	if (!PauseMenu)
	{
		PauseMenu = CreateWidget<UCosmoChompPauseMenu>(this, PauseMenuClass);
	}
	if (!PauseMenu)
	{
		UE_LOG(LogCosmoChomp, Error, TEXT("Could not create the pause menu widget."));
		return;
	}

	PauseMenu->AddToViewport(100);
	SetPause(true);
	SetShowMouseCursor(true);

	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
	PauseMenu->FocusFirstButton();
}

void ACosmoChompPlayerController::ClosePauseMenu()
{
	if (PauseMenu)
	{
		PauseMenu->RemoveFromParent();
	}

	SetPause(false);
	SetShowMouseCursor(false);
	SetInputMode(FInputModeGameOnly());
}

void ACosmoChompPlayerController::QuitGame()
{
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}
