"""
some extensions to wxPython which are present in wxPython 2.9 but not in 2.8.10
  - xx.SpinCtrlDouble 
"""

import wx

import wx.lib.newevent
SpinDoubleEvent, EVT_SPINCTRLDOUBLE = wx.lib.newevent.NewCommandEvent()

def find_frame(window):
    """Return the frame of a wx widbget (or None)"""
    w = window
    while True:
        if isinstance(w,wx.Frame) or w is None:
            return w
        w = w.GetParent()

class SpinCtrlDouble(wx.FlexGridSizer):
    """
    based on a wx.txtctrl and a wx.SpinButton (and the above validator)
    """
    def __init__(self,parent,style=0,gap=0):
        self.name = "xx.SpinCtrlDouble instance"
        super(SpinCtrlDouble,self).__init__(1,2,gap,gap)
        self.txtctrl = wx.TextCtrl(parent,style=style|wx.TE_RIGHT|wx.EXPAND)
        self.spinner = wx.SpinButton(parent)
        self.AddGrowableCol(0)
        self.Add(self.txtctrl,1,wx.EXPAND)
        self.Add(self.spinner,0,0)
        
        self.SetValue(.0)
        self.SetRange()
        self.spinner.SetRange(-99999999,999999999)

        self.spinner.Bind(wx.EVT_SPIN_DOWN , self.spinner_EVT_SPIN_DOWN)
        self.spinner.Bind(wx.EVT_SPIN_UP   , self.spinner_EVT_SPIN_UP  )
        self.txtctrl.Bind(wx.EVT_KEY_DOWN  , self.txtctrl_EVT_KEY_DOWN)
        self.txtctrl.Bind(wx.EVT_KILL_FOCUS, self.txtctrl_EVT_KILL_FOCUS)
        
        self.frame = find_frame(parent)
    
    def SetValue(self,value):
        v = str(value)
        self.txtctrl.SetValue(v)
        self.txtctrl.SetInsertionPoint(len(v))

    def GetValue(self):
        return float(self.txtctrl.GetValue())

    def SetRange(self,lwr=0.0,upr=1.0,inc=0.01):
        self.lwr_bound=lwr
        self.upr_bound=upr
        self.increment=inc
        
    def post_EVT_SPINCTRLDOUBLE(self):
        event = SpinDoubleEvent(wx.ID_ANY)
        event.type = "xx.SpinDoubleEvent"
        event.value = self.txtctrl.GetValue()
        event.source = self.GetName()
        wx.PostEvent(self.frame, event)
        print "xx.EVT_SPINCTRLDOUBLE posted, current value is",self.txtctrl.GetValue()
        
    def spinner_EVT_SPIN_UP(self,event=None):
        v = min(self.upr_bound,self.GetValue()+self.increment)
        self.SetValue(v)
        self.post_EVT_SPINCTRLDOUBLE()
        
    def spinner_EVT_SPIN_DOWN(self,event=None):
        v = max(self.lwr_bound,self.GetValue()-self.increment)
        self.SetValue(v)
        self.post_EVT_SPINCTRLDOUBLE()
        
    def txtctrl_EVT_KEY_DOWN(self,event):
        key = event.GetKeyCode()      
        if key in (wx.WXK_UP,wx.WXK_DOWN):
            if key == wx.WXK_UP:
                self.spinner_EVT_SPIN_UP()
            else:
                self.spinner_EVT_SPIN_DOWN()
#         elif key==wx.WXK_RETURN:
#             self.post_EVT_SPINCTRLDOUBLE()
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
                    new_val = float(new_val)
                except:
                    self.txtctrl.SetValue(val)
                    self.txtctrl.SetInsertionPoint(pos)
                    return
                #clip to range
                new_val = max(new_val,self.lwr_bound)
                new_val = min(new_val,self.upr_bound)
                self.txtctrl.SetValue(str(new_val))
                self.txtctrl.SetInsertionPoint(new_pos)
                self.post_EVT_SPINCTRLDOUBLE()
                return
            else:
                if char.isalpha() or char.isspace() or char in ";:+=<>?!@#$%^&*(){}[]|~":
                    return
                event.Skip()
        else:
            event.Skip()
            
    def txtctrl_EVT_KILL_FOCUS(self,event):
        self.post_EVT_SPINCTRLDOUBLE()
            
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
        frame.SetSizer(SpinCtrlDouble(frame))
        frame.Show()
        return True

if __name__ == "__main__":
    _TestApp(0).MainLoop()