/*========================================================================
   Copyright (c) Ars Electronica Futurelab, 2025

   AefPharus - GameInstance Subsystem

   Main entry point for the AefPharus plugin.
   Manages multiple tracker instances and loads configuration from INI.

   Usage:
     auto* Pharus = GetGameInstance()->GetSubsystem<UAefPharusSubsystem>();
     auto* FloorTracker = Pharus->GetTrackerInstance("Floor");
  ========================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AefPharusTypes.h"
#include "AefPharusInstance.h"
#include "AefPharusSubsystem.generated.h"

// Forward declaration
class AAefPharusRootOriginActor;

/**
 * Mox Pharus Subsystem
 *
 * GameInstance Subsystem for managing multiple Pharus tracking instances.
 * Automatically initializes from Config/AefConfig.ini on startup.
 *
 * Features:
 * - Multi-instance support (e.g., Floor + Wall tracking)
 * - INI-based configuration
 * - Persistent across level transitions
 * - Blueprint-accessible API
 */
UCLASS()
class AEFPHARUS_API UAefPharusSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	//--------------------------------------------------------------------------------
	// USubsystem Interface
	//--------------------------------------------------------------------------------

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	//--------------------------------------------------------------------------------
	// Instance Management
	//--------------------------------------------------------------------------------

	/**
	 * Create a new tracker instance with the given configuration
	 * @param Config Configuration for the new instance
	 * @param SpawnClass Optional actor class to spawn for tracks (overrides Config.SpawnClass if set)
	 * @return Result struct with success status, error code, and detailed error message
	 */
	UFUNCTION(BlueprintCallable, Category = "AEF|Pharus")
	FAefPharusCreateInstanceResult CreateTrackerInstance(const FAefPharusInstanceConfig& Config,
		UPARAM(meta = (MustImplement = "/Script/AefPharus.AefPharusActorInterface")) TSubclassOf<AActor> SpawnClass = nullptr);

	/**
	 * Create a new tracker instance with the given configuration (Blueprint helper)
	 * Filters spawn class to actors implementing IAefPharusActorInterface
	 * @param InstanceName Name for the new instance
	 * @param BindNIC Network interface IP (e.g., "127.0.0.1")
	 * @param UDPPort UDP port number
	 * @param SpawnClass Actor class to spawn (must implement IAefPharusActorInterface)
	 * @param MappingMode Coordinate mapping mode (Simple for floor, Regions for walls)
	 * @return Result struct with success status, error code, and detailed error message
	 */
	UFUNCTION(BlueprintCallable, Category = "AEF|Pharus")
	FAefPharusCreateInstanceResult CreateTrackerInstanceSimple(
		FName InstanceName,
		FString BindNIC,
		int32 UDPPort,
		UPARAM(meta = (MustImplement = "/Script/AefPharus.AefPharusActorInterface")) TSubclassOf<AActor> SpawnClass,
		EAefPharusMappingMode MappingMode = EAefPharusMappingMode::Simple
	);

	/**
	 * Get a tracker instance by name
	 * @param InstanceName Name of the instance to retrieve
	 * @return Tracker instance, or nullptr if not found
	 */
	UFUNCTION(BlueprintPure, Category = "AEF|Pharus")
	UAefPharusInstance* GetTrackerInstance(FName InstanceName);

	/**
	 * Remove and shutdown a tracker instance
	 * @param InstanceName Name of the instance to remove
	 * @return true if instance was removed
	 */
	UFUNCTION(BlueprintCallable, Category = "AEF|Pharus")
	bool RemoveTrackerInstance(FName InstanceName);

	/**
	 * Get all active tracker instance names
	 */
	UFUNCTION(BlueprintPure, Category = "AEF|Pharus")
	TArray<FName> GetAllInstanceNames() const;

	/**
	 * Get the number of active tracker instances
	 */
	UFUNCTION(BlueprintPure, Category = "AEF|Pharus")
	int32 GetInstanceCount() const;

	/**
	 * Check if an instance with the given name exists
	 */
	UFUNCTION(BlueprintPure, Category = "AEF|Pharus")
	bool HasInstance(FName InstanceName) const;

	/**
	 * Set a custom SpawnClass override for a specific instance before calling StartPharusSystem()
	 * This allows overriding the DefaultSpawnClass from AefConfig.ini at runtime.
	 * Must be called BEFORE StartPharusSystem() to take effect.
	 * @param InstanceName Name of the instance (e.g., "Floor", "Wall")
	 * @param SpawnClass Actor class to spawn (must implement IAefPharusActorInterface)
	 */
	UFUNCTION(BlueprintCallable, Category = "AEF|Pharus")
	void SetSpawnClassOverride(FName InstanceName,
		UPARAM(meta = (MustImplement = "/Script/AefPharus.AefPharusActorInterface")) TSubclassOf<AActor> SpawnClass);

	/**
	 * Start the Pharus system by creating all configured instances
	 * This allows manual control over when Pharus tracking begins
	 * @return true if instances were created successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "AEF|Pharus")
	bool StartPharusSystem();

	/**
	 * Stop the Pharus system by removing all tracker instances
	 * This stops all tracking and destroys all spawned actors
	 * @return true if system was stopped successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "AEF|Pharus")
	bool StopPharusSystem();

	/**
	 * Check if the Pharus system is currently running
	 * @return true if at least one instance is active
	 */
	UFUNCTION(BlueprintPure, Category = "AEF|Pharus")
	bool IsPharusSystemRunning() const;

	/**
	 * Check if a track ID is currently active in the specified instance
	 * Useful for actors to self-validate if their track still exists
	 * @param InstanceName Name of the tracker instance (e.g., "Floor", "Wall")
	 * @param TrackID Track ID to check
	 * @return true if track is active in the specified instance
	 */
	UFUNCTION(BlueprintPure, Category = "AEF|Pharus")
	bool IsTrackActive(FName InstanceName, int32 TrackID) const;

	/**
	* Debug flag for Pharus
	*/
	UFUNCTION(BlueprintPure, Category = "AEF|Pharus")
	bool GetIsPharusDebug();

	UFUNCTION(BlueprintCallable, Category = "AEF|Pharus")
	void SetIsPharusDebug(bool bNewValue);

	/**
	 * Get current Floor settings directly from disk (Config/AefConfig.ini)
	 * NOTE: Reads from file system, not from memory. Use this to check config file state.
	 * @param InstanceName Name of the instance to read settings for (e.g., "Floor")
	 * @param OutConfig Configuration structure with all floor settings
	 * @return true if settings were successfully read from disk
	 */
	UFUNCTION(BlueprintCallable, Category = "AEF|Pharus|Floor")
	bool GetFloorSettingsFromDisk(FName InstanceName, FAefPharusInstanceConfig& OutConfig) const;

	/**
	 * Get current Wall settings directly from disk (Config/AefConfig.ini)
	 * NOTE: Reads from file system, not from memory. Use this to check config file state.
	 * @param InstanceName Name of the instance to read settings for (e.g., "Wall")
	 * @param OutConfig Configuration structure with all wall settings
	 * @return true if settings were successfully read from disk
	 */
	UFUNCTION(BlueprintCallable, Category = "AEF|Pharus|Wall")
	bool GetWallSettingsFromDisk(FName InstanceName, FAefPharusInstanceConfig& OutConfig) const;

	//--------------------------------------------------------------------------------
	// Global Root Origin Management
	//--------------------------------------------------------------------------------

	/**
	 * Set the global root origin point for all tracking calculations
	 * This is the manual way to set the origin (alternative to placing a RootOriginActor)
	 * Use this for static origins or when origin is controlled programmatically
	 * @param Origin The origin point in world space (cm)
	 */
	UFUNCTION(BlueprintCallable, Category = "AEF|Pharus|Origin")
	void SetRootOrigin(FVector Origin);

	/**
	 * Get the current global root origin point
	 * Returns actor position if UsePharusRootOriginActor=true and actor exists,
	 * otherwise returns the manually set RootOrigin from config or SetRootOrigin()
	 * @return Current origin in world space (cm)
	 */
	UFUNCTION(BlueprintPure, Category = "AEF|Pharus|Origin")
	FVector GetRootOrigin() const;

	/**
	 * Get the current global root origin rotation
	 * Returns actor rotation if UsePharusRootOriginActor=true and actor exists,
	 * otherwise returns the manually set RootRotation from config or SetRootOriginRotation()
	 * @return Current rotation in world space
	 */
	UFUNCTION(BlueprintPure, Category = "AEF|Pharus|Origin")
	FRotator GetRootOriginRotation() const;

	/**
	 * Set the global root origin rotation for all tracking calculations
	 * Use this for static rotations or when origin is controlled programmatically
	 * @param Rotation The rotation in world space
	 */
	UFUNCTION(BlueprintCallable, Category = "AEF|Pharus|Origin")
	void SetRootOriginRotation(FRotator Rotation);

	/**
	 * Check if a valid root origin is configured
	 * @return true if RootOriginActor is valid OR RootOrigin was configured
	 */
	UFUNCTION(BlueprintPure, Category = "AEF|Pharus|Origin")
	bool HasValidRootOrigin() const;

	/**
	 * Check if using a placed RootOriginActor for dynamic origin
	 * @return true if UsePharusRootOriginActor=true in config
	 */
	UFUNCTION(BlueprintPure, Category = "AEF|Pharus|Origin")
	bool IsUsingRootOriginActor() const;

	/**
	 * Register a PharusRootOriginActor as the dynamic origin source
	 * Called automatically by AAefPharusRootOriginActor::BeginPlay()
	 * Only ONE actor can be registered - duplicates are rejected with error log
	 * @param RootActor The actor to use as origin source
	 */
	void RegisterRootOriginActor(AAefPharusRootOriginActor* RootActor);

	/**
	 * Unregister the PharusRootOriginActor
	 * Called automatically by AAefPharusRootOriginActor::EndPlay()
	 * @param RootActor The actor to unregister
	 */
	void UnregisterRootOriginActor(AAefPharusRootOriginActor* RootActor);

	/**
	 * Get the registered RootOriginActor (if any)
	 * @return The registered actor, or nullptr if not using dynamic origin
	 */
	UFUNCTION(BlueprintPure, Category = "AEF|Pharus|Origin")
	AAefPharusRootOriginActor* GetRootOriginActor() const;

	/**
	 * Check if relative spawning is active
	 * @return true if UseRelativeSpawning=true AND UsePharusRootOriginActor=true AND actor is valid
	 */
	UFUNCTION(BlueprintPure, Category = "AEF|Pharus|Origin")
	bool IsRelativeSpawningActive() const;

private:
	// Debugging
	bool bIsPharusDebug;

	/** Flag to control if system should auto-start on initialization */
	bool bAutoStartSystem;


	//--------------------------------------------------------------------------------
	// Internal State
	//--------------------------------------------------------------------------------

	/** Active tracker instances */
	UPROPERTY()
	TMap<FName, UAefPharusInstance*> TrackerInstances;

	/** SpawnClass overrides set via SetSpawnClassOverride() before StartPharusSystem() */
	TMap<FName, TSubclassOf<AActor>> SpawnClassOverrides;

	/** Timer handle for delayed auto-initialization */
	FTimerHandle DelayedInitTimerHandle;

	/** Flag to track if delayed init has been executed */
	bool bDelayedInitExecuted = false;

	//--------------------------------------------------------------------------------
	// Root Origin State
	//--------------------------------------------------------------------------------

	/** Whether to use a placed AefPharusRootOriginActor for dynamic origin (from config) */
	bool bUsePharusRootOriginActor = false;

	/** Whether actors should be attached as children to RootOriginActor (from config) */
	bool bUseRelativeSpawning = false;

	/** Static root origin set via SetRootOrigin() or from config GlobalOrigin */
	UPROPERTY()
	FVector RootOrigin = FVector::ZeroVector;

	/** Static root rotation set via SetRootOriginRotation() or from config GlobalRotation */
	UPROPERTY()
	FRotator RootRotation = FRotator::ZeroRotator;

	/** Reference to placed root origin actor (if using dynamic origin) */
	UPROPERTY()
	TWeakObjectPtr<AAefPharusRootOriginActor> PharusRootOriginActor;

	//--------------------------------------------------------------------------------
	// Configuration Loading
	//--------------------------------------------------------------------------------

	/**
	 * Load configuration from Config/AefConfig.ini
	 */
	void LoadConfigurationFromIni();

	/**
	 * Create instances based on [PharusSubsystem] AutoCreateInstances setting
	 */
	void CreateInstancesFromConfig();

	/**
	 * Parse instance configuration from INI section
	 * @param SectionName INI section name (e.g., "Pharus.Floor")
	 * @return Parsed configuration
	 */
	FAefPharusInstanceConfig ParseInstanceConfigFromIni(const FString& SectionName);

	/**
	 * Parse wall region configurations from INI
	 * @param BaseSectionName Base section name (e.g., "Pharus.Wall")
	 * @return Array of parsed wall regions
	 */
	TArray<FAefPharusWallRegion> ParseWallRegionsFromIni(const FString& BaseSectionName) const;

	/**
	 * Parse a single wall region from INI
	 * @param SectionName Section name (e.g., "Pharus.Wall")
	 * @param WallName Wall name (e.g., "Front", "Left", "Back", "Right")
	 * @return Parsed wall region
	 */
	FAefPharusWallRegion ParseWallRegionFromIni(const FString& SectionName, const FString& WallName) const;

	//--------------------------------------------------------------------------------
	// Helper Functions
	//--------------------------------------------------------------------------------

	/**
	 * Get the config file path
	 */
	FString GetConfigFilePath() const;
};

