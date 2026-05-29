

#include "HitReactProfile.h"

#include "Misc/DataValidation.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HitReactProfile)

#define LOCTEXT_NAMESPACE "HitReactProfile"

#if WITH_EDITOR
EDataValidationResult UHitReactProfile::IsDataValid(class FDataValidationContext& Context) const
#endif
{
	for (int32 i = 1; i < SubsequentImpulseScalars.Num(); i++)
	{
		if (SubsequentImpulseScalars[i].ElapsedTime <= SubsequentImpulseScalars[i - 1].ElapsedTime)
		{
			Context.AddError(FText::Format(
				LOCTEXT("HitReactProfile_SubsequentImpulseScalarsOrderError", "Subsequent impulse scalars must be in ascending order. Index {0} has an elapsed time of {1} which is less than or equal to the previous index's elapsed time of {2}"),
				i, SubsequentImpulseScalars[i].ElapsedTime, SubsequentImpulseScalars[i - 1].ElapsedTime));
			return EDataValidationResult::Invalid;
		}
	}

	if (BlendParams.GetTotalTime() < 0.01f)
	{
		Context.AddError(LOCTEXT("HitReactProfile_TotalTimeZeroError", "Total time must be greater than 0.01"));
		return EDataValidationResult::Invalid;
	}

	return Super::IsDataValid(Context);
}


#undef LOCTEXT_NAMESPACE