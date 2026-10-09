"""Real listen-server/client PIE input tests. Positions and resource/health fixtures
are scripted; F/W/R use the normal local player input and RPC paths. No maps saved.
"""
import json,time,traceback
from pathlib import Path
import unreal
OUT=Path(__file__).parent
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
levels.load_level('/Game/PillowWars/Maps/PillowWarsBedroomPolished')
rows=[];handle=None;due=0;busy=False;started=time.monotonic()
def actors(w,c):return unreal.GameplayStatics.get_all_actors_of_class(w,c)
def prop(o,n):return o.get_editor_property(n)
def key(pc,k,hold=.08):unreal.PillowWarsEditorTestLibrary.queue_player_key(pc,k,hold)
def ps(pc):return pc.player_state
def weapon(w,pc):return next(a for a in actors(w,unreal.PillowWarsWeapon) if a.get_owner()==pc.get_controlled_pawn())
def match(w):return actors(w,unreal.PillowWarsMatchState)[0]
def snap(w):return {p.get_player_name():dict(health=prop(p,'health'),daze=prop(p,'daze'),out=prop(p,'eliminated'),stuff=prop(p,'stuffing'),hits=prop(p,'round_hits'),taken=prop(p,'round_hits_taken'),damage=prop(p,'last_hit_damage'),crit=prop(p,'last_hit_critical')) for p in actors(w,unreal.PillowWarsPlayerState)}
def save(error=None):(OUT/'health-results.json').write_text(json.dumps(dict(results=rows,error=error,configuration='UE5.5.3, two actual network PIE worlds, listen host and client, one server-only bot; normal input with scripted positioning and resources'),indent=2))
def check(name,passed,detail):
    row=dict(test=name,passed=bool(passed),detail=detail);rows.append(row);unreal.log('PW_HEALTH_TEST '+json.dumps(row));save()
def place(pc,x,y=0,yaw=0):
    p=pc.get_controlled_pawn();m=p.get_movement_component();m.stop_movement_immediately();m.set_movement_mode(unreal.MovementMode.MOVE_WALKING)
    p.set_actor_location(unreal.Vector(x,y,350),False,True);p.set_actor_rotation(unreal.Rotator(0,yaw,0),True);pc.set_control_rotation(unreal.Rotator(0,yaw,0));return p
def fixture(pc,hp=100,stuff=100):
    s=ps(pc);s.set_editor_property('health',float(hp));s.set_editor_property('stuffing',float(stuff));s.set_editor_property('daze',0.);s.force_net_update()
def flow():
    for resonance,expected in [(0,.015),(25,.14875),(50,.2825),(75,.41625),(100,.55),(-50,.015),(150,.55)]:
        v=unreal.PillowWarsGameMode.critical_chance(resonance);check('crit curve '+str(resonance),abs(v-expected)<.00001,v)
    for p,cost in [(-1,8),(0,8),(.25,14),(.5,20),(.75,26),(1,32)]:
        v=unreal.PillowWarsGameMode.swing_stuffing_cost(p);check('swing cost '+str(p),abs(v-cost)<.001,v)
    check('critical multiplier and guard formula',abs(unreal.PillowWarsGameMode.health_damage(1,1,True,False)-61.25)<.001 and abs(unreal.PillowWarsGameMode.health_damage(-1,1,False,True)-5.4)<.001,dict(full_crit=unreal.PillowWarsGameMode.health_damage(1,1,True,False),guard=unreal.PillowWarsGameMode.health_damage(-1,1,False,True)))
    unreal.PillowWarsEditorTestLibrary.configure_pie(2);levels.editor_request_begin_play();yield 7
    worlds=unreal.EditorLevelLibrary.get_pie_worlds(False);assert len(worlds)==2
    humans=lambda w:[c for c in actors(w,unreal.PillowWarsPlayerController) if not isinstance(c,unreal.PillowWarsBotController)]
    cw,sw=sorted(worlds,key=lambda w:len(humans(w)))
    host=next(c for c in humans(sw) if c.is_local_controller());client=humans(cw)[0];remote=next(c for c in humans(sw) if not c.is_local_controller())
    key(host,'Enter');key(client,'Enter');yield .5;key(host,'F6');yield .6
    bot=actors(sw,unreal.PillowWarsBotController)[0];bot.set_editor_property('thinking_enabled',False)
    key(host,'Enter');key(client,'Enter');yield .4;key(host,'F8');yield 5
    check('both players load and all fighters start at 100 health','PLAYING' in str(prop(match(sw),'match_phase')).upper() and all(v['health']==100 for v in snap(sw).values()) and snap(sw)==snap(cw),dict(server=snap(sw),client=snap(cw)))
    place(bot,1200,700)
    for label,pc,serverpc in [('host',host,host),('client',client,remote)]:
        place(serverpc,-300,600);yield .4;before=serverpc.get_controlled_pawn().get_actor_location();key(pc,'W',.35);yield .6
        check(label+' W movement', (serverpc.get_controlled_pawn().get_actor_location()-before).length()>60,(serverpc.get_controlled_pawn().get_actor_location()-before).length())
    place(host,110,0,180);place(remote,0);yield .6
    fixture(remote);fixture(host);before=prop(ps(host),'round_hits_taken');key(client,'F');yield 1
    damage=prop(ps(host),'last_hit_damage')
    check('client F causes one server-authoritative health hit',prop(ps(host),'round_hits_taken')==before+1 and 0<prop(ps(host),'health')<100 and abs(100-prop(ps(host),'health')-damage)<.01,snap(sw))
    check('health, Daze, critical result and hits replicate',snap(sw)==snap(cw),dict(server=snap(sw),client=snap(cw)))
    # A distant miss spends stuffing but cannot change target health/hit count.
    place(remote,-700);place(host,700);yield .5;fixture(remote);before=prop(ps(host),'health');taken=prop(ps(host),'round_hits_taken');key(client,'F');yield 1
    check('miss costs 8 stuffing without damage',abs(prop(ps(remote),'stuffing')-92)<.5 and prop(ps(host),'health')==before and prop(ps(host),'round_hits_taken')==taken,snap(sw))
    measurements=[]
    for held in [.5,2.05]:
        place(remote,0);place(host,110,0,180);fixture(remote);fixture(host);yield .5
        before_hits=prop(ps(host),'round_hits_taken');key(client,'F',held);yield held-.1
        check('no damage during charge '+str(held),prop(ps(host),'round_hits_taken')==before_hits,snap(sw))
        yield 1.15
        w=weapon(sw,remote);power=prop(w,'charge_power');cost=100-prop(ps(remote),'stuffing');damage=prop(ps(host),'last_hit_damage');crit=prop(ps(host),'last_hit_critical')
        measurements.append(dict(power=power,cost=cost,damage=damage,critical=crit))
        check('charged input cost and hit '+str(held),prop(ps(host),'round_hits_taken')==before_hits+1 and abs(cost-(8+24*power))<.6,measurements[-1])
    check('full uppercut costs more and has greater base damage',measurements[1]['cost']>measurements[0]['cost']+15 and measurements[1]['damage']/(1.75 if measurements[1]['critical'] else 1)>measurements[0]['damage']/(1.75 if measurements[0]['critical'] else 1),measurements)
    # Not enough stuffing: no attack accepted or resource charged.
    place(remote,-700);place(host,700);fixture(remote,100,10);yield .4;w=weapon(sw,remote);seq=prop(w,'attack_sequence');key(client,'F',2.05);yield 3
    check('insufficient stuffing rejects full-power swing',prop(w,'attack_sequence')==seq and abs(prop(ps(remote),'stuffing')-10)<.1 and prop(w,'charge_start_time')<0,dict(sequence=prop(w,'attack_sequence'),stuff=prop(ps(remote),'stuffing')))
    fixture(remote);yield .4;seq=prop(w,'attack_sequence')
    for _ in range(4):key(client,'F',.04);yield .1
    yield .65
    check('rapid input preserves cooldown and charges only accepted swing',prop(w,'attack_sequence')-seq==1 and abs(prop(ps(remote),'stuffing')-92)<.5,dict(sequence_delta=prop(w,'attack_sequence')-seq,stuff=prop(ps(remote),'stuffing')))
    # Confirm a live server critical at full gauge; this is not a probability-distribution claim.
    critical_seen=False
    for attempt in range(20):
        place(remote,0);place(host,110,0,180);fixture(remote);fixture(host);yield .3
        w=weapon(sw,remote);w.set_editor_property('resonance_charge',100.);w.set_editor_property('last_resonance_impact_time',unreal.GameplayStatics.get_time_seconds(sw));w.force_net_update()
        before=prop(ps(host),'round_hits_taken');key(client,'F');yield .9
        if prop(ps(host),'round_hits_taken')>before and prop(ps(host),'last_hit_critical'):
            critical_seen=True;break
    check('live full-gauge client critical is 55% and 1.75x tap damage',critical_seen and abs(prop(w,'attack_critical_chance')-.55)<.0001 and abs(prop(ps(host),'last_hit_damage')-26.25)<.01,dict(attempts=attempt+1,chance=prop(w,'attack_critical_chance'),snapshot=snap(sw)))
    yield .4;check('critical flag and damage agree on client',snap(sw)==snap(cw),dict(server=snap(sw),client=snap(cw)))
    # Host input can also deplete client health. Third fighter keeps round alive.
    place(host,0);place(remote,110,0,180);fixture(host);fixture(remote,1);yield .5
    key(host,'F');yield 1
    check('zero-health client is eliminated with no pawn on either world',prop(ps(remote),'health')==0 and prop(ps(remote),'eliminated') and remote.get_controlled_pawn() is None and client.get_controlled_pawn() is None,snap(sw))
    check('round continues with host and bot alive','PLAYING' in str(prop(match(sw),'match_phase')).upper() and not prop(match(sw),'result'),str(prop(match(sw),'match_phase')))
    round_number=prop(match(sw),'round_number');key(host,'R');key(client,'R');key(client,'F');yield 1.2
    check('neither player can respawn or reset a live competitive round',remote.get_controlled_pawn() is None and client.get_controlled_pawn() is None and prop(match(sw),'round_number')==round_number,snap(sw))
    # Kill remaining bot through a normal host hit, rather than directly eliminating it.
    place(host,0);place(bot,110,0,180);fixture(host);fixture(bot,1);yield .5;key(host,'F');yield 1.1
    result=prop(match(sw),'result');other=prop(match(cw),'result')
    check('health elimination produces one agreeing winner',result==other and ps(host).get_player_name() in result and prop(ps(bot),'eliminated') and bot.get_controlled_pawn() is None and prop(ps(host),'round_wins')==1,dict(server=result,client=other,roster=snap(sw)))
    key(client,'R');yield .6
    check('client vote does not independently resurrect anyone',remote.get_controlled_pawn() is None and prop(match(sw),'rematch_votes')==1,dict(votes=prop(match(sw),'rematch_votes'),roster=snap(sw)))
    key(host,'R');yield 4.8
    check('explicit post-result rematch restores players and bot to full health',all(c.get_controlled_pawn() for c in [host,client,remote,bot]) and all(v['health']==100 and not v['out'] and v['stuff']==100 and v['daze']==0 for v in snap(sw).values()) and snap(sw)==snap(cw),dict(server=snap(sw),client=snap(cw)))
    # Retain falling-out elimination and the same winner agreement.
    remote.get_controlled_pawn().set_actor_location(unreal.Vector(0,0,-800),False,True)
    bot.get_controlled_pawn().set_actor_location(unreal.Vector(0,0,-800),False,True);yield 1
    check('ring-outs still set health zero and agree on winner',prop(ps(remote),'health')==0 and prop(ps(bot),'health')==0 and prop(match(sw),'result')==prop(match(cw),'result') and ps(host).get_player_name() in prop(match(sw),'result'),dict(server=snap(sw),client=snap(cw),result=prop(match(sw),'result')))
def finish(error=None):
    save(error);levels.editor_request_end_play();unreal.PillowWarsEditorTestLibrary.restore_pie();unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False);unreal.SystemLibrary.quit_editor()
script=flow()
def tick(dt):
    global busy,due
    if busy or time.monotonic()<due:return
    busy=True
    try:
        if time.monotonic()-started>220:raise RuntimeError('Health test timeout')
        due=time.monotonic()+next(script)
    except StopIteration:finish()
    except Exception:finish(traceback.format_exc())
    finally:busy=False
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle=unreal.register_slate_post_tick_callback(tick)
