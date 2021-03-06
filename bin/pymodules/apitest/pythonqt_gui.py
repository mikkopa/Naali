import rexviewer as r
import PythonQt
from PythonQt.QtGui import QGroupBox, QVBoxLayout, QPushButton, QLineEdit, QInputDialog 

from circuits import Component

#INTERNAL = 1
#EXTERNAL = 0

#print dir(PythonQt)
UiWidgetProperties = PythonQt.__dict__['UiServices::UiWidgetProperties']

class TestGui(Component):
    def __init__(self):
        Component.__init__(self)
        #loader = QUiLoader() 
        uism = r.getUiSceneManager() #self.canvas = r.createCanvas(EXTERNAL)

        #group = QGroupBox()
        #box = QVBoxLayout(group)
        #button = QPushButton(group)
        #box.addWidget(button)
        #group.show()
        #self.canvas.AddWidget(group)

        lineedit = QLineEdit()
        self.widget = lineedit
        #lineedit.show()
        #uiprops = UiWidgetProperties("PythonQt Test")
        #uism.AddWidgetToScene(lineedit) #uiprops) #self.canvas.AddWidget(lineedit)

        uism = r.getUiSceneManager()
        uiprops = r.createUiWidgetProperty()
        uiprops.widget_name_ = "Test Widget"
        #uiprops.my_size_ = QSize(width, height)
        #self.proxywidget = uism.AddWidgetToScene(ui, uiprops)
        self.proxywidget = r.createUiProxyWidget(self.widget, uiprops)
        ##print widget, dir(widget)
        if not uism.AddProxyWidget(self.proxywidget):
            print "Adding the ProxyWidget to the bar failed."

        #self.canvas.Show()

        #QInputDialog.getText(None, "Mesh asset UUID", "Please give mesh asset UUID", QLineEdit.Normal, "")

        lineedit.connect('textChanged(QString)', self.changed)

        r.logInfo("TestGui inited.")

    def changed(self, text):
        r.logInfo("TestGui LineEdit changed: %s" % text)
                
    def on_exit(self):
        r.logInfo("TestGui starting exit...")
        
        #qtmod = r.getQtModule()
        #qtmod.DeleteCanvas(self.canvas)

        uism = r.getUiSceneManager()
        uism.RemoveProxyWidgetFromScene(self.proxywidget)
        r.logInfo("TestGui proxywidget removed from scene.")
