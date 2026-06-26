// GrabComponent.h

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GrabComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class TEAMCARRY_API UGrabComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// 생성자
	UGrabComponent();

	// 매 프레임마다 컴포넌트 상태 갱신
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// 현재 화면에 잡힌 최적의 대상
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	AActor* CurrentBestTarget;

	// 캐릭터가 상호작용 키(E)를 눌렀을 때 호출할 함수
	void TryInteract();

	// 캐릭터가 던지기 키(F)를 눌렀을 때 호출할 함수
	void TryThrow();

	// 네트워크 변수 동기화를 위한 필수 함수
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 현재 잡고 있는 액터 반환
	AActor* GetGrabbedActor() const { return GrabbedActor; }

private:
	// 매 프레임 전방을 스캔하여 BestTarget을 찾는 함수
	void ScanBestTarget();
	
protected:
	// 현재 플레이어가 잡고 있는 가구를 기억하는 변수(GrabbedActor)
	// Replicated 키워드를 추가하여 서버의 값을 클라이언트에도 자동 공유
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Interaction")
	AActor* GrabbedActor;

	// 서버에 상호작용-잡기를 요청하는 RPC 함수
	UFUNCTION(Server, Reliable)
	void ServerTryInteract(AActor* TargetActor);

	// 서버에 상호작용-던지기를 요청하는 RPC 함수
	UFUNCTION(Server, Reliable)
	void ServerTryThrow();

};
