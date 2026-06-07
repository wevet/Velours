// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"
#include "Modules/ModuleManager.h"

// using
// *GETENUMSTRING("/Script/ProjectName.EnumName")
#define GETENUMSTRING(etype, evalue)\
	 ((FindObject<UEnum>(nullptr, TEXT(etype), EFindObjectFlags::ExactClass) != nullptr) ? FindObject<UEnum>(nullptr, TEXT(etype), EFindObjectFlags::ExactClass)->GetNameStringByIndex((int32)evalue) : FString("Invalid - UENUM() macro?"))

#define WEVET_COMMENT(Comment)

// Input
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Game_Input_Disable);

// PlayerAction
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Character_Player_Melee);

// Is AI allowed to attack Player?
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Character_AI_NotAllowed_Attack);

// QTE Command Enable
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Character_Action_QTE);
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Character_Action_QTE_Pressed);

// Combo frag tag
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Character_ActionMelee_ComboRequire);

// Attack frag tag
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Character_ActionMelee_Forbid);

// look at target
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Character_ActionLookAt);

// Melee Action 
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Character_ActionMelee);
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Character_ActionMelee_Hold);
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Character_ActionMelee_Combo1);
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Character_ActionMelee_Combo2);
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Character_ActionMelee_Combo3);

// Knife Action
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Character_ActionKnife);
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Character_ActionKnife_Hold);
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Character_ActionKnife_Combo1);
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Character_ActionKnife_Combo2);
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Character_ActionKnife_Combo3);

// Combat Chain
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Character_ActionCombatChain);
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Character_ActionCombatChain_Trigger);

// Rotation Mode Change
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Character_ActionStrafeChange);
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Character_ActionAimChange);

// JumpAction
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Character_ActionJump);
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Character_ActionJump_Forbid);

// Dash
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Character_ActionDash);
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Character_ActionDash_Forbid);

// Walk
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Character_ActionWalk);
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Character_ActionWalk_Forbid);

// Crouch
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Character_ActionCrouch);
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Character_ActionCrouch_Forbid);

// Roll
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Character_ActionRoll);
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Character_ActionRoll_Forbid);

// Drive
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Character_ActionDrive);
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Character_ActionDrive_Forbid);

// if reload active tag added character state tag
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Character_Action_GunReload);

// TargetLockOn
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Character_TargetLock);
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Character_TargetLocking);
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Character_TargetLock_Forbid);

// cinematic
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Character_Action_Cinematic);


// Climbing
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Locomotion_ClimbingLedgeEnd);

// Mantling
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Locomotion_Mantling);

// Vault Hurdle Mantle
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Locomotion_Traversal);

// Forbid
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Locomotion_ForbidClimbing);
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Locomotion_ForbidMantling);
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Locomotion_ForbidMovement);
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Locomotion_ForbidJump);
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Locomotion_ForbidRagdoll);
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Locomotion_ForbidTraversal);


// vehicle
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Vehicle_Drive); // drive action start
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Vehicle_UnDrive); // drive actoin end
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Vehicle_State_Drive); // current is driving

// smart object difinition
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Character_State_SmartObject_Using);

// async load data asset tag
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Game_Asset_HitReaction);
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Game_Asset_FinisherSender);
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Game_Asset_FinisherReceiver);
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Game_Asset_CloseCombat);
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Game_Asset_CharacterVFX);


// minimap
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Game_Minimap_Player);
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Game_Minimap_KeyCharacter);
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Game_Minimap_EventCheckPoint);

#pragma region Faction

VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Faction_Neutral);
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Faction_Player);
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Faction_Player_Companion);

VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Faction_Civilian);
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Faction_Police);

VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Faction_Gang);

VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Faction_Vehicle);
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Faction_Vehicle_Civilian);
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Faction_Vehicle_Police);

VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Reputation_Honor);
VELOURS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Reputation_Wanted);

#pragma endregion


/*
* AIPerception Sight CancelEvent Interval
*/
#define SIGHT_AGE 20.0f

/*
* AIPerception Hear CancelEvent Interval
*/
#define HEAR_AGE 30.0f

/*
* AIPerception Follow CancelEvent Interval
*/
#define FOLLOW_AGE -1.0

/*
* AIPerception Friend CancelEvent Interval
*/
#define FRIEND_AGE 30.0


#define NEARLEST_TARGET_SYNC_POINT FName(TEXT("NearlestTarget"))
#define AI_NEARLEST_TARGET_SYNC_POINT FName(TEXT("AI_NearlestTarget"))
#define FINISHER_TARGET_SYNC_POINT FName(TEXT("FinisherTarget"))

/*
* project custom collision preset
*/
#define K_CHARACTER_COLLISION_PRESET FName(TEXT("BaseCharacter"))

/*
* actor and component add tagname
*/
#define K_QTE_COMPONENT_TAG FName(TEXT("QTE"))


#define FPS_60 1.0/60

#define K_LOCK_ON_WIDGET_TAG FName("TargetSystem.LockOnWidget")

#define QTE_SYSTEM_RECEIVE 1

#define K_MOVING_THRESHOLD 3.0f
#define K_MOVING_ACC_THRESHOLD 1.0f

#define K_WALKABLE_FLOOR_ANGLE 50.0f

// character dead reason
#define K_REASON_DEAD TEXT("Character Dead")
#define K_REASON_FREEZE TEXT("Character Freeze")
#define K_REASON_UNFREEZE TEXT("Character UnFreeze")
#define K_REASON_SYSTEM_HIDDEN TEXT("System Hidden")

#define K_FOLLOW_NUM 5


#define K_SIGNIGICANCE_ACTOR FName(TEXT("Significance"))



class FVeloursModule : public IModuleInterface
{
public:
	static FVeloursModule* GameModuleInstance;

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

protected:
};


