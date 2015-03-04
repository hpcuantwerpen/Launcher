"""
some extensions to wxPython which are present in wxPython 2.9 but not in 2.8.10
  - xx.SpinCtrl 
"""

import wx

import wx.lib.newevent
SpinDoubleEvent, EVT_SPINCTRL = wx.lib.newevent.NewCommandEvent()

def get_frame(window):
    """Return the frame of a wx widbget (or None)"""
    w = window
    while True:
        if isinstance(w,wx.Frame) or w is None:
            return w
        w = w.GetParent()
        
#default ranges for common converters
default_ranges = { int   : (0,100,1)
                 , float : (0.00, 1.00, 0.01)
                 }
    
class SpinCtrl(wx.FlexGridSizer):
    """
    based on a wx.txtctrl and a wx.SpinButton (and the above validator)
    """
    def __init__(self,parent,style=0,gap=0,converter=int,range_=None):
        super(SpinCtrl,self).__init__(1,2,gap,gap)
        self.name = "xx.SpinCtrl{} instance".format(int)
        self.frame = get_frame(parent)
        
        self.converter = converter
        
        self.txtctrl = wx.TextCtrl(parent,style=style|wx.TE_RIGHT|wx.EXPAND)
        self.spinner = wx.SpinButton(parent)
        self.AddGrowableCol(0)
        self.Add(self.txtctrl,1,wx.EXPAND)
        self.Add(self.spinner,0,0)
        
        if range_ is None:
            default_range = default_ranges[self.converter]
        else:
            default_range = range_
        self.SetRange(default_range)
        self.spinner.SetRange(-999999999,999999999)

        self.spinner.Bind(wx.EVT_SPIN_DOWN , self.spinner_EVT_SPIN_DOWN)
        self.spinner.Bind(wx.EVT_SPIN_UP   , self.spinner_EVT_SPIN_UP  )
        self.txtctrl.Bind(wx.EVT_KEY_DOWN  , self.txtctrl_EVT_KEY_DOWN)
        self.txtctrl.Bind(wx.EVT_KILL_FOCUS, self.txtctrl_EVT_KILL_FOCUS)
    
    def SetValue(self,value):
        v = str(self.converter(value))
        self.txtctrl.SetValue(v)
        self.txtctrl.SetInsertionPoint(len(v))

    def GetValue(self):
        return self.converter(self.txtctrl.GetValue())

    def SetRange(self,r):
        assert isinstance(r,tuple),"expecting tuple of length 2 or 3."
        l = len(r)
        assert l>1                ,"expecting tuple of length 2 or 3."
        
        self.lwr_bound = r[0]
        self.upr_bound = r[1]
        self.increment = r[2] if (l>2) else \
                         self.converter( (r[1]-r[0])/100. )
        self.SetValue(r[0])
        
    def post_EVT_SPINCTRL(self):
        event = SpinDoubleEvent(wx.ID_ANY)
        event.type = "xx.SpinDoubleEvent"
        event.value = self.txtctrl.GetValue()
        event.source = self.GetName()
        wx.PostEvent(self.frame, event)
        print "xx.EVT_SPINCTRL posted, current value is",self.txtctrl.GetValue()
        
    def spinner_EVT_SPIN_UP(self,event=None):
        v = min(self.upr_bound,self.GetValue()+self.increment)
        self.SetValue(v)
        self.post_EVT_SPINCTRL()
        
    def spinner_EVT_SPIN_DOWN(self,event=None):
        v = max(self.lwr_bound,self.GetValue()-self.increment)
        self.SetValue(v)
        self.post_EVT_SPINCTRL()
        
    def txtctrl_EVT_KEY_DOWN(self,event):
        key = event.GetKeyCode()      
        #print "txtctrl_EVT_KEY_DOWN",key
        if key in (wx.WXK_UP,wx.WXK_DOWN):
            if key == wx.WXK_UP:
                self.spinner_EVT_SPIN_UP()
            else:
                self.spinner_EVT_SPIN_DOWN()
        elif key < 256:
            char= chr(key)
            is_digit = char.isdigit()
            is_dot   = char=='.'
            if is_digit or is_dot:
                val = self.txtctrl.GetValue()
                pos = self.txtctrl.GetInsertionPoint()
                sel = self.txtctrl.GetSelection()
                if sel[0]<sel[1]:
                    new_val = val[:sel[0]]+char+val[sel[1]:]
                    if new_val==".":
                        new_val = "0."
                        new_pos = 2
                    else:
                        new_pos = sel[0]+1
                else:
                    if pos == len(val):
                        new_val = val+char
                    else:
                        new_val = val[:pos] + char + val[pos:]
                    new_pos = pos+1
                try:
                    new_val = self.converter(new_val)
                except:
                    #print "oops"
                    self.txtctrl.SetValue(val)
                    self.txtctrl.SetInsertionPoint(pos)
                    return
                #clip to range
                new_val = max(new_val,self.lwr_bound)
                new_val = min(new_val,self.upr_bound)
                self.txtctrl.SetValue(str(new_val))
                self.txtctrl.SetInsertionPoint(new_pos)
                return
            else:
                if char.isalpha() or char.isspace() or char in ",;:+=<>?!@#$%^&*(){}[]|`~/\\'\"-_":
                    return
                event.Skip()
        else:
            event.Skip()
            
    def txtctrl_EVT_KILL_FOCUS(self,event):
        self.post_EVT_SPINCTRL()

    def SetName(self,name):
        self.name = name
        
    def GetName(self):
        return self.name

################################################################################
### code below is for testing purposes only        
################################################################################
class _TestApp(wx.App):

    def OnInit(self):
        frame = wx.Frame(None,-1, "Test Ctrl")
        frame.SetSizer(SpinCtrl(frame,converter=int))
        frame.Show()
        return True

if __name__ == "__main__":
    _TestApp(0).MainLoop()