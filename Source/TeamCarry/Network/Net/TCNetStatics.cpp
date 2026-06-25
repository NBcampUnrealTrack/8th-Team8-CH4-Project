// TCNetStatics.cpp

#include "Network/Net/TCNetStatics.h"
#include "GameFramework/Pawn.h"

DEFINE_LOG_CATEGORY(LogTCNet);

// 액터가 유효하고 서버 권위를 가질 때만 true
bool UTCNetStatics::HasAuthority(const AActor* Actor)
{
	return Actor != nullptr && Actor->HasAuthority();
}

// 폰이 유효하고 로컬에서 조종될 때만 true
bool UTCNetStatics::IsLocallyControlledPawn(const APawn* Pawn)
{
	return Pawn != nullptr && Pawn->IsLocallyControlled();
}

// 싱글플레이(Standalone)가 아니면 네트워크 환경으로 판정
bool UTCNetStatics::IsNetworked(const AActor* Actor)
{
	if (!Actor)
	{
		return false;
	}

	const ENetMode Mode = Actor->GetNetMode();
	return Mode != NM_Standalone;
}

// 디버그·교차검증 로그에 쓰는 역할 문자열
FString UTCNetStatics::NetRoleToString(ENetRole Role)
{
	switch (Role)
	{
	case ROLE_Authority:		return TEXT("Authority");
	case ROLE_AutonomousProxy:	return TEXT("AutonomousProxy");
	case ROLE_SimulatedProxy:	return TEXT("SimulatedProxy");
	default:					return TEXT("None");
	}
}
