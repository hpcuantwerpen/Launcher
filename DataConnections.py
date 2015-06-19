
################################################################################
class ModelAccessAttribute(object):
################################################################################
    def __init__(self,model,attr):
        self.model = model
        self.attr  = attr
    def get(self):
        return getattr(self.model,self.attr)
    def set(self,value):
        setattr(self.model, self.attr, value)
        
################################################################################
class ModelAccessDict(object):
################################################################################
    def __init__(self,model,attr):
        self.model = model
        self.attr  = attr
    def get(self):
        return self.model[self.attr]
    def set(self,value):
        self.model[self.attr] = value

################################################################################
class ModelAccessGetSet(object):
################################################################################
    def __init__(self,model,attr):
        self.model = model
        get = eval('self.model.get_'+attr)
        self.get = get
        set = eval('self.model.set_'+attr)
        self.get = set
        
#     def get(self):
#         return self.model[self.attr].value
#     def set(self,value):
#         self.model[self.attr].value = value

################################################################################
class WidgetAccessValue(object):
################################################################################
    """widget has value property, value() and setValue() methods."""
    def __init__(self,widget):
        self.widget = widget
        if hasattr(widget, 'value'):
            self.get = self.widget.value
            self.set = self.widget.setValue
        elif hasattr(widget,'currentText'):
            self.get = self.widget.currentText
            self.set = self.widget.setCurrentText
        else:
            raise Exception("No widget_access implemented for widget "+str(type(widget)))
        
#     def get(self):
#         return self.widget.value()
#     def set(self,value):
#         self.widget.setValue(value)
    
################################################################################
class DataConnection(object):
################################################################################
    connections = {}
    SEND = 0
    RECV = 1
    @staticmethod
    def send_all(connection_list):
        for dc in DataConnection.connections[connection_list]:
            dc.send()
    @staticmethod
    def receive_all(connection_list):
        for dc in DataConnection.connections[connection_list]:
            dc.receive()
    @staticmethod
    def undo_all(connection_list):
        for dc in DataConnection.connections[connection_list]:
            dc.undo()
    
    def __init__(self,widget,model,attribute):
        assert isinstance(attribute,str)
        if hasattr(model,attribute):
            self.model_access = ModelAccessAttribute(model,attribute)
        elif hasattr(model,'get_'+attribute):
            self.model_access = ModelAccessGetSet(model,attribute)
        else:
            self.model_access = ModelAccessDict(model,attribute)
        self.widget_access = WidgetAccessValue(widget)
            
        self.prev   = None
        self.what   = None

    def send(self):
        self.what = DataConnection.SEND
        self.prev = self.model_access.get()
        self.model_access.set( self.widget_access.get())
        
    def receive(self):
        self.what = DataConnection.RECV
        self.prev = self.widget_access.get()
        self.widget_access.set(self.model_access.get())
        
    def undo(self):
        if self.what==DataConnection.SEND:
            self.model_access.set(self.prev)
        elif self.what==DataConnection.RECV:
            self.widget_access.set(self.prev)
            
################################################################################
def add_data_connection(widget,config,var_name,connection_list=None):
    if connection_list:
        dc = DataConnection(widget,config,var_name)
        if connection_list in DataConnection.connections:
            DataConnection.connections[connection_list].append(dc)
        else:
            DataConnection.connections[connection_list] = [dc]
    