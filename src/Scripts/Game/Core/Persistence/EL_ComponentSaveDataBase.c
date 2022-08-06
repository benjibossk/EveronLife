[BaseContainerProps()]
class EL_ComponentSaveDataBase
{
	bool ReadFrom(GenericComponent worldEntityComponent);
	bool ApplyTo(GenericComponent worldEntityComponent);
	
	bool IsFor(GenericComponent worldEntityComponent)
	{
		return true;
	}
}

class EL_ComponentSaveDataType
{
	static ref map<typename, typename> m_Mapping;
	
	void EL_ComponentSaveDataType(typename saveDataType, typename componentType, string dbEntityName = "")
	{
		if(!m_Mapping) m_Mapping = new map<typename, typename>();
		
		if(!saveDataType.IsInherited(EL_ComponentSaveDataBase))
		{
			Debug.Error(string.Format("Failed to register '%1' as persistence save-data type for '%2'. '%1' must inherit from '%3'.", saveDataType, componentType, EL_ComponentSaveDataBase));
		}
		
		m_Mapping.Set(saveDataType, componentType);
		
		if(dbEntityName)
		{
			EL_DbName.Set(saveDataType, dbEntityName);
		}
	}
	
	static typename Get(typename saveDataType)
	{
		if(!m_Mapping) return typename.Empty;
		
		return m_Mapping.Get(saveDataType);
	}
}
