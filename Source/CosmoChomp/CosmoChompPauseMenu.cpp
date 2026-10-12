#include "CosmoChompPauseMenu.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "CosmoChompPlayerController.h"

void UCosmoChompPauseMenu::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Needed to receive the key that closes the menu
	SetIsFocusable(true);
}

TSharedRef<SWidget> UCosmoChompPauseMenu::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		// Full-screen dim with the menu centered on it
		UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Background"));
		Background->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.65f));
		Background->SetHorizontalAlignment(HAlign_Center);
		Background->SetVerticalAlignment(VAlign_Center);
		WidgetTree->RootWidget = Background;

		UVerticalBox* Menu = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Menu"));
		Background->SetContent(Menu);

		UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
		Title->SetText(NSLOCTEXT("CosmoChomp", "Paused", "Paused"));
		FSlateFontInfo TitleFont = Title->GetFont();
		TitleFont.Size = 56;
		Title->SetFont(TitleFont);
		if (UVerticalBoxSlot* TitleSlot = Menu->AddChildToVerticalBox(Title))
		{
			TitleSlot->SetHorizontalAlignment(HAlign_Center);
			TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 30.0f));
		}

		ContinueButton = MakeButton(NSLOCTEXT("CosmoChomp", "Continue", "Continue"));
		QuitButton = MakeButton(NSLOCTEXT("CosmoChomp", "Quit", "Quit"));
		for (UButton* Button : { ContinueButton.Get(), QuitButton.Get() })
		{
			if (UVerticalBoxSlot* ButtonSlot = Menu->AddChildToVerticalBox(Button))
			{
				ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
				ButtonSlot->SetPadding(FMargin(0.0f, 8.0f));
			}
		}

		ContinueButton->OnClicked.AddDynamic(this, &UCosmoChompPauseMenu::OnContinueClicked);
		QuitButton->OnClicked.AddDynamic(this, &UCosmoChompPauseMenu::OnQuitClicked);
	}

	return Super::RebuildWidget();
}

UButton* UCosmoChompPauseMenu::MakeButton(const FText& Label)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	Button->SetBackgroundColor(ButtonColor);

	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Text->SetText(Label);
	FSlateFontInfo Font = Text->GetFont();
	Font.Size = 30;
	Text->SetFont(Font);

	if (UButtonSlot* TextSlot = Cast<UButtonSlot>(Button->AddChild(Text)))
	{
		TextSlot->SetHorizontalAlignment(HAlign_Center);
		TextSlot->SetPadding(FMargin(80.0f, 12.0f));
	}
	return Button;
}

void UCosmoChompPauseMenu::FocusFirstButton()
{
	if (ContinueButton)
	{
		ContinueButton->SetKeyboardFocus();
	}
}

FReply UCosmoChompPauseMenu::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// The same keys that open the menu close it (while paused, the menu has focus)
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::P || Key == EKeys::Gamepad_Special_Right || Key == EKeys::Gamepad_FaceButton_Right)
	{
		OnContinueClicked();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UCosmoChompPauseMenu::OnContinueClicked()
{
	if (ACosmoChompPlayerController* Controller = GetOwningPlayer<ACosmoChompPlayerController>())
	{
		Controller->ClosePauseMenu();
	}
}

void UCosmoChompPauseMenu::OnQuitClicked()
{
	if (ACosmoChompPlayerController* Controller = GetOwningPlayer<ACosmoChompPlayerController>())
	{
		Controller->QuitGame();
	}
}
