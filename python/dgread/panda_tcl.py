import Tkinter
import dgread
import numpy as np
import pandas as pd

script = '''
set env(DLSH_LIBRARY) /Users/sheinb/anaconda/lib/dlsh
package require dlsh
set n 100000
set g [dg_create]
dl_set $g:gender [dl_choose [dl_slist f m] [dl_irand $n 2]]
dl_set $g:class [dl_repeat "0 1" [expr $n/2]]
dl_set $g:rates [dl_add [dl_zrand $n] [dl_mult $g:class 0.2]]
dg_toString64 $g $g
return $g
'''

t = Tkinter.Tcl()
g = t.eval(script)
pygroup = dgread.fromString64(t.getvar(g))
df = pd.DataFrame(pygroup)
gender_class = pd.pivot_table(df,values='rates',
                              rows=['gender','class'],aggfunc=np.mean)
fig = gender_class.unstack().plot(kind='bar', title='Panda Chart')
print(gender_class.unstack())
