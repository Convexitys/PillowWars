"""Real PIE input and replication; explicit source/position/resource fixtures.
Join parser checks do not claim successful network travel to a custom port.
"""
import unreal,time,json,traceback
from pathlib import Path
OUT=Path(__file__).parent;rows=[];handle=None;busy=False;due=0;started=time.monotonic()
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);levels.load_level('/Game/PillowWars/Maps/PillowWarsBedroomPolished')
def actors(w,c):return unreal.GameplayStatics.get_all_actors_of_class(w,c)
def prop(o,n):return o.get_editor_property(n)
def key(pc,k,hold=.08):unreal.PillowWarsEditorTestLibrary.queue_player_key(pc,k,hold)
def check(n,ok,d):rows.append(dict(test=n,passed=bool(ok),detail=d));unreal.log('PW_EMAIL_TEST '+json.dumps(rows[-1]))
def place(pc,x,y=0,yaw=0):
    p=pc.get_controlled_pawn();p.get_movement_component().stop_movement_immediately();p.get_movement_component().set_movement_mode(unreal.MovementMode.MOVE_WALKING);p.set_actor_location(unreal.Vector(x,y,350),False,True);p.set_actor_rotation(unreal.Rotator(0,yaw,0),True);pc.set_control_rotation(unreal.Rotator(0,yaw,0));return p
def weapon(w,pc):return next(a for a in actors(w,unreal.PillowWarsWeapon) if a.get_owner()==pc.get_controlled_pawn())
def clear(w):
    for c in actors(w,unreal.PillowWarsCover):c.destroy_actor()
    for p in actors(w,unreal.PillowWarsStuffingPickup):p.destroy_actor()
def fixture(pc):
    unreal.PillowWarsEditorTestLibrary.resource_fixture(pc,100,True);pc.player_state.set_editor_property('health',100.);pc.player_state.set_editor_property('daze',0.)
def flow():
    unreal.PillowWarsEditorTestLibrary.configure_pie(2);levels.editor_request_begin_play();yield 7
    humans=lambda w:[c for c in actors(w,unreal.PillowWarsPlayerController) if not isinstance(c,unreal.PillowWarsBotController)]
    cw,sw=sorted(unreal.EditorLevelLibrary.get_pie_worlds(False),key=lambda w:len(humans(w)))
    host=next(c for c in humans(sw) if c.is_local_controller());client=humans(cw)[0];remote=next(c for c in humans(sw) if not c.is_local_controller())
    # Actual Join Party key editor, before any match. No valid Enter/network travel.
    key(host,'Down');yield .2;key(host,'Enter');yield .3
    key(host,'Tab');yield .2
    for k in ['Seven','Seven','Seven','Eight']:key(host,k,0);yield .1
    yield .3
    a=host.get_join_address();check('Join input constructs IPv4:7778',a=='127.0.0.1:7778' and unreal.PillowWarsPlayerController.is_valid_join_address(a),a)
    for _ in range(4):key(host,'BackSpace',0)
    yield .2;check('empty port rejected',not unreal.PillowWarsPlayerController.is_valid_join_address(host.get_join_address()),host.get_join_address())
    key(host,'Enter');yield .2;check('invalid Enter remains in Join Party','JOIN' in str(host.get_local_screen()).upper(),str(host.get_local_screen()))
    for k in ['Six','Five','Five','Three','Six']:key(host,k,0);yield .1
    yield .2;check('65536 rejected after ordinary input',host.get_join_address()=='127.0.0.1:65536' and not unreal.PillowWarsPlayerController.is_valid_join_address(host.get_join_address()),host.get_join_address())
    for _ in range(6):key(host,'BackSpace',0)
    yield .2;check('deletion across separator preserves default IPv4',host.get_join_address()=='127.0.0.1' and unreal.PillowWarsPlayerController.is_valid_join_address(host.get_join_address()),host.get_join_address())
    for address,valid in [('192.168.1.20',True),('192.168.1.20:1',True),('192.168.1.20:65535',True),('256.1.1.1',False),('1.2.3.4:0',False),('1.2.3.4::7778',False),('1.2.3',False)]:
        check('native parser '+address,unreal.PillowWarsPlayerController.is_valid_join_address(address)==valid,valid)
    key(host,'Escape');yield .2;check('Join Escape returns main menu','MAIN' in str(host.get_local_screen()).upper(),str(host.get_local_screen()))
    key(host,'Enter');key(client,'Enter');yield .5
    key(host,'V');yield .3;check('lobby V retains character selection','CHARACTER' in str(host.get_local_screen()).upper(),str(host.get_local_screen()));key(host,'Enter');yield .3
    key(host,'F6');yield .5;bot=actors(sw,unreal.PillowWarsBotController)[0];bot.set_editor_property('thinking_enabled',False)
    key(host,'Enter');key(client,'Enter');yield .4;key(host,'F8');yield 5
    place(bot,1400,700);clear(sw)
    for label,pc,authority in [('host',host,host),('client',client,remote)]:
        clear(sw);place(authority,-400,-500);fixture(authority);yield .5;w=weapon(sw,authority);variant=prop(w,'variant_index')
        key(pc,'E');yield .5;check(label+' creates one cover',len(actors(sw,unreal.PillowWarsCover))==1,len(actors(sw,unreal.PillowWarsCover)))
        key(pc,'V');yield .5
        check(label+' V reclaims without appearance cycle',not actors(sw,unreal.PillowWarsCover) and prop(w,'variant_index')==variant,dict(variant=prop(w,'variant_index'),expected=variant))
        clear(sw);before=prop(authority.player_state,'stuffing');key(pc,'V');yield .3
        check(label+' V without cover no cycle or credit',prop(w,'variant_index')==variant and prop(authority.player_state,'stuffing')==before,dict(stuffing=prop(authority.player_state,'stuffing'),variant=prop(w,'variant_index')))
        key(pc,'B');yield .3;check(label+' B explicitly cycles without cover',prop(w,'variant_index')!=(variant) and not actors(sw,unreal.PillowWarsCover),prop(w,'variant_index'))
        fixture(authority);seq=prop(w,'attack_sequence');key(pc,'Escape');yield .3
        for k in ['F','Gamepad_FaceButton_Left']:key(pc,k,.3);yield .5
        check(label+' Pause F and gamepad X do not attack',prop(w,'attack_sequence')==seq and prop(w,'charge_start_time')<0 and abs(prop(authority.player_state,'stuffing')-100)<.01,dict(sequence=prop(w,'attack_sequence'),charge=prop(w,'charge_start_time'),stuffing=prop(authority.player_state,'stuffing')))
        key(pc,'Down');yield .2;key(pc,'Enter');yield .3
        for k in ['F','Gamepad_FaceButton_Left']:key(pc,k,.3);yield .5
        key(pc,'V');key(pc,'B');yield .2
        check(label+' Settings from Pause blocks attack/cycle/reclaim',prop(w,'attack_sequence')==seq and prop(w,'charge_start_time')<0 and abs(prop(authority.player_state,'stuffing')-100)<.01,dict(screen=str(pc.get_local_screen()),sequence=prop(w,'attack_sequence')))
        key(pc,'Escape');yield .2;key(pc,'Escape');yield .3
        key(pc,'F',1.5);yield .3;check(label+' normal charge begins',prop(w,'charge_start_time')>=0,prop(w,'charge_start_time'))
        key(pc,'Escape');yield .35
        check(label+' Pause cancels held charge without payment',prop(w,'charge_start_time')<0 and prop(w,'attack_sequence')==seq and abs(prop(authority.player_state,'stuffing')-100)<.01,dict(charge=prop(w,'charge_start_time'),sequence=prop(w,'attack_sequence')))
        key(pc,'Escape');yield 1.3
        check(label+' late cancelled physical release causes no resumed swing',prop(w,'attack_sequence')==seq and abs(prop(authority.player_state,'stuffing')-100)<.01,dict(sequence=prop(w,'attack_sequence'),stuffing=prop(authority.player_state,'stuffing')))
        key(pc,'F');yield .85;check(label+' fresh F press attacks after resume',prop(w,'attack_sequence')==seq+1 and abs(prop(authority.player_state,'stuffing')-92)<.01,dict(sequence=prop(w,'attack_sequence'),stuffing=prop(authority.player_state,'stuffing')))
        check(label+' server owner state reaches client',abs(prop(pc.player_state,'stuffing')-prop(authority.player_state,'stuffing'))<.01,dict(local=prop(pc.player_state,'stuffing'),authority=prop(authority.player_state,'stuffing')))
    # Editor fixtures for direction cases: receiver uses real authoritative code.
    clear(sw);place(bot,1400,700)
    for shooter_x in [500,-500]:
        for facing,blocked in [(0,True),(180,False)]:
            place(host,0,0,facing);place(remote,shooter_x,400);fixture(host);fixture(remote);yield .8
            key(host,'G',.9);yield .2;guards=prop(host.player_state,'round_guards')
            origin=host.get_controlled_pawn().get_actor_location()
            ok=unreal.PillowWarsEditorTestLibrary.projectile_contact_fixture(remote,host.get_controlled_pawn(),unreal.Vector(-1,0,0));yield .12
            v=host.get_controlled_pawn().get_velocity()
            delta=host.get_controlled_pawn().get_actor_location()-origin
            check('projectile direction shooter '+str(shooter_x)+' facing '+str(facing),ok and (prop(host.player_state,'round_guards')>guards)==blocked and delta.x<0 and v.x<=0,dict(guards=prop(host.player_state,'round_guards')-guards,velocity=[v.x,v.y,v.z],displacement=[delta.x,delta.y,delta.z]))
            yield .9
    for direction in [unreal.Vector(0,0,-1),unreal.Vector(0,0,0)]:
        place(host,0,0);place(remote,-500,400);fixture(host);yield .8;key(host,'G',.6);yield .2;guards=prop(host.player_state,'round_guards')
        ok=unreal.PillowWarsEditorTestLibrary.projectile_contact_fixture(remote,host.get_controlled_pawn(),direction);yield .12;v=host.get_controlled_pawn().get_velocity()
        check('vertical/zero projectile has no invented horizontal guard or launch '+str(direction),ok and prop(host.player_state,'round_guards')==guards and abs(v.x)<1 and abs(v.y)<1,dict(velocity=[v.x,v.y,v.z],guards=prop(host.player_state,'round_guards')-guards));yield .6
    # Actual projectile collision, with shooter already across victim behind flight.
    place(host,0,0);place(remote,-500,400);fixture(host);fixture(remote);yield .8;key(host,'G',1);yield .2
    guards=prop(host.player_state,'round_guards');hits=prop(host.player_state,'round_hits_taken')
    projectile=unreal.PillowWarsEditorTestLibrary.projectile_flight_fixture(remote,unreal.Vector(220,0,360),unreal.Vector(-1,0,0));yield .4
    check('actual projectile OnImpact guards by flight not owner location',prop(host.player_state,'round_hits_taken')==hits+1 and prop(host.player_state,'round_guards')==guards+1,dict(hits=prop(host.player_state,'round_hits_taken')-hits,guards=prop(host.player_state,'round_guards')-guards));yield .7
    # Registered paid-pile fixtures, moved only for eligibility boundary testing.
    clear(sw);place(bot,500,500);fixture(bot);unreal.PillowWarsEditorTestLibrary.resource_fixture(bot,20,True)
    high=unreal.PillowWarsEditorTestLibrary.resource_pile_fixture(bot,unreal.Vector(540,500,350),8);assert high;high.set_actor_location(unreal.Vector(550,500,550),False,True)
    low=unreal.PillowWarsEditorTestLibrary.resource_pile_fixture(bot,unreal.Vector(500,760,350),8);assert low
    unreal.PillowWarsEditorTestLibrary.bot_think_fixture(bot)
    check('bot chooses eligible alternative instead of visible high pile','Seeking' in prop(bot,'decision') and prop(bot,'steering').y>0,dict(decision=prop(bot,'decision'),steering=str(prop(bot,'steering'))))
    low.destroy_actor();unreal.PillowWarsEditorTestLibrary.bot_think_fixture(bot)
    check('bot falls back rather than restuff at ineligible high pile','feathers' not in prop(bot,'decision'),prop(bot,'decision'))
    high.destroy_actor();low=unreal.PillowWarsEditorTestLibrary.resource_pile_fixture(bot,unreal.Vector(500,500,350),8);assert low
    unreal.PillowWarsEditorTestLibrary.bot_think_fixture(bot);before=prop(bot.player_state,'stuffing');rest_decision=prop(bot,'decision');yield 2
    check('same-level bot rest transfers registered finite pile',prop(bot.player_state,'stuffing')>before and 'Restuffing' in rest_decision,dict(before=before,after=prop(bot.player_state,'stuffing'),selected_decision=rest_decision))
    gm=actors(sw,unreal.PillowWarsGameMode)[0];check('focused fixtures preserve resource conservation',json.loads(gm.get_resource_ledger_snapshot())['residual']==0,gm.get_resource_ledger_snapshot())
def finish(error=None):
    (OUT/'input-results.json').write_text(json.dumps(dict(results=rows,error=error,configuration='UE5.5.3 listen host, client and server bot in one editor process; normal queued input; scripted positions/resources; direction receiver and bot decision fixtures explicitly injected; one actual projectile collision'),indent=2));levels.editor_request_end_play();unreal.PillowWarsEditorTestLibrary.restore_pie();unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False);unreal.SystemLibrary.quit_editor()
script=flow()
def tick(dt):
    global busy,due
    if busy or time.monotonic()<due:return
    busy=True
    try:
        if time.monotonic()-started>230:raise RuntimeError('Input tests timeout')
        due=time.monotonic()+next(script)
    except StopIteration:finish()
    except Exception:finish(traceback.format_exc())
    finally:busy=False
unreal.EditorPythonScripting.set_keep_python_script_alive(True);handle=unreal.register_slate_post_tick_callback(tick)
