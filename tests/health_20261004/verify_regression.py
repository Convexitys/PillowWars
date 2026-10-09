"""Retest existing movement, swings, stuffing, cover, guard, throw and networking.
Health is restored only when setting an isolated combat positioning fixture,
so prior animation tests are not interrupted by the new intended KO rules.
"""
from pathlib import Path
import unreal
root=Path(__file__).resolve().parents[2]
source=(root/'work/polish_release/verify.py').read_text()
needle="source=(root/'work/windup_review/verify.py').read_text()"
old='    pc.set_control_rotation(unreal.Rotator(0,yaw,0)); return p'
new='    pc.set_control_rotation(unreal.Rotator(0,yaw,0)); state(pc).set_editor_property("health",100.0); state(pc).force_net_update(); return p'
stop='    unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False)'
extra='\nsource=source.replace('+repr(old)+','+repr(new)+')\nsource=source.replace('+repr(stop)+','+repr(stop+';unreal.SystemLibrary.quit_editor()')+')\n'
first="    check('two possessed players'"
wait="    for possession_poll in range(50):\n        if all(c.get_controlled_pawn() for c in [host,client]):break\n        yield .1\n"+first
extra+='source=source.replace('+repr(first)+','+repr(wait)+')\n'
source=source.replace(needle,needle+extra)
exec(compile(source,str(__file__),'exec'),globals())
