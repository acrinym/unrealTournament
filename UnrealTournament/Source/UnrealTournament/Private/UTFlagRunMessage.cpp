// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTFlagRunMessage.h"
#include "UTAnnouncer.h"
#include "UTFlagRunScoreboard.h"

/*
1: Canvas->DrawText(UTHUDOwner->SmallFont, FText::Format(NSLOCTEXT("UTFlagRun", "DefendersMustStop", " must hold on defense to have \n a chance."), Args).ToString(), XOffset, YPos, RenderScale, RenderScale, TextRenderInfo);
2: BRONZE	Canvas->DrawText(UTHUDOwner->SmallFont, FText::Format(NSLOCTEXT("UTFlagRun", "DefendersMustHold", " must hold {AttackerName} to {BonusType} \n to have a chance."), Args).ToString(), XOffset, YPos, RenderScale, RenderScale, TextRenderInfo);
3: SILVER
4: BRONZE 	Canvas->DrawText(UTHUDOwner->SmallFont, FText::Format(NSLOCTEXT("UTFlagRun", "AttackersMustScore", " must score {BonusType} to have a chance."), Args).ToString(), XOffset, YPos, RenderScale, RenderScale, TextRenderInfo);
5: SILVER
6: GOLD
7: Canvas->DrawText(UTHUDOwner->SmallFont, FText::Format(NSLOCTEXT("UTFlagRun", "UnhandledCondition", "UNHANDLED WIN CONDITION"), Args).ToString(), XOffset, YPos, RenderScale, RenderScale, TextRenderInfo);
8: BRONZE  Canvas->DrawText(UTHUDOwner->SmallFont, FText::Format(NSLOCTEXT("UTFlagRun", "AttackersMustScoreWin", " must score {BonusType} to win."), Args).ToString(), XOffset, YPos, RenderScale, RenderScale, TextRenderInfo);
9: SILVER
10: GOLD
Time*100 + 4,5,6  Canvas->DrawText(UTHUDOwner->SmallFont, FText::Format(NSLOCTEXT("UTFlagRun", "AttackersMustScoreTime", " must score {BonusType} with at least\n {TimeNeeded} remaining to have a chance."), Args).ToString(), XOffset, YPos, RenderScale, RenderScale, TextRenderInfo);
Time * 100 + 8,9,10  Canvas->DrawText(UTHUDOwner->SmallFont, FText::Format(NSLOCTEXT("UTFlagRun", "AttackersMustScoreTimeWin", " must score {BonusType} with\n at least {TimeNeeded} remaining to win."), Args).ToString(), XOffset, YPos, RenderScale, RenderScale, TextRenderInfo);
*/

UUTFlagRunMessage::UUTFlagRunMessage(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Lifetime = 6.0f;
	MessageArea = FName(TEXT("Announcements"));
	MessageSlot = FName(TEXT("MajorRewardMessage"));

	bIsStatusAnnouncement = true;
	bIsPartiallyUnique = true;
	ScaleInSize = 3.f;
	AnnouncementDelay = 2.f;

	DefendersMustStop = NSLOCTEXT("UTFlagRun", "DefendersMustStop", " must hold on defense to have a chance.");
	DefendersMustHold = NSLOCTEXT("UTFlagRun", "DefendersMustHold", " must hold {SecondaryName} to {BonusType}.");
	AttackersMustScore = NSLOCTEXT("UTFlagRun", "AttackersMustScore", " must score {BonusType} to have a chance.");
	UnhandledCondition = NSLOCTEXT("UTFlagRun", "UnhandledCondition", "UNHANDLED WIN CONDITION");
	AttackersMustScoreWin = NSLOCTEXT("UTFlagRun", "AttackersMustScoreWin", " must score {BonusType} to win.");
	AttackersMustScoreTime = NSLOCTEXT("UTFlagRun", "AttackersMustScoreTime", " must score {BonusType} with over {TimeNeeded}s bonus to have a chance.");
	AttackersMustScoreTimeWin = NSLOCTEXT("UTFlagRun", "AttackersMustScoreTimeWin", " must score {BonusType} with over {TimeNeeded}s bonus to win.");
	AttackersMustScoreTimeOne = NSLOCTEXT("UTFlagRun", "AttackersMustScoreTimeOne", " must score {BonusType} with over {TimeNeeded}");
	AttackersMustScoreChanceTwo = NSLOCTEXT("UTFlagRun", "AttackersMustScoreChanceTwo", "seconds bonus to have a chance.");
	AttackersMustScoreTimeWinTwo = NSLOCTEXT("UTFlagRun", "AttackersMustScoreTimeWinTwo", "seconds bonus to win.");

	RedTeamText = NSLOCTEXT("UTTeamScoreboard", "RedTeam", "RED");
	BlueTeamText = NSLOCTEXT("UTTeamScoreboard", "BlueTeam", "BLUE");
	GoldBonusText = NSLOCTEXT("FlagRun", "GoldBonusText", "\u2605 \u2605 \u2605");
	SilverBonusText = NSLOCTEXT("FlagRun", "SilverBonusText", "\u2605 \u2605");
	BronzeBonusText = NSLOCTEXT("FlagRun", "BronzeBonusText", "\u2605");
}

void UUTFlagRunMessage::GetEmphasisText(FText& PrefixText, FText& EmphasisText, FText& PostfixText, FLinearColor& EmphasisColor, int32 Switch, class APlayerState* RelatedPlayerState_1, class APlayerState* RelatedPlayerState_2, class UObject* OptionalObject) const
{
	PrefixText = FText::GetEmpty();
	AUTTeamInfo* SubjectTeam = Cast<AUTTeamInfo>(OptionalObject);
	EmphasisText = (SubjectTeam && (SubjectTeam->TeamIndex == 0)) ? RedTeamText : BlueTeamText;
	EmphasisColor = (SubjectTeam && (SubjectTeam->TeamIndex == 0)) ? FLinearColor::Red : FLinearColor::Blue;

	FFormatNamedArguments Args;
	FText BonusType = BronzeBonusText;
	FText SecondaryText = (SubjectTeam && (SubjectTeam->TeamIndex == 0)) ? BlueTeamText : RedTeamText;
	int32 TimeNeeded = 0;
	if (Switch >= 100)
	{
		TimeNeeded = Switch / 100;
		Switch = Switch - 100 * TimeNeeded;
	}
	Args.Add("SecondaryName", SecondaryText);
	Args.Add("TimeNeeded", TimeNeeded);
	PostfixText = FText::GetEmpty();
	switch (Switch)
	{
		case 1: PostfixText = DefendersMustStop; break;
		case 2: PostfixText = DefendersMustHold; break;
		case 3: PostfixText = DefendersMustHold; BonusType = SilverBonusText;  break;
		case 4: PostfixText = (TimeNeeded > 0) ? AttackersMustScoreTime : AttackersMustScore; break;
		case 5: PostfixText = (TimeNeeded > 0) ? AttackersMustScoreTime : AttackersMustScore; BonusType = SilverBonusText; break;
		case 6: PostfixText = (TimeNeeded > 0) ? AttackersMustScoreTime : AttackersMustScore; BonusType = GoldBonusText; break;
		case 7: PostfixText = UnhandledCondition; break;
		case 8: PostfixText = (TimeNeeded > 0) ? AttackersMustScoreTimeWin : AttackersMustScoreWin; break;
		case 9: PostfixText = (TimeNeeded > 0) ? AttackersMustScoreTimeWin : AttackersMustScoreWin; BonusType = SilverBonusText; break;
		case 10: PostfixText = (TimeNeeded > 0) ? AttackersMustScoreTimeWin : AttackersMustScoreWin; BonusType = GoldBonusText; break;
	}

	Args.Add("BonusType", BonusType);
	PostfixText = FText::Format(PostfixText, Args);
}

FName UUTFlagRunMessage::GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject, const class APlayerState* RelatedPlayerState_1, const class APlayerState* RelatedPlayerState_2) const
{
	int32 TimeNeeded = 0;
	if (Switch >= 100)
	{
		TimeNeeded = Switch / 100;
		Switch = Switch - 100 * TimeNeeded;
	}
	switch (Switch)
	{
	case 1: return TEXT("DefendersMustDefendForChance"); break;
	case 2: /* DefendersMustHold */; break;
	case 3: /* DefendersMustHold; BonusType = SilverBonusText */;  break;
	case 4: return TEXT("AttackersMustScore1ForChance"); break;
	case 5: return TEXT("AttackersMustScore2ForChance"); break;
	case 6: return TEXT("AttackersMustScore3ForChance"); break;
	case 7: /*UnhandledCondition */; break;
	case 8: return TEXT("AttackersMustScore1ToWin"); break;
	case 9: return TEXT("AttackersMustScore2ToWin"); break;
	case 10: return TEXT("AttackersMustScore3ToWin"); break;
	}
	return NAME_None;
}

FText UUTFlagRunMessage::GetText(int32 Switch, bool bTargetsPlayerState1, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject) const
{
	return BuildEmphasisText(Switch, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);
}

