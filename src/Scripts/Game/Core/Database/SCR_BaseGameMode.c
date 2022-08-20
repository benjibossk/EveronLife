modded class SCR_BaseGameMode
{
	override void OnGameEnd()
	{
		// On game end before any final flushes, hard map the async api into sync to make those operations blocking and not loose data
		EL_DbContext.s_bForceSyncApi = true;
		
		super.OnGameEnd();
	}
	
	void ~SCR_BaseGameMode()
	{
		// TODO: Find better even for "session" teardown that works on dedicated servers, workbench and player hosted missions.
		EL_DbContextFactory.ResetCache();
		EL_DbRepositoryFactory.ResetCache();
	}
}