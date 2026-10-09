import unreal,time,json,traceback
from pathlib import Path
OUT=Path(__file__).parent;rows=[];handle=None;busy=False;due=0;started=time.monotonic()
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);levels.load_level('/Game/PillowWars/Maps/PillowWarsBedroomPolished')
def actors(w,c):return unreal.GameplayStatics.get_all_actors_of_class(w,c)
def prop(o,n):return o.get_editor_property(n)
def key(pc,k,hold=.08):unreal.PillowWarsEditorTestLibrary.queue_player_key(pc,k,hold)
def check(n,ok,d):rows.append(dict(test=n,passed=bool(ok),detail=d));unreal.log('PW_PRACTICE_TEST '+json.dumps(rows[-1]))
def place(pc,x,y=0):
    p=pc.get_controlled_pawn();p.get_movement_component().stop_movement_immediately();p.set_actor_location(unreal.Vector(x,y,350),False,True);p.set_actor_rotation(unreal.Rotator(0,0,0),True);pc.set_control_rotation(unreal.Rotator(0,0,0))
def flow():
    unreal.PillowWarsEditorTestLibrary.configure_pie(1);levels.editor_request_begin_play();yield 7
    sw=unreal.EditorLevelLibrary.get_pie_worlds(False)[0];pc=actors(sw,unreal.PillowWarsPlayerController)[0];gm=actors(sw,unreal.PillowWarsGameMode)[0]
    key(pc,'Down');yield .2;key(pc,'Down');yield .3;key(pc,'Enter');yield 5
    for i in range(40):
        if pc.get_controlled_pawn():break
        yield .1
    check('practice challenge is optional and off initially',pc.get_practice_step()==-1,pc.get_practice_step())
    unreal.PillowWarsEditorTestLibrary.resource_fixture(pc,100,True)
    for p in actors(sw,unreal.PillowWarsStuffingPickup):p.destroy_actor()
    target=actors(sw,unreal.PillowWarsPracticeTarget)[0];target.set_actor_location(unreal.Vector(110,0,350),False,True);place(pc,0);yield .5
    key(pc,'T');yield .3;check('T starts optional practice',pc.get_practice_step()==0,pc.get_practice_step())
    hits=prop(target,'hits');key(pc,'F');yield .85
    check('normal dummy hit advances to low-resource challenge',prop(target,'hits')==hits+1 and pc.get_practice_step()==1 and abs(prop(pc.player_state,'stuffing')-4)<.01,dict(step=pc.get_practice_step(),stuffing=prop(pc.player_state,'stuffing'),hits=prop(target,'hits')))
    key(pc,'F');yield .75;check('rejected normal input advances recovery instruction',pc.get_practice_step()==2 and 'Not enough stuffing' in pc.get_action_hint(),dict(step=pc.get_practice_step(),hint=pc.get_action_hint()))
    piles=[p for p in actors(sw,unreal.PillowWarsStuffingPickup) if not prop(p,'ambient_source')];assert piles
    loc=piles[0].get_actor_location();place(pc,loc.x,loc.y);yield 1.9
    check('finite pile rest unlocks affordable attack step',pc.get_practice_step()==3 and prop(pc.player_state,'stuffing')>=8,dict(step=pc.get_practice_step(),stuffing=prop(pc.player_state,'stuffing')))
    place(pc,0);yield .5;key(pc,'F');yield .85;check('normal dummy hit completes challenge',pc.get_practice_step()==4 and prop(target,'hits')==hits+2,dict(step=pc.get_practice_step(),hits=prop(target,'hits')))
    check('practice resource bookkeeping remains conserved',json.loads(gm.get_resource_ledger_snapshot())['residual']==0,gm.get_resource_ledger_snapshot())
    key(pc,'T');yield .3;check('T restarts completed challenge',pc.get_practice_step()==0,pc.get_practice_step());key(pc,'T');yield .3;check('T can skip unfinished challenge',pc.get_practice_step()==-1,pc.get_practice_step())
def finish(error=None):
    (OUT/'practice-results.json').write_text(json.dumps(dict(results=rows,error=error,configuration='one real standalone PIE world; T/F production input with server position/resource fixtures; no map saved'),indent=2));levels.editor_request_end_play();unreal.PillowWarsEditorTestLibrary.restore_pie();unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False);unreal.SystemLibrary.quit_editor()
script=flow()
def tick(dt):
    global busy,due
    if busy or time.monotonic()<due:return
    busy=True
    try:
        if time.monotonic()-started>110:raise RuntimeError('Practice timeout')
        due=time.monotonic()+next(script)
    except StopIteration:finish()
    except Exception:finish(traceback.format_exc())
    finally:busy=False
unreal.EditorPythonScripting.set_keep_python_script_alive(True);handle=unreal.register_slate_post_tick_callback(tick)
