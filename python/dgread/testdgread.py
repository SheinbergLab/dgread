import dgread
import numpy as np
from tkinter import *
t = Tcl()
t.eval("lappend auto_path /usr/local/lib")
t.eval("package require dlsh")
gname = t.eval("set gname [dg_create]")
t.eval("dl_set $gname:ints [dl_fromto 0 100]; dl_set $gname:floats [dl_zrand 100]")
t.eval("dg_toString64 $gname g")
g = t.getvar("g")
data = dgread.fromString64(g);
t.eval("dg_delete $gname; unset g")
print("%d %d" % (np.mean(data['ints']),  np.mean(data['floats'])))
print(data['floats'])
