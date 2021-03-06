"""
an experiment in overriding default behaviour by a qtscript defined handler,
using qt signals and properties. for the problem statement see
http://wiki.realxtend.org/index.php/Qt_Script_as_a_means_of_extending_and_overriding_internal_Naali_behavior

NOTE: doesn't disconnect the default handler when a new one is added.
do browsers do that when you define e.g. myimg.onclick via DOM in js?
how should we do it? an explicit disconnect would not be that nice..
DOM has also the concept of 'bubbling', i.e. whether you want other handlers work too.
"""

from PyQt4 import QtCore, QtScript
from PyQt4.QtCore import QObject

class SceneEntity(QObject):
    """
    a dummy entity that's in a scene and which updates.
    has he update call as a qt signal,
    and the position as a qt property,
    so qtscript can override.
    
    in the actual viewer we have decoupled passive entities which are just data,
    and active controllers etc - most certainly want to keep that, so might be
    a good idea to refactor this to reflect that idea.
    """
    
    update = QtCore.pyqtSignal()
    
    def __init__(self):
        QObject.__init__(self)
        self._pos = 0.0        
        self.update.connect(self.default_update)
        
    def default_update(self):
        pass
        
    """pos as a qt property so that qtscript code can see and modify it"""
    def get_pos(self):
        return self._pos
    def set_pos(self, val):
        self._pos = val
    pos = QtCore.pyqtProperty("float", get_pos, set_pos)
        
class Avatar(SceneEntity):
    def default_update(self):
        self.pos += 0.1 #moves at constant speed so we can see whether cam tracking really works
        
av = Avatar()

class Camera(SceneEntity):    
    def default_update(self):
        self.pos = av.pos - 1.0 #default cam behav: keeps 1.0 behind av
        
cam = Camera()

engine = QtScript.QScriptEngine()

def debugprint(ctx, eng): #for javascript to print to console
    print ctx.argument(0).toString()
    return QtScript.QScriptValue(eng, 0)

"""the javascript source of the custom camera code - keeps 0.5 behind av"""
jscam = """
function mycam() {
    //print("MYCAM: " + avatar.pos + " - " + camera.pos);
    camera.pos = avatar.pos - 0.5; 
}

camera.update.connect(mycam);
print("MYCAM: connected a custom code to control camera.");
"""

eprop = engine.globalObject().setProperty
eprop("avatar", engine.newQObject(av))
eprop("camera", engine.newQObject(cam))
eprop("print", engine.newFunction(debugprint))

def update(): #we probably don't want such individual calls per entity .. or do we?
    av.update.emit()
    cam.update.emit()

def checkcamdist(name, target):
    d = av.pos - cam.pos
    print "CHECK CAM DIST: cam distance from av is %f (posses: %f - %f)" % (d, av.pos, cam.pos)
    assert (target - 0.01) < d < (target + 0.01) #with an epsilon of 0.01 for float inaccuracy
    print "SUCCESS: %s cam is keeping cam within %f from av" % (name, target)    

def test_custom_cam():
    """let's say the default behaviour is that cam follows at -1.0"""
    for _ in range(3):
        update()
        checkcamdist("default", 1.0)
    
    """now let's change to a qtscripted behaviour that keeps at 0.5"""
    engine.evaluate(jscam)
    for _ in range(3):
        update()
        checkcamdist("custom", 0.5)
    
if __name__ == '__main__':
    test_custom_cam()
    
"""test passes now, outputs:
D:\k2\rex\viewer\trunk\QtScript>c:\Python26\python.exe customcam.py
CHECK CAM DIST: cam distance from av is 1.000000 (posses: 0.100000 - -0.900000)
SUCCESS: default cam is keeping cam within 1.000000 from av
CHECK CAM DIST: cam distance from av is 1.000000 (posses: 0.200000 - -0.800000)
SUCCESS: default cam is keeping cam within 1.000000 from av
CHECK CAM DIST: cam distance from av is 1.000000 (posses: 0.300000 - -0.700000)
SUCCESS: default cam is keeping cam within 1.000000 from av
MYCAM: connected a custom code to control camera.
CHECK CAM DIST: cam distance from av is 0.500000 (posses: 0.400000 - -0.100000)
SUCCESS: custom cam is keeping cam within 0.500000 from av
CHECK CAM DIST: cam distance from av is 0.500000 (posses: 0.500000 - 0.000000)
SUCCESS: custom cam is keeping cam within 0.500000 from av
CHECK CAM DIST: cam distance from av is 0.500000 (posses: 0.600000 - 0.100000)
SUCCESS: custom cam is keeping cam within 0.500000 from av
"""
