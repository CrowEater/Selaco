
struct RenderEvent native ui version("2.4")
{
    native readonly Vector3 ViewPos;
    native readonly double ViewAngle;
    native readonly double ViewPitch;
    native readonly double ViewRoll;
    native readonly double FracTic;
    native readonly Actor Camera;
}

struct WorldEvent native play version("2.4")
{
    // for loaded/unloaded
    native readonly bool IsSaveGame;
    // this will be true if we are re-entering the hub level.
    native readonly bool IsReopen;
    // for unloaded, name of next map (if any)
    native readonly String NextMap;
    // for thingspawned/thingdied/thingdestroyed/thingground
    native readonly Actor Thing;
    // for thingdied. can be null
    native readonly Actor Inflictor;
    // for thingdamaged, line/sector damaged
    native readonly int Damage;
    native readonly Actor DamageSource;
    native readonly Name DamageType;
    native readonly EDmgFlags DamageFlags;
    native readonly double DamageAngle;
    // for line(pre)activated
    native readonly Line ActivatedLine;
	native readonly int ActivationType;
    native bool ShouldActivate;
    // for line/sector damaged
    native readonly SectorPart DamageSectorPart;
    native readonly Line DamageLine;
    native readonly Sector DamageSector;
    native readonly int DamageLineSide;
    native readonly vector3 DamagePosition;
    native readonly bool DamageIsRadius;
    native int NewDamage;
    native readonly State CrushedState;
}

struct PlayerEvent native play version("2.4")
{
    // this is the player number that caused the event.
    // note: you can get player struct from this by using players[e.PlayerNumber]
    native readonly int PlayerNumber;
    // this will be true if we are re-entering the hub level.
    native readonly bool IsReturn;
}

struct ConsoleEvent native version("2.4")
{
    // for net events, this will be the activator.
    // for UI events, this is always -1, and you need to check if level is loaded and use players[consoleplayer].
    native readonly int Player;
    // this is the name and args as specified in SendNetworkEvent or event/netevent CCMDs
    native readonly String Name;
    native readonly int Args[3];
    // this will be true if the event is fired from the console by event/netevent CCMD
    native readonly bool IsManual;
}

// This version number is meaningless right now
struct StatsEvent native version("4.1")
{
    native readonly String Name, Text;
    native readonly double Value;
    native readonly bool IsAchievement;
}

struct ReplaceEvent native version("2.4")
{
	native readonly Class<Actor> Replacee;
	native Class<Actor> Replacement;
	native bool IsFinal;
}

struct ReplacedEvent native version("3.7")
{
	native Class<Actor> Replacee;
	native readonly Class<Actor> Replacement;
	native bool IsFinal;
}

enum SaveType {
    SAVE_MANUAL = 0,
    SAVE_QUICK  = 1,
    SAVE_AUTO   = 2
}

class StaticEventHandler : Object native play version("2.4")
{
    // static event handlers CAN register other static event handlers.
    // unlike EventHandler.Create that will not create them.
    clearscope static native StaticEventHandler Find(Class<StaticEventHandler> type); // just for convenience. who knows.
    
    // these are called when the handler gets registered or unregistered
    // you can set Order/IsUiProcessor here.
    virtual void OnRegister() {}
    virtual void OnUnregister() {}

    // actual handlers are here
	virtual void WorldLoaded(WorldEvent e) {}
    virtual void WorldUnloaded(WorldEvent e) {}
    virtual void WorldThingSpawned(WorldEvent e) {}
    virtual void WorldThingDied(WorldEvent e) {}
    virtual void WorldThingGround(WorldEvent e) {}
    virtual void WorldThingRevived(WorldEvent e) {}
    virtual void WorldThingDamaged(WorldEvent e) {}
    virtual void WorldThingDestroyed(WorldEvent e) {}
    virtual void WorldLinePreActivated(WorldEvent e) {}
    virtual void WorldLineActivated(WorldEvent e) {}
    virtual void WorldSectorDamaged(WorldEvent e) {}
    virtual void WorldLineDamaged(WorldEvent e) {}
    virtual void WorldLightning(WorldEvent e) {} // for the sake of completeness.
    virtual void WorldTick() {}
	virtual String, Int GetSavegameComment() { return "", 0; }	              // @Cockatrice - Supply additional information to the savegame comment field during a save
    virtual bool IsSaveAllowed(bool quicksave) { return true; }               // @Cockatrice - Returning false from any event manager will prevent a savegame
    virtual void PreSave(int type) {}                                         // @Cockatrice - Called before any type of save. Use this to alter the world before save
    virtual void PostSave(int type) {}                                        // @Cockatrice - Called after any type of save. Use this to alter the world after save (not saving the changes)

    //
    //virtual ui void RenderFrame(RenderEvent e) {}
    virtual ui void RenderOverlay(RenderEvent e) {}
    virtual ui void RenderUnderlay(RenderEvent e) {}
    
    //
    virtual void PlayerEntered(PlayerEvent e) {}
    virtual void PlayerSpawned(PlayerEvent e) {}
    virtual void PlayerRespawned(PlayerEvent e) {}
    virtual void PlayerDied(PlayerEvent e) {}
    virtual void PlayerDisconnected(PlayerEvent e) {}
    
    //
	virtual ui bool UiProcess(UiEvent e) { return false; }
	virtual ui bool InputProcess(InputEvent e) { return false; }
    virtual ui void UiTick() {}
    virtual ui void PostUiTick() {}
    
    //
    virtual ui void ConsoleProcess(ConsoleEvent e) {}
    virtual void NetworkProcess(ConsoleEvent e) {}
    
    //
    virtual void CheckReplacement(ReplaceEvent e) {}
	virtual void CheckReplacee(ReplacedEvent e) {}

    // 
    virtual ui void StatProcess(StatsEvent e) {}
    //virtual void WorldStatProcess(StatsEvent e) {}

    //
    virtual void SkillChanged(int oldSkill, int newSkill) {}
    virtual bool SkillShouldChange(int oldSkill, int newSkill) { return true; }

    //
    virtual  void NewGame() {}

    // this value will be queried on Register() to decide the relative order of this handler to every other.
    // this is most useful in UI systems.
    // default is 0.
    native readonly int Order;
    native void SetOrder(int order);
    // this value will be queried on user input to decide whether to send UiProcess to this handler.
    native bool IsUiProcessor;
    // this value determines whether mouse input is required.
    native bool RequireMouse;
}

class EventHandler : StaticEventHandler native version("2.4")
{
    clearscope static native StaticEventHandler Find(class<StaticEventHandler> type);
    clearscope static native void SendNetworkEvent(String name, int arg1 = 0, int arg2 = 0, int arg3 = 0);
}
