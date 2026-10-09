"""Real host/client visual-state measurements; scripted gauge/position fixtures,
ordinary F attacks through production input/RPC; no assets or maps saved.
"""
import unreal,time,json,traceback,math
from pathlib import Path
OUT=Path(__file__).parent;rows=[];samples=[];handle=None;busy=False;due=0;started=time.monotonic()
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);levels.load_level('/Game/PillowWars/Maps/PillowWarsBedroomPolished')
def actors(w,c):return unreal.GameplayStatics.get_all_actors_of_class(w,c)
def prop(o,n):return o.get_editor_property(n)
def key(pc,k,hold=.08):unreal.PillowWarsEditorTestLibrary.queue_player_key(pc,k,hold)
def weapon(w,pc):return next(a for a in actors(w,unreal.PillowWarsWeapon) if a.get_owner()==pc.get_controlled_pawn())
def check(name,passed,detail):rows.append(dict(test=name,passed=bool(passed),detail=detail));unreal.log('PW_BOBBLE_TEST '+json.dumps(rows[-1]))
def xy(v):return [v.x,v.y]
def place(pc,x,y=0,yaw=0):
    p=pc.get_controlled_pawn();p.get_movement_component().stop_movement_immediately();p.set_actor_location(unreal.Vector(x,y,350),False,True);p.set_actor_rotation(unreal.Rotator(0,yaw,0),True);pc.set_control_rotation(unreal.Rotator(0,yaw,0))
def gauge(w,value):w.set_editor_property('resonance_charge',float(value));w.set_editor_property('last_resonance_impact_time',unreal.GameplayStatics.get_time_seconds(w));w.force_net_update()
def flow():
    for r,expected in [(0,1),(25,1.140625),(50,1.45),(75,1.759375),(100,1.9)]:
        v=unreal.PillowWarsWeapon.resonance_reaction_gain(r);check('reaction gain '+str(r),abs(v-expected)<.0001,v)
    unreal.PillowWarsEditorTestLibrary.configure_pie(2);levels.editor_request_begin_play();yield 7
    worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
    cw,sw=sorted(worlds,key=lambda w:len(actors(w,unreal.PillowWarsPlayerController)))
    host=next(c for c in actors(sw,unreal.PillowWarsPlayerController) if c.is_local_controller());client=actors(cw,unreal.PillowWarsPlayerController)[0];remote=next(c for c in actors(sw,unreal.PillowWarsPlayerController) if not c.is_local_controller())
    key(host,'Enter');key(client,'Enter');yield .5;key(host,'Enter');key(client,'Enter');yield .5;key(host,'H');yield 5
    place(host,700);place(remote,-700);yield .5
    hw=weapon(sw,host);cw_host=next(a for a in actors(cw,unreal.PillowWarsWeapon) if a.get_owner() and prop(a.get_owner(),'player_state') and prop(a.get_owner(),'player_state').get_player_name()==host.player_state.get_player_name())
    peak=[]
    for r in [0,50,100]:
        gauge(hw,r);yield .7
        values=[]
        for i in range(12):yield .05;values.append(prop(hw,'resonance_bobble').length())
        peak.append(max(values));check('bounded ready bobble '+str(r),max(values)<4.9,values)
    check('high Resonance increases visible ready sway',peak[0]<.01 and peak[2]>peak[1]*1.5 and peak[2]>2.5,peak)
    samples=[]
    for i in range(8):yield .07;samples.append((prop(hw,'resonance_bobble')-prop(cw_host,'resonance_bobble')).length())
    check('host ready bobble agrees with client within timing tolerance',max(samples)<1.5,samples)
    gauge(hw,100);key(host,'F',1.0);yield .55
    check('extra ready sway is suppressed during charged attack',prop(hw,'resonance_bobble').length()<.15,xy(prop(hw,'resonance_bobble')));yield 1.1
    # Each receiver starts with zero combat Daze, avoiding a stronger second
    # knockback being mistaken for the gauge effect. Opposite-side inputs use
    # production swings; directions need not be identical animation samples.
    peaks=[]
    for start_gauge,inputpc,attacker,victim in [(0,client,remote,host),(100,host,host,remote)]:
        place(attacker,0);place(victim,110,0,180);victim.player_state.set_editor_property('health',100.);attacker.player_state.set_editor_property('stuffing',100.);yield .5
        receiver=weapon(sw,victim);gauge(receiver,start_gauge);before=prop(receiver,'reaction_sequence');key(inputpc,'F');yield .35
        values=[]
        for i in range(9):yield .04;values.append(prop(receiver,'head_reaction').length())
        peaks.append(max(values));check('incoming hit event once at gauge '+str(start_gauge),prop(receiver,'reaction_sequence')==before+1 and max(values)>1,dict(peak=max(values),sequence=prop(receiver,'reaction_sequence')))
        yield 4.8
        check('head settles after hits stop '+str(start_gauge),prop(receiver,'head_reaction').length()<.15 and prop(receiver,'resonance_bobble').length()<.15,dict(reaction=xy(prop(receiver,'head_reaction')),ready=xy(prop(receiver,'resonance_bobble'))))
    check('full Resonance incoming head reaction is stronger',peaks[1]>peaks[0]*1.1,peaks)
    place(remote,-700);place(host,700);yield .5;before=prop(hw,'reaction_sequence');key(client,'F');yield .9
    check('miss does not generate hit or head-reaction event',prop(hw,'reaction_sequence')==before,prop(hw,'reaction_sequence'))
    # Repeated hits remain bounded and replicated, even during accumulating springs.
    for i in range(4):
        place(remote,0);place(host,110,0,180);host.player_state.set_editor_property('health',100.);remote.player_state.set_editor_property('stuffing',100.);yield .25;key(client,'F');yield .55
        v=prop(hw,'head_reaction');check('repeated hit remains bounded '+str(i),abs(v.x)<=28 and abs(v.y)<=28 and math.isfinite(v.length()),xy(v))
    yield .4
    check('reaction event and gauge replicate',prop(hw,'reaction_sequence')==prop(cw_host,'reaction_sequence') and abs(prop(hw,'resonance_charge')-prop(cw_host,'resonance_charge'))<.01,dict(server_sequence=prop(hw,'reaction_sequence'),client_sequence=prop(cw_host,'reaction_sequence'),server_gauge=prop(hw,'resonance_charge'),client_gauge=prop(cw_host,'resonance_charge')))
    yield 5.2
    check('repeated head motion eventually settles on both players',max(prop(w,'head_reaction').length()+prop(w,'resonance_bobble').length() for w in [hw,cw_host])<.15,dict(server=xy(prop(hw,'head_reaction')),client=xy(prop(cw_host,'head_reaction'))))
def finish(error=None):
    (OUT/'bobble-results.json').write_text(json.dumps(dict(results=rows,error=error,configuration='two real PIE network worlds, same laptop, normal F input; gauge, positions and survivable health are scripted fixtures'),indent=2));levels.editor_request_end_play();unreal.PillowWarsEditorTestLibrary.restore_pie();unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False);unreal.SystemLibrary.quit_editor()
script=flow()
def tick(dt):
    global busy,due
    if busy or time.monotonic()<due:return
    busy=True
    try:
        if time.monotonic()-started>170:raise RuntimeError('Bobble timeout')
        due=time.monotonic()+next(script)
    except StopIteration:finish()
    except Exception:finish(traceback.format_exc())
    finally:busy=False
unreal.EditorPythonScripting.set_keep_python_script_alive(True);handle=unreal.register_slate_post_tick_callback(tick)
