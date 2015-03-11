import wx
import xx
import math

def add_notebook_page(wNotebook,title):
    """
    Add a notebook page to a wx.Notebook
    Return the notebook page object (wx.Panel)
    """
    page = wx.Panel(wNotebook, wx.ID_ANY)
    wNotebook.AddPage(page, title)
    return page

def create_staticbox(parent,label='',orient=wx.VERTICAL, colour=wx.Colour(20,40,120)):
    """
    return a wx.StaticBoxSizer containing a wx.StaticBox 
    """
    staticbox = wx.StaticBox(parent, wx.ID_ANY)
    if not colour is None:
        #TODO: colour doesn't seem to work on darwin...
        # it works on win32 though
        staticbox.SetForegroundColour(colour)
    staticbox.SetLabel(label)
    sizer = wx.StaticBoxSizer(staticbox, orient=orient)
    #staticbox.Lower()
    return sizer

def grid_layout(items,ncols=0,gap=0,growable_rows=[],growable_cols=[]):
    """
    return a wx.FlexGridSizer with ncols colums and enough rows to 
    contain all items in the items list. Growable rows and columns 
    can be set.
    ncols=0 corresponds to all items on a single row. To put all 
    items on a single column specify ncols=1.
    """
    nitems = len(items)
    ncols_ = ncols if ncols>0 else nitems 
    nrows_ = int(math.ceil(float(nitems)/ncols_))
    szr = wx.FlexGridSizer(nrows_,ncols_,gap,gap)
    for r in growable_rows:
        if r<nrows_:
            szr.AddGrowableRow(r)
    for c in growable_cols:
        if c<ncols_:
            szr.AddGrowableCol(c)
    for item in items:
        if isinstance(item, list):
            szr.Add(*item)
        else:
            szr.Add(item,0,0)
    return szr

def pair(parent,label, value=None, value_range=None,tip=None,style0=0,style1=0):
    text = wx.StaticText(parent,label=label,style=style0)
    if isinstance(value,bool):
        widget = wx.CheckBox(parent,style=style1)
        widget.SetValue( False if (value is None) else \
                         value )
    elif isinstance(value,int):
        widget = xx.SpinCtrl(parent,converter=int,value=value,value_range=value_range)
    elif isinstance(value,float):
        widget = xx.SpinCtrl(parent,converter=float,value=value,value_range=value_range)
    elif isinstance(value,str):
        if value_range is None:
            widget = wx.TextCtrl(parent,value=value)
        else:
            assert isinstance(value_range,list), "expecting list of strings"
            widget = wx.ComboBox(parent,value=value,choices=value_range)
    else:
        raise TypeError("unknown value type: {}".format(type(value)))
    if not tip is None:
        text  .SetToolTipString(tip)
        widget.SetToolTipString(tip)
    return [text,widget]

def set_tool_tip_string(controls,tip=""):
    if not isinstance(controls,(list,tuple)):
        #a single control
        controls.SetToolTipString(tip)
    else:
        #list of tuple of controls
        for ctrl in controls:
            ctrl.SetToolTipString(tip)