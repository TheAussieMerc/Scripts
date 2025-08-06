class AUS_MinigunBarrelControllerClass: ScriptGameComponentClass {}

enum AUS_BarrelSpinState
{
    IDLE = 0,
    SPIN_UP = 1,
    READY_TO_FIRE = 2,
    FIRING = 3,
    SPIN_DOWN = 4
}

class AUS_MinigunBarrelController : ScriptGameComponent
{
    // Configuration Attributes
    [Attribute("680", UIWidgets.EditBox, "Spin-up duration (ms)")]
    protected float m_fSpinUpTime;
    
    [Attribute("2151", UIWidgets.EditBox, "Spin-down duration (ms)")]
    protected float m_fSpinDownTime;
    
    [Attribute("150", UIWidgets.EditBox, "Firing delay after spin-up (ms)")]
    protected float m_fFiringDelay;
    
    [Attribute("1750", UIWidgets.EditBox, "Maximum RPM")]
    protected float m_fMaxRPM;
    
    [Attribute("150", UIWidgets.EditBox, "Fire detection tolerance (ms)")]
    protected float m_fFireDetectionTolerance;
    
    [Attribute("500", UIWidgets.EditBox, "Trigger spam dead-zone (ms)")]
    protected float m_fTriggerDeadZone;
    
    [Attribute("2.0", UIWidgets.EditBox, "Spin-up curve exponent")]
    protected float m_fSpinUpCurve;
    
    [Attribute("1.5", UIWidgets.EditBox, "Spin-down curve exponent")]
    protected float m_fSpinDownCurve;
    
    // Component References
    private SignalsManagerComponent m_SignalsManager;
    private BaseMuzzleComponent m_Muzzle;
    private AUS_WeaponAnimationComponent m_AnimationComponent;
    private BaseWeaponComponent m_WeaponComponent;
    
    // State Management
    private AUS_BarrelSpinState m_eCurrentState = AUS_BarrelSpinState.IDLE;
    private float m_fStateTimer = 0.0;
    private float m_fCurrentSpinSpeed = 0.0;
    private float m_fLastFireTime = -1000.0;
    private float m_fLastStateChangeTime = -1000.0;
    private int m_iPreviousAmmoCount = 0;
    private bool m_bWasFiring = false;
    private bool m_bReloadLocked = false;
    
    // Simplified State Gating Variables
    private bool m_bForceSpinDown = false;
    private bool m_bSpinUpComplete = false;
    
    // Weapon Firing Control
    private bool m_bWeaponFiringBlocked = false;
    
    // Signal IDs
    private int m_iBarrelSpinSignal = -1;
    private int m_iSpinUpActiveSignal = -1;
    private int m_iSpinDownActiveSignal = -1;
    private int m_iFiringActiveSignal = -1;
    
    // Animation Variable IDs
    private int m_iBarrelSpinSpeedVar = -1;
    private int m_iSpinStateVar = -1;
    private int m_iFiringVar = -1;
    private int m_iVehicleFireReleasedVar = -1;
    
    // Debug
    private float m_fLastDebugTime = -1000.0;
    private float m_fInitTime = -1000.0;
    private static const float DEBUG_INTERVAL = 200.0;
    private static const float INIT_DEBUG_DELAY = 2000.0;
    
    //------------------------------------------------------------------------------------------------
    override event protected void OnPostInit(IEntity owner)
    {
        super.OnPostInit(owner);
        
        Print("[AUS_MinigunBarrelController] *** WEAPON FIRING CONTROL VERSION ***", LogLevel.NORMAL);
        Print("[AUS_MinigunBarrelController] Entity: " + owner.GetName(), LogLevel.NORMAL);
        
        // Find required components
        m_SignalsManager = SignalsManagerComponent.Cast(owner.FindComponent(SignalsManagerComponent));
        m_Muzzle = BaseMuzzleComponent.Cast(owner.FindComponent(BaseMuzzleComponent));
        m_AnimationComponent = AUS_WeaponAnimationComponent.Cast(owner.FindComponent(AUS_WeaponAnimationComponent));
        m_WeaponComponent = BaseWeaponComponent.Cast(owner.FindComponent(BaseWeaponComponent));
        
        Print("[AUS_MinigunBarrelController] SignalsManager: " + (m_SignalsManager != null), LogLevel.NORMAL);
        Print("[AUS_MinigunBarrelController] Muzzle: " + (m_Muzzle != null), LogLevel.NORMAL);
        Print("[AUS_MinigunBarrelController] AnimationComponent: " + (m_AnimationComponent != null), LogLevel.NORMAL);
        Print("[AUS_MinigunBarrelController] WeaponComponent: " + (m_WeaponComponent != null), LogLevel.NORMAL);
        
        if (!m_SignalsManager)
        {
            Print("[AUS_MinigunBarrelController] ERROR: SignalsManagerComponent not found!", LogLevel.ERROR);
            return;
        }
        
        if (!m_Muzzle)
        {
            Print("[AUS_MinigunBarrelController] ERROR: BaseMuzzleComponent not found!", LogLevel.ERROR);
            return;
        }
        
        InitializeSignals();
        InitializeAnimationVariables();
        
        // Set initial state
        m_iPreviousAmmoCount = m_Muzzle.GetAmmoCount();
        m_fLastFireTime = System.GetTickCount();
        m_fLastStateChangeTime = System.GetTickCount();
        m_fInitTime = System.GetTickCount();
        
        // Start with weapon firing blocked
        m_bWeaponFiringBlocked = true;
        
        SetEventMask(owner, EntityEvent.FRAME);
    }
    
    //------------------------------------------------------------------------------------------------
    private void InitializeSignals()
    {
        if (!m_SignalsManager)
            return;
            
        m_iBarrelSpinSignal = m_SignalsManager.AddOrFindMPSignal("AUS_BarrelSpin", 0.1, 1.0/30.0, 0, SignalCompressionFunc.Range01);
        m_iSpinUpActiveSignal = m_SignalsManager.AddOrFindMPSignal("AUS_SpinUpActive", 0.1, 1.0/30.0, 0, SignalCompressionFunc.Range01);
        m_iSpinDownActiveSignal = m_SignalsManager.AddOrFindMPSignal("AUS_SpinDownActive", 0.1, 1.0/30.0, 0, SignalCompressionFunc.Range01);
        m_iFiringActiveSignal = m_SignalsManager.AddOrFindMPSignal("AUS_FiringActive", 0.1, 1.0/30.0, 0, SignalCompressionFunc.Range01);
        
        DebugPrint("Audio signals initialized");
        GetGame().GetCallqueue().CallLater(DelayedInitMessage, INIT_DEBUG_DELAY, false);
    }
    
    //------------------------------------------------------------------------------------------------
    private void InitializeAnimationVariables()
    {
        if (!m_AnimationComponent)
        {
            DebugPrint("AUS_WeaponAnimationComponent not found - animation integration disabled");
            return;
        }
            
        m_iBarrelSpinSpeedVar = m_AnimationComponent.BindVariableFloat("BarrelSpinSpeed");
        m_iSpinStateVar = m_AnimationComponent.BindVariableInt("SpinState");
        m_iFiringVar = m_AnimationComponent.BindVariableBool("Firing");
        m_iVehicleFireReleasedVar = m_AnimationComponent.BindVariableBool("VehicleFireReleased");
        
        DebugPrint("Animation variables bound successfully");
        GetGame().GetCallqueue().CallLater(DelayedAnimationMessage, INIT_DEBUG_DELAY + 100, false);
    }
    
    //------------------------------------------------------------------------------------------------
    override event protected bool OnTicksOnRemoteProxy()
    {
        return false;
    }
    
    //------------------------------------------------------------------------------------------------
    override event protected void EOnFrame(IEntity owner, float timeSlice)
    {
        super.EOnFrame(owner, timeSlice);
        
        if (!m_Muzzle || !m_SignalsManager)
            return;
            
        float currentTime = System.GetTickCount();
        
        // Detect firing state based on barrel state, not ammo consumption
        bool isFiring = DetectFiringIntent(currentTime);
        
        // Update state machine with firing delay
        UpdateStateMachineWithFiringDelay(isFiring, currentTime, timeSlice);
        
        // Calculate current spin speed
        CalculateSpinSpeed(timeSlice);
        
        // Update outputs
        UpdateOutputs();
        
        // Handle reload and weapon firing control
        UpdateReloadLock();
        UpdateWeaponFiringControl();
    }
    
    //------------------------------------------------------------------------------------------------
    private bool DetectFiringIntent(float currentTime)
    {
        // For now, use the same ammo-based detection
        // Later we can modify this to use trigger input instead
        int currentAmmoCount = m_Muzzle.GetAmmoCount();
        int deltaAmmo = currentAmmoCount - m_iPreviousAmmoCount;
        
        bool firingDetected = deltaAmmo < 0;
        
        if (firingDetected)
        {
            m_fLastFireTime = currentTime;
        }
        
        bool isFiring = firingDetected || (currentTime - m_fLastFireTime) < m_fFireDetectionTolerance;
        
        m_iPreviousAmmoCount = currentAmmoCount;
        
        return isFiring;
    }
    
    //------------------------------------------------------------------------------------------------
    private void UpdateStateMachineWithFiringDelay(bool isFiring, float currentTime, float timeSlice)
    {
        bool stateChanged = false;
        AUS_BarrelSpinState previousState = m_eCurrentState;
        
        bool inDeadZone = (currentTime - m_fLastStateChangeTime) < m_fTriggerDeadZone;
        
        switch (m_eCurrentState)
        {
            case AUS_BarrelSpinState.IDLE:
                if (isFiring)
                {
                    StartSpinUp(currentTime);
                    stateChanged = true;
                }
                break;
                
            case AUS_BarrelSpinState.SPIN_UP:
                if (m_fStateTimer >= m_fSpinUpTime)
                {
                    m_bSpinUpComplete = true;
                    DebugPrintImmediate("SPIN_UP completed at " + m_fStateTimer.ToString() + "ms");
                }
                
                if (m_bSpinUpComplete)
                {
                    if (isFiring)
                    {
                        EnterReadyToFireState(currentTime);
                        stateChanged = true;
                    }
                    else if (m_bForceSpinDown)
                    {
                        StartSpinDown(currentTime);
                        stateChanged = true;
                        DebugPrintImmediate("FORCED transition to SPIN_DOWN");
                    }
                    else if (!isFiring)
                    {
                        StartSpinDown(currentTime);
                        stateChanged = true;
                    }
                }
                else if (!isFiring && !inDeadZone)
                {
                    m_bForceSpinDown = true;
                    DebugPrintImmediate("Will FORCE SPIN_DOWN when spin-up completes");
                }
                break;
                
            case AUS_BarrelSpinState.READY_TO_FIRE:
                if (m_fStateTimer >= m_fFiringDelay)
                {
                    if (isFiring)
                    {
                        float actualDelay = m_fStateTimer;
                        EnterFiringState(currentTime);
                        stateChanged = true;
                        DebugPrintImmediate("Firing delay completed after " + actualDelay.ToString() + "ms");
                    }
                    else
                    {
                        StartSpinDown(currentTime);
                        stateChanged = true;
                    }
                }
                else if (!isFiring && !inDeadZone)
                {
                    StartSpinDown(currentTime);
                    stateChanged = true;
                    DebugPrintImmediate("Trigger released during firing delay at " + m_fStateTimer.ToString() + "ms");
                }
                else
                {
                    static float lastDelayDebug = 0;
                    if (m_fStateTimer - lastDelayDebug > 50.0)
                    {
                        DebugPrintImmediate("Firing delay progress: " + m_fStateTimer.ToString() + "ms / " + m_fFiringDelay.ToString() + "ms");
                        lastDelayDebug = m_fStateTimer;
                    }
                }
                break;
                
            case AUS_BarrelSpinState.FIRING:
                if (!isFiring)
                {
                    StartSpinDown(currentTime);
                    stateChanged = true;
                }
                break;
                
            case AUS_BarrelSpinState.SPIN_DOWN:
                if (m_fStateTimer >= m_fSpinDownTime)
                {
                    ReturnToIdle(currentTime);
                    stateChanged = true;
                }
                else if (isFiring && !inDeadZone)
                {
                    StartSpinUp(currentTime);
                    stateChanged = true;
                }
                break;
        }
        
        m_fStateTimer += timeSlice * 1000.0;
        
        if (stateChanged)
        {
            DebugPrintImmediate("State changed: " + typename.EnumToString(AUS_BarrelSpinState, previousState) + 
                      " -> " + typename.EnumToString(AUS_BarrelSpinState, m_eCurrentState));
        }
    }
    
    //------------------------------------------------------------------------------------------------
    private void StartSpinUp(float currentTime)
    {
        m_eCurrentState = AUS_BarrelSpinState.SPIN_UP;
        m_fStateTimer = 0.0;
        m_fLastStateChangeTime = currentTime;
        m_bWasFiring = false;
        m_bSpinUpComplete = false;
        m_bForceSpinDown = false;
        
        DebugPrintImmediate("Starting SPIN_UP - Audio should play once");
    }
    
    //------------------------------------------------------------------------------------------------
    private void EnterReadyToFireState(float currentTime)
    {
        m_eCurrentState = AUS_BarrelSpinState.READY_TO_FIRE;
        m_fStateTimer = 0.0;
        m_fLastStateChangeTime = currentTime;
        m_bSpinUpComplete = false;
        m_bForceSpinDown = false;
        
        DebugPrintImmediate("Entering READY_TO_FIRE - Waiting for firing delay");
    }
    
    //------------------------------------------------------------------------------------------------
    private void EnterFiringState(float currentTime)
    {
        m_eCurrentState = AUS_BarrelSpinState.FIRING;
        m_fStateTimer = 0.0;
        m_fLastStateChangeTime = currentTime;
        m_bWasFiring = true;
        m_bSpinUpComplete = false;
        m_bForceSpinDown = false;
        
        DebugPrintImmediate("Entering FIRING state - Loop should start NOW");
    }
    
    //------------------------------------------------------------------------------------------------
    private void StartSpinDown(float currentTime)
    {
        m_eCurrentState = AUS_BarrelSpinState.SPIN_DOWN;
        m_fStateTimer = 0.0;
        m_fLastStateChangeTime = currentTime;
        m_bSpinUpComplete = false;
        m_bForceSpinDown = false;
        
        DebugPrintImmediate("Starting SPIN_DOWN - Audio should play once");
    }
    
    //------------------------------------------------------------------------------------------------
    private void ReturnToIdle(float currentTime)
    {
        m_eCurrentState = AUS_BarrelSpinState.IDLE;
        m_fStateTimer = 0.0;
        m_fLastStateChangeTime = currentTime;
        m_fCurrentSpinSpeed = 0.0;
        m_bWasFiring = false;
        m_bSpinUpComplete = false;
        m_bForceSpinDown = false;
        
        DebugPrintImmediate("Returned to IDLE");
    }
    
    //------------------------------------------------------------------------------------------------
    private void CalculateSpinSpeed(float timeSlice)
    {
        switch (m_eCurrentState)
        {
            case AUS_BarrelSpinState.IDLE:
                m_fCurrentSpinSpeed = 0.0;
                break;
                
            case AUS_BarrelSpinState.SPIN_UP:
                {
                    float progress = Math.Clamp(m_fStateTimer / m_fSpinUpTime, 0.0, 1.0);
                    m_fCurrentSpinSpeed = Math.Pow(progress, m_fSpinUpCurve);
                }
                break;
                
            case AUS_BarrelSpinState.READY_TO_FIRE:
                m_fCurrentSpinSpeed = 1.0;
                break;
                
            case AUS_BarrelSpinState.FIRING:
                m_fCurrentSpinSpeed = 1.0;
                break;
                
            case AUS_BarrelSpinState.SPIN_DOWN:
                {
                    float progress = Math.Clamp(m_fStateTimer / m_fSpinDownTime, 0.0, 1.0);
                    m_fCurrentSpinSpeed = Math.Pow(1.0 - progress, m_fSpinDownCurve);
                }
                break;
        }
    }
    
    //------------------------------------------------------------------------------------------------
    private void UpdateOutputs()
    {
        if (!m_SignalsManager)
            return;
            
        m_SignalsManager.SetSignalValue(m_iBarrelSpinSignal, m_fCurrentSpinSpeed);
        
        int spinUpActive = 0;
        int firingActive = 0;
        int spinDownActive = 0;
        
        if (m_eCurrentState == AUS_BarrelSpinState.SPIN_UP)
            spinUpActive = 1;
        else if (m_eCurrentState == AUS_BarrelSpinState.FIRING)
            firingActive = 1;
        else if (m_eCurrentState == AUS_BarrelSpinState.SPIN_DOWN)
            spinDownActive = 1;
        
        m_SignalsManager.SetSignalValue(m_iSpinUpActiveSignal, spinUpActive);
        m_SignalsManager.SetSignalValue(m_iFiringActiveSignal, firingActive);
        m_SignalsManager.SetSignalValue(m_iSpinDownActiveSignal, spinDownActive);
        
        if (m_AnimationComponent && m_eCurrentState != AUS_BarrelSpinState.IDLE)
        {
            m_AnimationComponent.SetVariableFloat(m_iBarrelSpinSpeedVar, m_fCurrentSpinSpeed);
            m_AnimationComponent.SetVariableInt(m_iSpinStateVar, m_eCurrentState);
            
            bool firingState = false;
            if (m_eCurrentState == AUS_BarrelSpinState.FIRING)
                firingState = true;
            m_AnimationComponent.SetVariableBool(m_iFiringVar, firingState);
            
            bool vehicleFireReleased = false;
            if (m_eCurrentState == AUS_BarrelSpinState.SPIN_DOWN)
                vehicleFireReleased = true;
            m_AnimationComponent.SetVariableBool(m_iVehicleFireReleasedVar, vehicleFireReleased);
        }
    }
    
    //------------------------------------------------------------------------------------------------
    private void UpdateReloadLock()
    {
        bool shouldLockReload = (m_eCurrentState != AUS_BarrelSpinState.IDLE);
        
        if (shouldLockReload != m_bReloadLocked)
        {
            m_bReloadLocked = shouldLockReload;
            
            if (m_bReloadLocked)
            {
                DebugPrint("Reload locked - barrel spinning");
            }
            else
            {
                DebugPrint("Reload unlocked");
            }
        }
    }
    
    //------------------------------------------------------------------------------------------------
    private void UpdateWeaponFiringControl()
    {
        bool shouldAllowFiring = (m_eCurrentState == AUS_BarrelSpinState.FIRING);
        
        if (shouldAllowFiring != !m_bWeaponFiringBlocked)
        {
            if (shouldAllowFiring)
            {
                m_bWeaponFiringBlocked = false;
                DebugPrintImmediate("Weapon firing ENABLED - Ready to fire");
            }
            else
            {
                m_bWeaponFiringBlocked = true;
                DebugPrintImmediate("Weapon firing BLOCKED - Barrels not ready");
            }
        }
    }
    
    //------------------------------------------------------------------------------------------------
    private void DelayedInitMessage()
    {
        Print("[AUS_MinigunBarrelController] === WEAPON FIRING CONTROL INITIALIZATION COMPLETE ===", LogLevel.NORMAL);
        Print("[AUS_MinigunBarrelController] Component ready for testing", LogLevel.NORMAL);
    }
    
    //------------------------------------------------------------------------------------------------
    private void DelayedAnimationMessage()
    {
        if (m_AnimationComponent)
        {
            Print("[AUS_MinigunBarrelController] Animation system: ACTIVE", LogLevel.NORMAL);
        }
        else
        {
            Print("[AUS_MinigunBarrelController] Animation system: DISABLED - Component not found", LogLevel.ERROR);
        }
    }
    
    //------------------------------------------------------------------------------------------------
    private void DebugPrint(string message)
    {
        float currentTime = System.GetTickCount();
        
        if (currentTime - m_fInitTime < INIT_DEBUG_DELAY)
            return;
        
        if (currentTime - m_fLastDebugTime >= DEBUG_INTERVAL)
        {
            Print("[AUS_MinigunBarrelController] " + message + 
      " | State: " + typename.EnumToString(AUS_BarrelSpinState, m_eCurrentState) + 
      " | Speed: " + m_fCurrentSpinSpeed.ToString() + 
      " | Timer: " + m_fStateTimer.ToString(), LogLevel.NORMAL);
            m_fLastDebugTime = currentTime;
        }
    }
    
    //------------------------------------------------------------------------------------------------
    private void DebugPrintImmediate(string message)
    {
        float currentTime = System.GetTickCount();
        
        if (currentTime - m_fInitTime < INIT_DEBUG_DELAY)
            return;
        
        Print("[AUS_MinigunBarrelController] " + message + 
      " | State: " + typename.EnumToString(AUS_BarrelSpinState, m_eCurrentState) + 
      " | Speed: " + m_fCurrentSpinSpeed.ToString() + 
      " | Timer: " + m_fStateTimer.ToString(), LogLevel.NORMAL);
    }
    
    //------------------------------------------------------------------------------------------------
    AUS_BarrelSpinState GetCurrentState()
    {
        return m_eCurrentState;
    }
    
    //------------------------------------------------------------------------------------------------
    float GetCurrentSpinSpeed()
    {
        return m_fCurrentSpinSpeed;
    }
    
    //------------------------------------------------------------------------------------------------
    bool IsReloadLocked()
    {
        return m_bReloadLocked;
    }
    
    //------------------------------------------------------------------------------------------------
    bool CanWeaponFire()
    {
        return !m_bWeaponFiringBlocked && (m_eCurrentState == AUS_BarrelSpinState.FIRING);
    }
    
    //------------------------------------------------------------------------------------------------
    float GetStateProgress()
    {
        if (m_eCurrentState == AUS_BarrelSpinState.SPIN_UP)
        {
            return Math.Clamp(m_fStateTimer / m_fSpinUpTime, 0.0, 1.0);
        }
        else if (m_eCurrentState == AUS_BarrelSpinState.READY_TO_FIRE)
        {
            return Math.Clamp(m_fStateTimer / m_fFiringDelay, 0.0, 1.0);
        }
        else if (m_eCurrentState == AUS_BarrelSpinState.SPIN_DOWN)
        {
            return Math.Clamp(m_fStateTimer / m_fSpinDownTime, 0.0, 1.0);
        }
        else
        {
            return 0.0;
        }
    }
}