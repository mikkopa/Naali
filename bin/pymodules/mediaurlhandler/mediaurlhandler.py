import rexviewer as r
import PythonQt

from circuits import Component

INTERNAL = 1

class WebviewCanvas:
    def __init__(self, urlstring, refreshrate):
        self.canvas = r.createCanvas(INTERNAL)
        self.webview = PythonQt.QtWebKit.QWebView()
        self.canvas.AddWidget(self.webview)
        
        url = PythonQt.QtCore.QUrl(urlstring)
        self.webview.load(url)

        self.refreshrate = refreshrate #XXX refreshing not implemented yet. or is it automatic with webviews and uicanvases?-o

class MediaURLHandler(Component):
    def __init__(self):
        Component.__init__(self)
        #which texture uuids with mediaurl are in which canvas impl
        self.texture2canvas = {}

        """
        the code below was to test webviews first as 2d widgets. 
        perhaps useful later when add feat to open from 3d canvas to 2d usage
        """
        #uism = r.getUiSceneManager()
        #uiprops = r.createUiWidgetProperty()
        #uiprops.widget_name_ = "MediaURL"
        ##uiprops.my_size_ = QSize(width, height)
        ##self.proxywidget = uism.AddWidgetToScene(ui, uiprops)
        #self.proxywidget = r.createUiProxyWidget(self.wv, uiprops)
        ##print widget, dir(widget)
        #uism.AddProxyWidget(self.proxywidget)
        #self.wv.show()

    def on_genericmessage(self, name, data):
        #print "MediaURLHandler got Generic Message:", name, data
        if name == "RexMediaUrl":
            print "MediaURLHandler got data:", data
            textureuuid, urlstring, refreshrate = data

            wc = WebviewCanvas(urlstring, refreshrate)
            
            #for objects we already had in the scene
            r.applyUICanvasToSubmeshesWithTexture(wc.canvas, textureuuid)
            
            #for when get visuals_modified events later, 
            #e.g. for newly downloaded objects
            self.texture2canvas[textureuuid] = wc
                          
    def on_entity_visuals_modified(self, entid):
        #print "MediaURLHandler got Visual Modified for:", entid
        #XXX add checks to not re-apply blindly when is already up-to-date!
        for tx, wc in self.texture2canvas.iteritems():
            submeshes = r.getSubmeshesWithTexture(entid, tx)
            if submeshes:
                print "Modified entity uses a known mediaurl texture:", entid, tx, submeshes, wc
                r.applyUICanvasToSubmeshes(entid, submeshes, wc.canvas)
        
    def on_keydown(self, key, mods, callback):
        if key == 46: #C
            for tx, wc in self.texture2canvas.iteritems():
                r.applyUICanvasToSubmeshesWithTexture(wc.canvas, tx)
