class AUS_WeaponAnimationComponentClass: WeaponAnimationComponentClass
{
}

class AUS_WeaponAnimationComponent: WeaponAnimationComponent
{
    // Variable ID storage for efficient access
    private ref map<string, int> m_VariableFloatIds = new map<string, int>();
    private ref map<string, int> m_VariableIntIds = new map<string, int>();
    private ref map<string, int> m_VariableBoolIds = new map<string, int>();
    
    //------------------------------------------------------------------------------------------------
    // Bind a float variable and return its ID for efficient access
    int BindVariableFloat(string variableName)
    {
        if (m_VariableFloatIds.Contains(variableName))
            return m_VariableFloatIds.Get(variableName);
            
        int variableId = BindFloatVariable(variableName);
        if (variableId != -1)
        {
            m_VariableFloatIds.Set(variableName, variableId);
            Print("[AUS_WeaponAnimationComponent] Bound float variable: " + variableName + " (ID: " + variableId + ")", LogLevel.VERBOSE);
            return variableId;
        }
        
        Print("[AUS_WeaponAnimationComponent] ERROR: Failed to bind float variable: " + variableName, LogLevel.ERROR);
        return -1;
    }
    
    //------------------------------------------------------------------------------------------------
    // Bind an int variable and return its ID for efficient access
    int BindVariableInt(string variableName)
    {
        if (m_VariableIntIds.Contains(variableName))
            return m_VariableIntIds.Get(variableName);
            
        int variableId = BindIntVariable(variableName);
        if (variableId != -1)
        {
            m_VariableIntIds.Set(variableName, variableId);
            Print("[AUS_WeaponAnimationComponent] Bound int variable: " + variableName + " (ID: " + variableId + ")", LogLevel.VERBOSE);
            return variableId;
        }
        
        Print("[AUS_WeaponAnimationComponent] ERROR: Failed to bind int variable: " + variableName, LogLevel.ERROR);
        return -1;
    }
    
    //------------------------------------------------------------------------------------------------
    // Bind a bool variable and return its ID for efficient access
    int BindVariableBool(string variableName)
    {
        if (m_VariableBoolIds.Contains(variableName))
            return m_VariableBoolIds.Get(variableName);
            
        int variableId = BindBoolVariable(variableName);
        if (variableId != -1)
        {
            m_VariableBoolIds.Set(variableName, variableId);
            Print("[AUS_WeaponAnimationComponent] Bound bool variable: " + variableName + " (ID: " + variableId + ")", LogLevel.VERBOSE);
            return variableId;
        }
        
        Print("[AUS_WeaponAnimationComponent] ERROR: Failed to bind bool variable: " + variableName, LogLevel.ERROR);
        return -1;
    }
    
    //------------------------------------------------------------------------------------------------
    // Set float variable by ID (efficient)
    void SetVariableFloat(int variableId, float value)
    {
        if (variableId == -1)
            return;
            
        // Debug variable updates
        static float lastDebugTime = -10000.0;
        float currentTime = System.GetTickCount();
        if (currentTime - lastDebugTime > 2000.0) // Every 2 seconds max
        {
            Print("[AUS_WeaponAnimationComponent] SetVariableFloat called - ID: " + variableId + ", Value: " + value, LogLevel.NORMAL);
            lastDebugTime = currentTime;
        }
            
        SetFloatVariable(variableId, value);
    }
    
    //------------------------------------------------------------------------------------------------
    // Set int variable by ID (efficient)
    void SetVariableInt(int variableId, int value)
    {
        if (variableId == -1)
            return;
            
        SetIntVariable(variableId, value);
    }
    
    //------------------------------------------------------------------------------------------------
    // Set bool variable by ID (efficient)
    void SetVariableBool(int variableId, bool value)
    {
        if (variableId == -1)
            return;
            
        SetBoolVariable(variableId, value);
    }
    
    //------------------------------------------------------------------------------------------------
    // Set float variable by name (less efficient - finds ID each time)
    void SetVariableFloatByName(string variableName, float value)
    {
        int variableId = BindVariableFloat(variableName);
        SetVariableFloat(variableId, value);
    }
    
    //------------------------------------------------------------------------------------------------
    // Set int variable by name (less efficient - finds ID each time)
    void SetVariableIntByName(string variableName, int value)
    {
        int variableId = BindVariableInt(variableName);
        SetVariableInt(variableId, value);
    }
    
    //------------------------------------------------------------------------------------------------
    // Set bool variable by name (less efficient - finds ID each time)
    void SetVariableBoolByName(string variableName, bool value)
    {
        int variableId = BindVariableBool(variableName);
        SetVariableBool(variableId, value);
    }
}