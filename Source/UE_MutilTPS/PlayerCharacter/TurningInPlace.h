UENUM(BlueprintType)
enum class ETurningInPlace: uint8
{
	// 武器状态用于决定武器的行为（例如是否可被拾取、是否显示在手中等）
	ETP_TurnLeft UMETA(DisplayName="Turn Left"),
	ETP_TurnRight UMETA(DisplayName="Turn Right"),
	ETP_InPlace UMETA(DisplayName="InPlace"),

	EWS_Max UMETA(DisplayName="DefaultMAX")
};