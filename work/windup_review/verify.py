"""Real two-world listen-server PIE, normal controller InputKey events.
Positions/resources are controlled fixtures, not a claim of unscripted play.
"""
import unreal, time, json, math, traceback
from pathlib import Path
OUT=Path(__file__).parent; OUT.mkdir(exist_ok=True)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
results=[]; samples=[]; started=time.monotonic(); handle=None; busy=False
def actors(w,c): return unreal.GameplayStatics.get_all_actors_of_class(w,c)
def prop(x,n): return x.get_editor_property(n)
def key(pc,k,hold=.1): unreal.PillowWarsEditorTestLibrary.queue_player_key(pc,k,hold)
def check(name,passed,detail):
    row=dict(test=name,passed=bool(passed),detail=detail); results.append(row)
    unreal.log('PW_REVIEW '+json.dumps(row)); save()
def save(error=None): (OUT/'functional-results.json').write_text(json.dumps(dict(results=results,samples=samples,error=error),indent=2))
def pos(p): return p.get_actor_location()
def xyz(v): return [v.x,v.y,v.z]
def dist(a,b):return (a-b).length()
def state(pc):return pc.player_state
def weapon(w,p):return next(x for x in actors(w,unreal.PillowWarsWeapon) if x.get_owner()==p)
def place(pc,location,yaw=0):
    p=pc.get_controlled_pawn(); p.get_movement_component().stop_movement_immediately()
    p.set_actor_location(location,False,True); p.set_actor_rotation(unreal.Rotator(0,yaw,0),True)
    pc.set_control_rotation(unreal.Rotator(0,yaw,0)); return p
def phase(w):return str(prop(actors(w,unreal.PillowWarsMatchState)[0],'match_phase'))
def snapshot(w):
    return {x.get_player_name():dict(daze=prop(x,'daze'),hits=prop(x,'round_hits'),taken=prop(x,'round_hits_taken'),stuff=prop(x,'stuffing'),rest=prop(x,'stuffing_resting'),out=prop(x,'eliminated')) for x in actors(w,unreal.PillowWarsPlayerState)}
def flow():
    unreal.PillowWarsEditorTestLibrary.configure_pie(2)
    levels.editor_request_begin_play(); yield 5
    worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
    assert len(worlds)==2, 'Expected two PIE worlds'
    cw,sw=sorted(worlds,key=lambda w:len(actors(w,unreal.PillowWarsPlayerController)))
    host=next(c for c in actors(sw,unreal.PillowWarsPlayerController) if c.is_local_controller())
    client=next(c for c in actors(cw,unreal.PillowWarsPlayerController) if c.is_local_controller())
    remote=next(c for c in actors(sw,unreal.PillowWarsPlayerController) if not c.is_local_controller())
    check('two possessed players',all(c.get_controlled_pawn() for c in [host,client]),[str(c.get_local_screen()) for c in [host,client]])
    key(host,'Enter');key(client,'Enter');yield .7
    key(host,'H');yield .7
    check('host cannot start unready lobby','PLAYING' not in phase(sw).upper(),phase(sw))
    key(host,'Enter');key(client,'Enter');yield .7
    key(host,'H');yield 6
    check('ready lobby loads both into game',all('ingame' in str(c.get_local_screen()).lower().replace('_','') for c in [host,client]),[str(c.get_local_screen()) for c in [host,client]])
    for label,pc,serverpc in [('host',host,host),('client',client,remote)]:
        p=place(serverpc,unreal.Vector(0,0,350)); yield .6
        before=pos(p); key(pc,'W',.5);yield .8
        check(label+' W input movement',dist(pos(p),before)>80,dict(cm=dist(pos(p),before)))
        before=pos(p); key(pc,'SpaceBar',.12);yield .2
        check(label+' jump input',pos(p).z>before.z+15,dict(rise=pos(p).z-before.z));yield 1
    # Stable pile positions and no global idle refill.
    hp=place(host,unreal.Vector(0,0,350)); place(remote,unreal.Vector(500,0,350));yield .7
    piles=actors(sw,unreal.PillowWarsStuffingPickup)
    saved=[(p,xyz(pos(p))) for p in piles]
    for p in piles:p.set_actor_location(unreal.Vector(900,400+80*piles.index(p),300),False,True)
    initial=[(p,xyz(pos(p))) for p in piles]
    state(host).set_editor_property('stuffing',30.0); yield 1.5
    check('no refill away from feathers',abs(prop(state(host),'stuffing')-30)<.1,snapshot(sw))
    check('pile roots stay fixed',all(xyz(pos(p))==v for p,v in initial),[xyz(pos(p)) for p in piles])
    check('three to four ambient piles',3<=len(piles)<=4,len(piles))
    pile=piles[0]; pile.set_actor_location(pos(hp)+unreal.Vector(45,0,-hp.capsule_component.get_scaled_capsule_half_height()),False,True)
    yield 1
    check('nearby pile refills with rest animation',prop(state(host),'stuffing')>35 and prop(state(host),'stuffing_resting'),snapshot(sw))
    yield .4
    check('refill flag replicates',snapshot(cw).get(state(host).get_player_name(),{}).get('rest',False),snapshot(cw))
    key(host,'W',.35);yield .25
    check('movement cancels stuffing',not prop(state(host),'stuffing_resting'),snapshot(sw));yield .8
    rp=place(remote,unreal.Vector(400,0,350));yield .6
    clientpile=piles[1]; clientpile.set_actor_location(pos(rp)+unreal.Vector(45,0,-rp.capsule_component.get_scaled_capsule_half_height()),False,True)
    state(remote).set_editor_property('stuffing',30.0);yield 1
    check('client refill and animation flag replicate',prop(state(remote),'stuffing')>40 and prop(state(client),'stuffing_resting'),dict(server=snapshot(sw),client=snapshot(cw)))
    key(client,'F',.7);yield .35
    check('charging cancels refill',not prop(state(remote),'stuffing_resting'),snapshot(sw));yield 1
    for p in actors(sw,unreal.PillowWarsStuffingPickup):p.set_actor_location(unreal.Vector(900,700,300),False,True)
    # Each attacker uses real F key press/release. Set positions only for range.
    for label,pc,serverpc,victimpc in [('host',host,host,remote),('client',client,remote,host)]:
        for hold in [.35,2.1]:
            attacker=place(serverpc,unreal.Vector(0,0,350)); victim=place(victimpc,unreal.Vector(110,0,350),180)
            state(serverpc).set_editor_property('stuffing',100.0)
            yield .7
            w=weapon(sw,attacker); before=prop(state(victimpc),'round_hits_taken');start=prop(w,'attack_sequence');daze_before=prop(state(victimpc),'daze')
            key(pc,'F',hold)
            charged=[]
            for i in range(3):
                yield hold/4
                mesh=w.get_component_by_class(unreal.PoseableMeshComponent)
                charged.append(dict(grip=prop(w,'grip_error'),pillow=xyz(mesh.get_bone_location_by_name('Pillow',unreal.BoneSpaces.COMPONENT_SPACE)),charge=prop(w,'charge_start_time'),hits=prop(state(victimpc),'round_hits_taken')))
            check(label+f' {hold}s no contact while charging',all(x['hits']==before for x in charged),charged)
            yield hold/4+.75
            ss=snapshot(sw);cs=snapshot(cw); name=state(victimpc).get_player_name()
            check(label+f' {hold}s released uppercut hits once',prop(state(victimpc),'round_hits_taken')==before+1,dict(server=ss,client=cs,attack=prop(w,'attack_variant'),power=prop(w,'charge_power')))
            check(label+f' {hold}s replicated Daze',abs(ss[name]['daze']-cs[name]['daze'])<.1 and ss[name]['taken']==cs[name]['taken'],dict(server=ss[name],client=cs[name]))
            samples.append(dict(attacker=label,hold=hold,daze=ss[name]['daze']-daze_before,charged=charged));save()
            check(label+f' {hold}s reachable grip',max(x['grip'] for x in charged)<1,charged)
            check(label+f' {hold}s recovery grip',prop(w,'grip_error')<1,prop(w,'grip_error'))
            yield .5
        values=[s for s in samples if s['attacker']==label]
        check(label+' full charge stronger than short',values[-1]['daze']>values[-2]['daze']*1.7,[x['daze'] for x in values])
    # Miss and repeated input must not add reactions or bypass cooldown.
    p=place(remote,unreal.Vector(-500,0,350));place(host,unreal.Vector(500,0,350));yield .7
    before=prop(state(host),'round_hits_taken');w=weapon(sw,p);seq=prop(w,'attack_sequence')
    for i in range(4):key(client,'F',.04);yield .1
    yield .6
    check('miss has no victim hit',before==prop(state(host),'round_hits_taken'),snapshot(sw))
    check('rapid input respects cooldown',prop(w,'attack_sequence')-seq<=1,prop(w,'attack_sequence')-seq)
    # Normal E creates cover; controlled scale/placement creates a thin scenery-block fixture.
    place(host,unreal.Vector(0,0,350));place(remote,unreal.Vector(500,0,350));yield .6
    state(host).set_editor_property('stuffing',100.0);key(host,'E');yield .4
    covers=actors(sw,unreal.PillowWarsCover)
    check('E places replicated pillow cover',len(covers)>0 and len(actors(cw,unreal.PillowWarsCover))>0,len(covers))
    if covers:
        blocker=covers[0];blocker.set_actor_scale3d(unreal.Vector(.12,4,4));blocker.set_actor_location(unreal.Vector(55,0,350),False,True)
        place(remote,unreal.Vector(110,0,350),180);yield .4
        before=prop(state(remote),'round_hits_taken');key(host,'F',.1);yield .9
        check('blocking geometry prevents melee through it',prop(state(remote),'round_hits_taken')==before,snapshot(sw))
        blocker.destroy_actor()
    place(host,unreal.Vector(0,0,350));place(remote,unreal.Vector(110,0,350),180);yield .6
    state(remote).set_editor_property('stuffing',100.0);before=prop(state(remote),'round_guards')
    key(client,'G',1.0);yield .15;key(host,'F',.1);yield .8
    check('client facing guard absorbs host attack',prop(state(remote),'round_guards')==before+1,dict(guards=prop(state(remote),'round_guards'),stuffing=prop(state(remote),'stuffing')));yield .4
    # Move during a charge, then throw via the client input path. Track actual projectile gravity.
    p=place(remote,unreal.Vector(-500,0,350));yield .5
    before=pos(p);key(client,'W',.45);key(client,'F',.6);yield .8
    check('client can move during charge',dist(pos(p),before)>60,dist(pos(p),before));yield .7
    key(client,'Q');yield .56
    projectiles=actors(sw,unreal.PillowWarsProjectile)
    if projectiles:
        projectile=projectiles[0];v1=projectile.get_velocity();a=pos(projectile);yield .13
        if unreal.SystemLibrary.is_valid(projectile):
            v2=projectile.get_velocity();b=pos(projectile)
            check('thrown pillow accelerates downward',v2.z<v1.z-50,dict(start=xyz(a),end=xyz(b),vz=[v1.z,v2.z]))
        else:check('thrown pillow accelerates downward',False,'Projectile hit scenery before second sample')
    else:check('throw input spawns projectile',False,'No projectile after release sample')
    tw=weapon(sw,p);ts=prop(tw,'throw_sequence');key(client,'Q');yield .6
    check('throw cannot bypass 15s recharge',prop(tw,'throw_sequence')==ts and prop(tw,'throw_ready_time')>unreal.GameplayStatics.get_time_seconds(sw),prop(tw,'throw_sequence'))
    # Deliberate boundary fixture, not claim of a combat-caused ring out.
    remote.get_controlled_pawn().set_actor_location(unreal.Vector(0,0,-780),False,True);yield 1
    sr=prop(actors(sw,unreal.PillowWarsMatchState)[0],'result');cr=prop(actors(cw,unreal.PillowWarsMatchState)[0],'result')
    check('elimination and winner agree',bool(sr) and sr==cr and prop(state(remote),'eliminated'),dict(server=sr,client=cr))
    key(client,'R');yield .6
    check('one client rematch vote does not force reset',bool(prop(actors(sw,unreal.PillowWarsMatchState)[0],'result')),str(prop(actors(sw,unreal.PillowWarsMatchState)[0],'result')))
    key(host,'R');yield 1.5
    check('host reset respawns both',all(c.get_controlled_pawn() for c in [host,remote,client]) and not prop(actors(sw,unreal.PillowWarsMatchState)[0],'result') and not prop(actors(cw,unreal.PillowWarsMatchState)[0],'result'),[str(c.get_local_screen()) for c in [host,client]])
    yield .5
def finish(error=None):
    save(error);unreal.log('PW_REVIEW_COMPLETE '+str(error))
    levels.editor_request_end_play();unreal.PillowWarsEditorTestLibrary.restore_pie()
    unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False)
script=flow(); due=0
def tick(dt):
    global due,busy
    if busy or time.monotonic()<due:return
    busy=True
    try:
        if time.monotonic()-started>200:raise RuntimeError('Test timeout')
        due=time.monotonic()+next(script)
    except StopIteration:finish()
    except Exception:finish(traceback.format_exc())
    finally:busy=False
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle=unreal.register_slate_post_tick_callback(tick)
