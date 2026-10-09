"""Repeat the preserved two-player regression on the polished map, adding tap attacks."""
from pathlib import Path
import unreal
root=Path(__file__).resolve().parents[2]
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level('/Game/PillowWars/Maps/PillowWarsBedroomPolished')
source=(root/'work/windup_review/verify.py').read_text()
extra='''    # Each of the three tap variants uses the actual host/client F input path.
    for label,pc,serverpc,victimpc in [('host',host,host,remote),('client',client,remote,host)]:
        seen=[]
        for index in range(3):
            attacker=place(serverpc,unreal.Vector(0,0,350));place(victimpc,unreal.Vector(110,0,350),180)
            state(serverpc).set_editor_property('stuffing',100.0)
            yield .7
            before=prop(state(victimpc),'round_hits_taken');w=weapon(sw,attacker)
            key(pc,'F',.08);yield .85
            variant=prop(w,'attack_variant');seen.append(variant)
            ss=snapshot(sw);cs=snapshot(cw);name=state(victimpc).get_player_name()
            check(label+' tap variant '+str(variant)+' one hit and agreement',prop(state(victimpc),'round_hits_taken')==before+1 and abs(ss[name]['daze']-cs[name]['daze'])<.1 and ss[name]['taken']==cs[name]['taken'],dict(server=ss[name],client=cs[name]))
        check(label+' cycles all three tap variants',sorted(seen)==[0,1,2],seen)
'''
source=source.replace('    # Miss and repeated input',extra+'    # Miss and repeated input')
begin=source.index("    key(client,'Q');yield .56")
end=source.index('    tw=weapon(sw,p);',begin)
source=source[:begin]+'''    # Poll for the replicated input's release rather than assume a fixed RPC latency.
    place(host,unreal.Vector(0,600,350));p=place(remote,unreal.Vector(-100,0,350));yield .7
    key(client,'Q')
    projectiles=[]
    for attempt in range(35):
        yield .04
        projectiles=actors(sw,unreal.PillowWarsProjectile)
        if projectiles:break
    check('throw input spawns projectile',bool(projectiles),dict(polls=attempt+1))
    if projectiles:
        projectile=projectiles[0];v1=projectile.get_velocity();a=pos(projectile);yield .13
        if unreal.SystemLibrary.is_valid(projectile):
            v2=projectile.get_velocity();b=pos(projectile)
            check('thrown pillow accelerates downward',v2.z<v1.z-50,dict(start=xyz(a),end=xyz(b),vz=[v1.z,v2.z]))
        else:check('thrown pillow accelerates downward',False,'Projectile hit before second sample')
''' +source[end:]
exec(compile(source,str(root/'work/windup_review/verify.py'),'exec'),globals())
