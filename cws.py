"""
config_value<->Widget<->Script connections
"""
import cfg
from PyQt5.QtWidgets import QLineEdit,QComboBox,QSpinBox,QDoubleSpinBox,\
                            QCheckBox,QTextEdit
import script

################################################################################
def identity(v):
    return v

################################################################################
class CWS_QWidget(object):
################################################################################
    def __init__(self,config_value,widget,script,script_attribute):
        self.config_value = config_value
        self.widget = widget
        self.script = script
        self.s_attr = script_attribute
        self.c2w()
        self.c2s()
    def c2w(self):
        pass
    def c2s(self):
        pass
        
################################################################################
class CWS_QComboBox(CWS_QWidget):
################################################################################
    def __init__(self,config_value,widget,script,script_attribute):
        assert type(widget) is QComboBox
        super(CWS_QComboBox, self).__init__(config_value,widget,script,script_attribute)
    
    def c2w(self):
        if not self.config_value is None and not self.widget is None:
            self.widget.clear()
            self.widget.addItems      (self.config_value.choices)
            self.widget.setCurrentText(self.config_value.value)
        
    def w2c(self):
        if not self.config_value is None and not self.widget is None:
            self.config_value.set(self.widget.currentText())

    def w2s(self):
        if not self.widget is None and not self.s_attr is None:
            self.script[self.s_attr] = self.widget.currentText()
        
    def s2w(self):
        if not self.widget is None and not self.s_attr is None:
            self.widget.setCurrentText(self.script[self.s_attr])
            
    def c2s(self):
        if not self.config_value is None and not self.s_attr is None:
            self.script[self.s_attr] = self.config_value.value
    
    def s2c(self):
        if not self.config_value is None and not self.s_attr is None:
            self.config_value.set(self.script[self.s_attr])
        
################################################################################
class CWS_QSpinBox(CWS_QWidget):
################################################################################
    def __init__(self,config_value,widget,script,script_attribute
                ,  to_script=identity
                ,from_script=identity
                ):
        assert type(widget) in [QSpinBox,QDoubleSpinBox]
        self.  to_script =   to_script
        self.from_script = from_script
        super(CWS_QSpinBox, self).__init__(config_value,widget,script,script_attribute)
        
    def c2w(self):
        if not self.config_value is None and not self.widget is None:
            self.widget.setRange(self.config_value.choices[0],self.config_value.choices[-1])
            self.widget.setValue(self.config_value.value)
        
    def w2c(self):
        if not self.config_value is None and not self.widget is None:
            self.config_value.set(self.widget.value())

    def w2s(self):
        if not self.widget is None and not self.s_attr is None:
            v = self.widget.value()
            v = self.to_script(v)
            self.script[self.s_attr] = v
        
    def s2w(self):
        if not self.widget is None and not self.s_attr is None:
            v = self.script[self.s_attr]
            v = self.from_script(v)
            self.widget.setValue(v)
            
    def c2s(self):
        if not self.config_value is None and not self.s_attr is None:
            v = self.config_value.value
            v = self.to_script(v)
            self.script[self.s_attr] = v
    
    def s2c(self):
        if not self.config_value is None and not self.s_attr is None:
            v = self.script[self.s_attr]
            v = self.from_script(v)
            self.config_value.set(v)

################################################################################
class CWS_QCheckBox(CWS_QWidget):
################################################################################
    def __init__(self,config_value,widget,script,script_attribute
                ,  to_str=identity
                ,from_str=identity
                ):
        """
        to_str and from_str convert between the script representation and the 
        True/False values in the widget and the config_value. 
        """
        assert type(widget) is QCheckBox
        self.  to_str =   to_str
        self.from_str = from_str
        super(CWS_QCheckBox, self).__init__(config_value,widget,script,script_attribute)
        
    def c2w(self):
        if not self.config_value is None and not self.widget is None:
            v = self.from_str(self.config_value.value)
            self.widget.setChecked(v)
        
    def w2c(self):
        if not self.config_value is None and not self.widget is None:
            v = self.to_str(self.widget.checked(),self.config_value.get())
            self.config_value.set(v)

    def w2s(self):
        if not self.widget is None and not self.s_attr is None:
            v = self.to_str(self.widget.checked(),self.script[self.s_attr])
            self.script[self.s_attr] = v
        
    def s2w(self):
        if not self.widget is None and not self.s_attr is None:
            v = self.from_str(self.script[self.s_attr])
            self.widget.setValue(v)
            
    def c2s(self):
        if not self.config_value is None and not self.s_attr is None:
            v = self.config_value.value
            self.script[self.s_attr] = v
    
    def s2c(self):
        if not self.config_value is None and not self.s_attr is None:
            v = self.script[self.s_attr]
            self.config_value.set(v)
        
################################################################################
class CWS_QLineEdit(CWS_QWidget):
################################################################################
    def __init__(self,config_value,widget,script,script_attribute,to_str=str,from_str=str):
        """
        s = to_str(config_value.value)   # converts config_value.value to str
        config_value.value = from_str(s) # does the reverse conversion (possibly with loss of precision)
        The default values for to_str and from_str are ok when the config_value.value stores a string
        The script uses the text/value coming from the QlineEdit widget/config_value without modification.
        """
        assert type(widget) is QLineEdit
        self.to_str   =   to_str
        self.from_str = from_str
        super(CWS_QLineEdit, self).__init__(config_value,widget,script,script_attribute)
        
    def c2w(self):
        if not self.config_value is None and not self.widget is None:
            self.widget.setText(self.to_str(self.config_value.value))
        
    def w2c(self):
        if not self.config_value is None and not self.widget is None:
            self.config_value.set(self.from_str(self.widget.value()))

    def w2s(self):
        if not self.widget is None and not self.s_attr is None:
            self.script[self.s_attr] = self.widget.value()
        
    def s2w(self):
        if not self.widget is None and not self.s_attr is None:
            self.widget.setValue(self.script[self.s_attr])
            
    def c2s(self):
        if not self.config_value is None and not self.s_attr is None:
            self.script[self.s_attr] = self.config_value.value
    
    def s2c(self):
        if not self.config_value is None and not self.s_attr is None:
            self.config_value.set(self.from_str(self.script[self.s_attr]))
        
################################################################################
class CWS_list():
################################################################################
    def __init__(self):
        self.lists = {}

    def add(self,key,cws):        
        if key in self.lists:
            self.lists[key].append(cws)
        else:
            self.lists[key] = [cws]
        if key!='*':
            self.add('*',cws)

    def c2w(self,key='*'):
        for cws in self.lists[key]:
            cws.c2w()
                     
    def w2c(self,key='*'):
        for cws in self.lists[key]:
            cws.w2c()

    def w2s(self,key='*'):
        for cws in self.lists[key]:
            cws.w2s()
        
    def s2w(self,key='*'):
        for cws in self.lists[key]:
            cws.s2w()

    def c2s(self,key='*'):
        for cws in self.lists[key]:
            cws.c2s()

    def s2c(self,key='*'):
        for cws in self.lists[key]:
            cws.s2c()

 