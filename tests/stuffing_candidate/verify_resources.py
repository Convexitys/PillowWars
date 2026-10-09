"""Actual PIE host/client normal input; isolated server position/resource fixtures.
Explicit callback injection is labelled separately from natural lifecycle tests.
"""
import unreal,time,json,traceback
from pathlib import Path
OUT=Path(__file__).parent;rows=[];handle=None;busy=False;due=0;started=time.monotonic()
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
levels.load_level('/Game/PillowWars/Maps/PillowWarsBedroomPolished')
def actors(w,c):return unreal.GameplayStatics.get_all_actors_of_class(w,c)
def prop(o,n):return o.get_editor_property(n)
def key(pc,k,hold=.08):unreal.PillowWarsEditorTestLibrary.queue_player_key(pc,k,hold)
def check(n,ok,d):
    rows.append(dict(test=n,passed=bool(ok),detail=d));unreal.log('PW_RESOURCE_TEST '+json.dumps(rows[-1]))
    (OUT/'resource-results.json').write_text(json.dumps(dict(results=rows),indent=2))
def place(pc,x,y=0,yaw=0):
    p=pc.get_controlled_pawn();p.get_movement_component().stop_movement_immediately();p.set_actor_location(unreal.Vector(x,y,350),False,True);p.set_actor_rotation(unreal.Rotator(0,yaw,0),True);pc.set_control_rotation(unreal.Rotator(0,yaw,0))
def held(pc):return prop(pc.player_state,'stuffing')
def fixture(pc,n):unreal.PillowWarsEditorTestLibrary.resource_fixture(pc,n,True)
def weapon(w,pc):return next(a for a in actors(w,unreal.PillowWarsWeapon) if a.get_owner()==pc.get_controlled_pawn())
def flow():
    unreal.PillowWarsEditorTestLibrary.configure_pie(2);levels.editor_request_begin_play();yield 7
    cw,sw=sorted(unreal.EditorLevelLibrary.get_pie_worlds(False),key=lambda w:len(actors(w,unreal.PillowWarsPlayerController)))
    host=next(c for c in actors(sw,unreal.PillowWarsPlayerController) if c.is_local_controller());client=actors(cw,unreal.PillowWarsPlayerController)[0];remote=next(c for c in actors(sw,unreal.PillowWarsPlayerController) if not c.is_local_controller());gm=actors(sw,unreal.PillowWarsGameMode)[0]
    ledger=lambda:json.loads(gm.get_resource_ledger_snapshot())
    key(host,'Enter');key(client,'Enter');yield .5;key(host,'Enter');key(client,'Enter');yield .5;key(host,'H');yield 5
    place(host,-700);place(remote,700);fixture(host,100);fixture(remote,100)
    for p in actors(sw,unreal.PillowWarsStuffingPickup):p.destroy_actor()
    yield .5;check('initial ledger conserved after explicit fixture cleanup',ledger()['residual']==0,ledger())
    # Hold client charge; settle/debit is through ordinary release RPC.
    before=ledger();w=weapon(sw,remote);seq=prop(w,'attack_sequence');key(client,'F',2.05);yield 1
    check('charging does not pay or spill early',abs(held(remote)-100)<.001 and ledger()['funded']==0 and prop(w,'attack_sequence')==seq,dict(held=held(remote),sequence=prop(w,'attack_sequence'),before=seq,ledger=ledger()))
    yield 1.5
    piles=[p for p in actors(sw,unreal.PillowWarsStuffingPickup) if not prop(p,'ambient_source')]
    check('full client charge pays 32 and spills funded 8',abs(held(remote)-68)<.01 and len(piles)==1 and abs(prop(piles[0],'remaining_stuffing')-8)<.01,dict(held=held(remote),ledger=ledger()))
    check('paid spill conserves and adds no ambient supply',ledger()['residual']==0 and ledger()['supply']==before['supply'] and ledger()['loss']-before['loss']==24000,ledger())
    yield .35;check('client sees paid spill and stuffing',abs(held(client)-held(remote))<.01 and len(actors(cw,unreal.PillowWarsStuffingPickup))==1,dict(client=held(client),server=held(remote)))
    # Leave pile before rest becomes eligible, then return normally.
    place(remote,500,500);yield .4;fixture(remote,0);seq=prop(w,'attack_sequence');key(client,'F');yield .8
    check('unaffordable input has no attack sequence or debit',prop(w,'attack_sequence')==seq and held(remote)==0,dict(sequence=prop(w,'attack_sequence'),stuffing=held(remote)))
    check('denial hint private to requesting client','Not enough stuffing' in client.get_action_hint() and not host.get_action_hint(),dict(client=client.get_action_hint(),host=host.get_action_hint()))
    if piles:
        loc=piles[0].get_actor_location();place(remote,loc.x,loc.y);yield 1.8
        check('grounded rest transfers finite paid pile once',abs(held(remote)-8)<.01 and not actors(sw,unreal.PillowWarsStuffingPickup) and ledger()['residual']==0,ledger())
        check('affordability hint delivered privately','Basic attack affordable' in client.get_action_hint() and not host.get_action_hint(),dict(client=client.get_action_hint(),host=host.get_action_hint()))
    fixture(host,100);place(host,0,550);yield .4;key(host,'G',1.5);yield 1.7
    check('guard drain follows elapsed server time',abs((100-held(host))-12)<1,dict(spent=100-held(host),ledger=ledger()))
    check('guard release and drain conserve resources',not prop(host.player_state,'guarding') and ledger()['residual']==0,ledger())
    # Natural V reclaim, then natural lifespan expiry; callbacks must not double credit.
    fixture(host,100);place(host,0,-550);yield .4;key(host,'E');yield .5
    covers=actors(sw,unreal.PillowWarsCover)
    check('cover commits paid 25',len(covers)==1 and abs(held(host)-75)<.01 and ledger()['cover']==25000 and ledger()['residual']==0,ledger())
    key(host,'V');yield .5
    check('V reclaim resolves to 15 salvage plus 10 loss',not actors(sw,unreal.PillowWarsCover) and ledger()['cover']==0 and ledger()['loose']==15000 and ledger()['residual']==0,ledger())
    place(host,-550,-550);fixture(host,100);yield 1;key(host,'E');yield .5
    covers=actors(sw,unreal.PillowWarsCover)
    check('second paid cover created',len(covers)==1 and ledger()['cover']==25000,ledger())
    if covers:
        cover=covers[0];b=ledger();unreal.PillowWarsEditorTestLibrary.resource_callback(cover,'break');one=ledger();unreal.PillowWarsEditorTestLibrary.resource_callback(cover,'expiry');two=ledger();cover.destroy_actor();yield .3
        check('injected duplicate break/expiry/destruction cannot double salvage',one['loose']-b['loose']==15000 and two==one and ledger()['loose']==one['loose'] and ledger()['residual']==0,ledger())
    place(host,0,650);fixture(host,100);yield 1;key(host,'E');yield .5;check('expiry test has committed cover',ledger()['cover']==25000,ledger());place(host,-700,0);yield 8.2
    check('natural lifespan expiry releases at most one salvage',ledger()['cover']==0 and ledger()['residual']==0 and ledger()['funded']==3,ledger())
    # A normal round ends by actual fall; both worlds agree, then vote reset.
    remote.get_controlled_pawn().set_actor_location(unreal.Vector(0,0,-800),False,True);yield 1
    sm=actors(sw,unreal.PillowWarsMatchState)[0];cm=actors(cw,unreal.PillowWarsMatchState)[0]
    check('elimination closes ledger with no orphan or resource residual',not ledger()['open'] and ledger()['residual']==0 and ledger()['held']==ledger()['loose']==ledger()['cover']==0,ledger())
    check('winner agrees on both actual worlds',bool(prop(sm,'result')) and prop(sm,'result')==prop(cm,'result'),dict(host=prop(sm,'result'),client=prop(cm,'result')))
    generation=ledger()['generation'];key(client,'R');yield .3;key(host,'R');yield 5
    check('rematch starts new resource generation and restores players',ledger()['open'] and ledger()['generation']>generation and held(host)==100 and held(remote)==100 and held(client)==100 and ledger()['residual']==0,ledger())
    check('rematch clears owner-only old hint',not client.get_action_hint() and not host.get_action_hint(),dict(client=client.get_action_hint(),host=host.get_action_hint()))
def finish(error=None):
    (OUT/'resource-results.json').write_text(json.dumps(dict(results=rows,error=error,configuration='UE5.5.3 real listen-server plus client PIE worlds on one laptop; normal F/G/E/V/R input; positions/resources scripted; duplicate callbacks explicitly injected'),indent=2));levels.editor_request_end_play();unreal.PillowWarsEditorTestLibrary.restore_pie();unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False);unreal.SystemLibrary.quit_editor()
script=flow()
def tick(dt):
    global busy,due
    if busy or time.monotonic()<due:return
    busy=True
    try:
        if time.monotonic()-started>160:raise RuntimeError('Resource timeout')
        due=time.monotonic()+next(script)
    except StopIteration:finish()
    except Exception:finish(traceback.format_exc())
    finally:busy=False
unreal.EditorPythonScripting.set_keep_python_script_alive(True);handle=unreal.register_slate_post_tick_callback(tick)
