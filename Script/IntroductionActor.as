class AIntroductionActor : AActor
{
    UPROPERTY(DefaultComponent, RootComponent)
    USceneComponent SceneRoot;

    UPROPERTY(DefaultComponent, Attach = SceneRoot)
    UStaticMeshComponent Mesh;

    UPROPERTY(DefaultComponent, Attach = SceneRoot)
    UBillboardComponent Billboard;

    UPROPERTY()
    float CountdownDuration = 5.0;

    float CurrentTimer = 0.0;
    bool bCountdownActive = false;

    UFUNCTION(BlueprintOverride)
    void BeginPlay()
    {
        // We no longer start the countdown on BeginPlay automatically
    }

    UFUNCTION()
    void StartCountdown()
    {
        // Start the countdown when StartCountdown() is called 
        CurrentTimer = CountdownDuration;
        bCountdownActive = true;
    }

    /**
     * Declare an overridable event so the actor blueprint can
     * respond when the countdown finishes
     */
    UFUNCTION(BlueprintEvent)
    void FinishedCountdown() {}

    UFUNCTION(BlueprintOverride)
    void Tick(float DeltaSeconds)
    {
        auto location = this.GetActorTransform().GetLocation();
        // Print("Actor location: " + location);
        if (bCountdownActive)
        {
            // Count down the timer 
            CurrentTimer -= DeltaSeconds;

            if (CurrentTimer <= 0.0)
            {
                // The countdown was complete!
                // Print a message to the screen
                Print("Countdown was completed!");
                // Also: trigger a blueprint event
                FinishedCountdown();
                bCountdownActive = false;
            }
        }
    }
}