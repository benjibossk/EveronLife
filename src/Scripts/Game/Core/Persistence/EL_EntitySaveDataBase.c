[BaseContainerProps()]
class EL_EntitySaveDataBase : EL_DbEntity
{
	[NonSerialized(), Attribute(desc: "Sava-data types for components to persist.")]
	ref array<ref EL_ComponentSaveDataBase> m_aComponents;
	
	int m_iDataLayoutVersion = 1;
	EL_DateTimeUtcAsInt m_iLastSaved;
	
	ResourceName m_rPrefab;
	ref map<typename, ref array<ref EL_ComponentSaveDataBase>> m_mComponentsSaveData;
	
	bool ReadFrom(notnull IEntity worldEntity)
	{
		EL_PersistenceComponent persistenceComponent = EL_PersistenceComponent.Cast(worldEntity.FindComponent(EL_PersistenceComponent));
		EL_PersistenceComponentClass settings = EL_PersistenceComponentClass.Cast(persistenceComponent.GetComponentData(worldEntity));
		
		SetId(persistenceComponent.GetPersistentId());
		m_iLastSaved = persistenceComponent.GetLastSaved();
		
		m_rPrefab = EL_Utils.GetPrefabName(worldEntity);
		
		m_mComponentsSaveData = new map<typename, ref array<ref EL_ComponentSaveDataBase>>();
		
		// Go through hierarchy sorted component types
		foreach(typename componentSaveDataType : settings.m_aComponentSaveDataTypenames)
		{
			array<ref EL_ComponentSaveDataBase> componentsSaveData();
			
			// Special handling for transformation as its not really a "component"
			if(componentSaveDataType == EL_TransformationSaveData)
			{
				EL_TransformationSaveData transformationSaveData = EL_TransformationSaveData.Cast(componentSaveDataType.Spawn());
				if(!transformationSaveData || !transformationSaveData.ReadFrom(worldEntity)) return false;
				
				componentsSaveData.Insert(transformationSaveData);
			}
			else
			{
				array<Managed> outComponents();
				worldEntity.FindComponents(EL_ComponentSaveDataType.Get(componentSaveDataType), outComponents);
				foreach(Managed componentRef : outComponents)
				{
					typename componentType = componentRef.Type();
					
					// Ingore base class find machtes if the parent class was already processed
					if(m_mComponentsSaveData.Contains(componentType)) continue;
					
					EL_ComponentSaveDataBase componentSaveData = EL_ComponentSaveDataBase.Cast(componentSaveDataType.Spawn());
					if(!componentSaveData || !componentSaveData.ReadFrom(GenericComponent.Cast(componentRef))) return false;
					
					componentsSaveData.Insert(componentSaveData);
				}
			}
			
			if(componentsSaveData.Count() > 0)
			{
				m_mComponentsSaveData.Set(componentSaveDataType, componentsSaveData);
			}
		}
		
		return true;
	}
	
	IEntity Spawn()
	{
		return EL_PersistenceManager.GetInstance().SpawnWorldEntity(this);
	}
	
	bool ApplyTo(notnull IEntity worldEntity)
	{
		EL_PersistenceComponent persistenceComponent = EL_PersistenceComponent.Cast(worldEntity.FindComponent(EL_PersistenceComponent));
		EL_PersistenceComponentClass settings = EL_PersistenceComponentClass.Cast(persistenceComponent.GetComponentData(worldEntity));
		
		// Special handling for transformation as its not really a "component" and it helps other component loads if the entity position is already set.
		array<ref EL_ComponentSaveDataBase> transformData = m_mComponentsSaveData.Get(EL_TransformationSaveData);
		if(transformData && transformData.Count() > 0) EL_TransformationSaveData.Cast(transformData.Get(0)).ApplyTo(worldEntity);
		
		set<Managed> processedInstances();
		set<typename> processedSaveDataTypes();
		processedSaveDataTypes.Insert(EL_TransformationSaveData);
		foreach(typename componentSaveDataType : settings.m_aComponentSaveDataTypenames)
		{
			if(!ApplyComponentTo(componentSaveDataType, worldEntity, processedSaveDataTypes, processedInstances, settings)) return false;
		}
		
		// Update any non character entity. On character this can cause fall through ground.
		if(!ChimeraCharacter.Cast(worldEntity)) worldEntity.Update();
		
		return true;
	}
	
	protected bool ApplyComponentTo(typename componentSaveDataType, IEntity worldEntity, set<typename> processedSaveDataTypes, set<Managed> processedInstances, EL_PersistenceComponentClass settings)
	{
		// Skip already processed save data
		if (processedSaveDataTypes.Contains(componentSaveDataType)) return true;
		processedSaveDataTypes.Insert(componentSaveDataType);
		
		// Make sure required save data is already applied
		array<ref EL_ComponentSaveDataBase> componentsSaveData = m_mComponentsSaveData.Get(componentSaveDataType);
		if(!componentsSaveData || componentsSaveData.IsEmpty()) return true;
		
		array<typename> requiredSaveDataTypes = componentsSaveData.Get(0).Requires();
		if(requiredSaveDataTypes)
		{
			foreach(typename requiredSaveDataType : requiredSaveDataTypes)
			{
				foreach(typename possibleType : settings.m_aComponentSaveDataTypenames)
				{
					if(!possibleType.IsInherited(requiredSaveDataType)) continue;
					
					ApplyComponentTo(possibleType, worldEntity, processedSaveDataTypes, processedInstances, settings);
				}
			}
		}
		
		// Apply save data to matching components
		array<Managed> outComponents();
		worldEntity.FindComponents(EL_ComponentSaveDataType.Get(componentSaveDataType), outComponents);
		foreach(EL_ComponentSaveDataBase componentSaveData : componentsSaveData)
		{
			bool applied = false;
			
			foreach(Managed componentRef : outComponents)
			{
				if(!processedInstances.Contains(componentRef) && componentSaveData.IsFor(GenericComponent.Cast(componentRef)))
				{
					if(!componentSaveData.ApplyTo(GenericComponent.Cast(componentRef))) return false;
					processedInstances.Insert(componentRef);
					applied = true;
					break;
				}
			}
			
			if(!applied)
			{
				Print(string.Format("No matching component for '%1' found on entity '%2'@%3", componentSaveData.Type().ToString(), EL_Utils.GetPrefabName(worldEntity), worldEntity.GetOrigin()), LogLevel.VERBOSE);
			}
		}
		
		return true;
	}
	
	protected bool SerializationSave(BaseSerializationSaveContext saveContext)
	{
		if (!saveContext.IsValid()) return false;
		
		saveContext.WriteValue("m_iDataLayoutVersion", m_iDataLayoutVersion);
		saveContext.WriteValue("m_sId", GetId());
		saveContext.WriteValue("m_iLastSaved", m_iLastSaved);
		saveContext.WriteValue("m_rPrefab", m_rPrefab.Substring(1, m_rPrefab.IndexOf("}") - 1));
		
		saveContext.StartObject("components");
		
		// Include components order info so they can be read back in the same order again (important for binary formats)
		array<string> componentTypesInOrder();
		array<EL_ComponentSaveDataBase> writeComponents();
	
		foreach(typename saveDataType, array<ref EL_ComponentSaveDataBase> componentsSaveData : m_mComponentsSaveData)
		{
			// If the same component type is included multiple times its written mutiple times
			foreach(EL_ComponentSaveDataBase component : componentsSaveData)
			{
				componentTypesInOrder.Insert(EL_DbName.Get(saveDataType));
				writeComponents.Insert(component);
			}
		}
		
		// Order needs to be written first for binary file types
		saveContext.WriteValue("order", componentTypesInOrder);
		
		// Then component data block can be written
		foreach(int idx, EL_ComponentSaveDataBase component : writeComponents)
		{
			saveContext.WriteValue(idx.ToString(), component);
		}
		
		saveContext.EndObject();
		
		return true;
	}

	protected bool SerializationLoad(BaseSerializationLoadContext loadContext)
	{
		if (!loadContext.IsValid()) return false;
		
		loadContext.ReadValue("m_iDataLayoutVersion", m_iDataLayoutVersion);
		
		string id;
		loadContext.ReadValue("m_sId", id);
		SetId(id);
		
		loadContext.ReadValue("m_iLastSaved", m_iLastSaved);
		
		loadContext.ReadValue("m_rPrefab", m_rPrefab);
		m_rPrefab = string.Format("{%1}", m_rPrefab);
		
		loadContext.StartObject("components");
		
		array<string> componentNamesInOrder;
		loadContext.ReadValue("order", componentNamesInOrder);
		
		m_mComponentsSaveData = new map<typename, ref array<ref EL_ComponentSaveDataBase>>();
		
		foreach(int idx, string componentName : componentNamesInOrder)
		{
			typename componentSaveDataType = EL_DbName.GetTypeByName(componentName);
			EL_ComponentSaveDataBase componentSaveData = EL_ComponentSaveDataBase.Cast(componentSaveDataType.Spawn());
			loadContext.ReadValue(idx.ToString(), componentSaveData);
			
			if(!m_mComponentsSaveData.Contains(componentSaveDataType))
			{				
				m_mComponentsSaveData.Set(componentSaveDataType, {componentSaveData});
				continue;
			}
			
			m_mComponentsSaveData.Get(componentSaveDataType).Insert(componentSaveData);
		}
		
		loadContext.EndObject();
		
		return true;
	}
}
