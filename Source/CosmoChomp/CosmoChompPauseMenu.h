#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CosmoChompPauseMenu.generated.h"

class UButton;

/**
 *  Pause menu with "Continue" and "Quit". The layout is built in C++, so no widget
 *  Blueprint is needed; ACosmoChompPlayerController opens and closes it.
 */
UCLASS()
class UCosmoChompPauseMenu : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Gives keyboard/gamepad focus to the first button */
	void FocusFirstButton();

protected:
	virtual void NativeOnInitialized() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(EditAnywhere, Category = "Pause Menu")
	FLinearColor ButtonColor = FLinearColor(0.15f, 0.2f, 0.35f);

	UPROPERTY()
	TObjectPtr<UButton> ContinueButton;

	UPROPERTY()
	TObjectPtr<UButton> QuitButton;

	UButton* MakeButton(const FText& Label);

	UFUNCTION()
	void OnContinueClicked();

	UFUNCTION()
	void OnQuitClicked();
};
