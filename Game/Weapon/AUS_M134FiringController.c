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
    
    //------------------------------------------------------------------------------------------------
    override event protected void OnPostInit(IEntity owner)
    {
        super.OnPostInit(owner);
        
        // Find required components
        m_SignalsManager = SignalsManagerComponent.Cast(owner.FindComponent(SignalsManagerComponent));
        m_Muzzle = BaseMuzzleComponent.Cast(owner.FindComponent(BaseMuzzleComponent));
        m_AnimationComponent = AUS_WeaponAnimationComponent.Cast(owner.FindComponent(AUS_WeaponAnimationComponent));
        m_WeaponComponent = BaseWeaponComponent.Cast(owner.FindComponent(BaseWeaponComponent));
        
        if (!m_SignalsManager || !m_Muzzle)
            return;
        
        InitializeSignals();
        InitializeAnimationVariables();
        
        // Set initial state
        m_iPreviousAmmoCount = m_Muzzle.GetAmmoCount();
        m_fLastFireTime = System.GetTickCount();
        m_fLastStateChangeTime = System.GetTickCount();
        
        // Start with weapon firing blocked
        m_bWeaponFiringBlocked = true;
        
        Print("[AUS_MinigunBarrelController] Component initialized successfully", LogLevel.NORMAL);
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
    }
    
    //------------------------------------------------------------------------------------------------
    private void InitializeAnimationVariables()
    {
        if (!m_AnimationComponent)
            return;
            
        m_iBarrelSpinSpeedVar = m_AnimationComponent.BindVariableFloat("BarrelSpinSpeed");
        m_iSpinStateVar = m_AnimationComponent.BindVariableInt("SpinState");
        m_iFiringVar = m_AnimationComponent.BindVariableBool("Firing");
        m_iVehicleFireReleasedVar = m_AnimationComponent.BindVariableBool("VehicleFireReleased");
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
        bool inDeadZone = (currentTime - m_fLastStateChangeTime) < m_fTriggerDeadZone;
        
        switch (m_eCurrentState)
        {
            case AUS_BarrelSpinState.IDLE:
                if (isFiring)
                {
                    StartSpinUp(currentTime);
                }
                break;
                
            case AUS_BarrelSpinState.SPIN_UP:
                if (m_fStateTimer >= m_fSpinUpTime)
                {
                    m_bSpinUpComplete = true;
                }
                
                if (m_bSpinUpComplete)
                {
                    if (isFiring)
                    {
                        EnterReadyToFireState(currentTime);
                    }
                    else if (m_bForceSpinDown)
                    {
                        StartSpinDown(currentTime);
                    }
                    else if (!isFiring)
                    {
                        StartSpinDown(currentTime);
                    }
                }
                else if (!isFiring && !inDeadZone)
                {
                    m_bForceSpinDown = true;
                }
                break;
                
            case AUS_BarrelSpinState.READY_TO_FIRE:
                if (m_fStateTimer >= m_fFiringDelay)
                {
                    if (isFiring)
                    {
                        EnterFiringState(currentTime);
                    }
                    else
                    {
                        StartSpinDown(currentTime);
                    }
                }
                else if (!isFiring && !inDeadZone)
                {
                    StartSpinDown(currentTime);
                }
                break;
                
            case AUS_BarrelSpinState.FIRING:
                if (!isFiring)
                {
                    StartSpinDown(currentTime);
                }
                break;
                
            case AUS_BarrelSpinState.SPIN_DOWN:
                if (m_fStateTimer >= m_fSpinDownTime)
                {
                    ReturnToIdle(currentTime);
                }
                else if (isFiring && !inDeadZone)
                {
                    StartSpinUp(currentTime);
                }
                break;
        }
        
        m_fStateTimer += timeSlice * 1000.0;
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
    }
    
    //------------------------------------------------------------------------------------------------
    private void EnterReadyToFireState(float currentTime)
    {
        m_eCurrentState = AUS_BarrelSpinState.READY_TO_FIRE;
        m_fStateTimer = 0.0;
        m_fLastStateChangeTime = currentTime;
        m_bSpinUpComplete = false;
        m_bForceSpinDown = false;
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
    }
    
    //------------------------------------------------------------------------------------------------
    private void StartSpinDown(float currentTime)
    {
        m_eCurrentState = AUS_BarrelSpinState.SPIN_DOWN;
        m_fStateTimer = 0.0;
        m_fLastStateChangeTime = currentTime;
        m_bSpinUpComplete = false;
        m_bForceSpinDown = false;
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
            }
            else
            {
                m_bWeaponFiringBlocked = true;
            }
        }
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