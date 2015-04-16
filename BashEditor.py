#!/opt/local/bin/python2.7


#import pprint
import wx, wx.stc

class PbsLexer(wx.stc.StyledTextCtrl):
    
    STC_PBS_DEFAULT = 14
    STC_PBS_KEYWORD = 15
    STC_PBS_COMMENT = 16
    
    def __init__(self, parent, ID=-1):
        wx.stc.StyledTextCtrl.__init__(self, parent)
        self.Show(False)
        self.SetLexer(wx.stc.STC_LEX_CONTAINER)
        self.Bind(wx.stc.EVT_STC_STYLENEEDED, self.on_EVT_STC_STYLENEEDED)
        self.keywords = "walltime= nodes= ppn= :".split()
          
    def on_EVT_STC_STYLENEEDED(self, event):
        stc = event.GetEventObject()
        # Last correctly styled character
        last_styled_pos = stc.GetEndStyled()
        # Get styling range for this call
        lino = stc.LineFromPosition(last_styled_pos)
        start_pos = stc.PositionFromLine(lino)
        end_pos = event.GetPosition()
        
        text = self.GetTextRange(start_pos,end_pos)
        kw_positions = []
        #find any keywords to color them differently
        i=0
        for kw in self.keywords:
            try:
                l = len(kw)
                while True:
                    j = text[i:].index(kw)
                    kw_positions.append((start_pos+i+j,l,kw)) 
                    i+=j+l
            except:
                i=0
        #find any comments in the pbs commands
        #we start looking for '#" at position 5 as text should begin with '#PBS ' anyway
        i=5
        len_text=len(text)
        while i<len_text:
            if text[i]=='#' and not text[i-1]=='\n':
                #found a '#' in the middle of a line (at position i), find the end of the line.
                j = i+text[i:].index('\n')+1
                kw_positions.append((start_pos+i,j-i,text[i:j]))
                i=j
            else:
                i+=1
        
        
        kw_positions.sort( key=lambda tpl: tpl[0]+float(tpl[1])*0.01)
        #pprint.pprint( kw_positions )
        if not kw_positions:
            self.StartStyling(start_pos,31)
            self.SetStyling(end_pos-start_pos,PbsLexer.STC_PBS_DEFAULT)
            return
        i=kw_positions[0][0]
        if i != start_pos:
            self.StartStyling(start_pos,31)
            self.SetStyling(kw_positions[0][0]-start_pos,PbsLexer.STC_PBS_DEFAULT)
        last = len(kw_positions)-1
        for t,tpl in enumerate(kw_positions):
            self.StartStyling(tpl[0],31)
            if tpl[2].startswith('#'):
                style = PbsLexer.STC_PBS_COMMENT
            else:
                style = PbsLexer.STC_PBS_KEYWORD
            self.SetStyling(tpl[0],style)
            if t<last:
                i1  = tpl[0]+tpl[1]
                i0n = kw_positions[t+1][0]
            else:
                i1  = tpl[0]+tpl[1]
                i0n = end_pos
            if i1 < i0n:
                self.StartStyling(i1,31)
                self.SetStyling(i0n-i1,PbsLexer.STC_PBS_DEFAULT)
                
class PbsEditor(wx.stc.StyledTextCtrl):

    def __init__(self, parent, ID=-1,
                 pos=wx.DefaultPosition, size=wx.DefaultSize, style=0):
        wx.stc.StyledTextCtrl.__init__(self, parent, ID, pos, size, style)
        self.SetLexer(wx.stc.STC_LEX_CONTAINER)
#         self.SetStyleBits(5)
        self.Bind(wx.stc.EVT_STC_STYLENEEDED, self.on_EVT_STC_STYLENEEDED)
        
#         l=[wx.stc.STC_SH_BACKTICKS
#           ,wx.stc.STC_SH_CHARACTER
#           ,wx.stc.STC_SH_COMMENTLINE
#           ,wx.stc.STC_SH_ERROR
#           ,wx.stc.STC_SH_HERE_DELIM
#           ,wx.stc.STC_SH_HERE_Q
#           ,wx.stc.STC_SH_IDENTIFIER
#           ,wx.stc.STC_SH_NUMBER
#           ,wx.stc.STC_SH_OPERATOR
#           ,wx.stc.STC_SH_PARAM
#           ,wx.stc.STC_SH_SCALAR
#           ,wx.stc.STC_SH_STRING
#           ,wx.stc.STC_SH_WORD
#           ]
#         print max(l),l

        self.StyleSetSpec(wx.stc.STC_STYLE_DEFAULT  ,"fore:#000000,back:#FFFFFF,face:Courier New,size:12")
        self.StyleClearAll()
        self.StyleSetSpec(wx.stc.STC_SH_DEFAULT     ,"fore:#333333,back:#FFFFFF,face:Courier New,size:12")
        self.StyleSetSpec(wx.stc.STC_SH_COMMENTLINE ,"fore:#009944,back:#FFFFFF,face:Courier New,size:12,")
  
        self.StyleSetSpec(PbsLexer.STC_PBS_DEFAULT  ,"fore:#94071F,back:#FFFFFF,face:Courier New,size:12,bold")
        self.StyleSetSpec(PbsLexer.STC_PBS_KEYWORD  ,"fore:#4B8AD1,back:#FFFFFF,face:Courier New,size:12,bold")
        self.StyleSetSpec(PbsLexer.STC_PBS_COMMENT  ,"fore:#009944,back:#FFFFFF,face:Courier New,size:12,")

        map_theme_to_style(self,'comment'   , PbsLexer.STC_PBS_COMMENT)

        map_theme_to_style(self,'comment'   , wx.stc.STC_SH_COMMENTLINE)
        map_theme_to_style(self,'number'     , wx.stc.STC_SH_NUMBER)
        map_theme_to_style(self,'keyword1'   , wx.stc.STC_SH_WORD)
        map_theme_to_style(self,'string'     , wx.stc.STC_SH_STRING)
        map_theme_to_style(self,'character'  , wx.stc.STC_SH_CHARACTER)
        map_theme_to_style(self,'operator'   , wx.stc.STC_SH_OPERATOR)
        map_theme_to_style(self,'identifier' , wx.stc.STC_SH_IDENTIFIER)
        map_theme_to_style(self,'number'     , wx.stc.STC_SH_SCALAR)
        map_theme_to_style(self,'identifier' , wx.stc.STC_SH_PARAM)
        #map_theme_to_style(self,'backticks'  , wx.stc.STC_SH_BACKTICKS)
        #map_theme_to_style(self,'here_delim' , wx.stc.STC_SH_HERE_DELIM)
        #map_theme_to_style(self,'here_q'     , wx.stc.STC_SH_HERE_Q)
        #map_theme_to_style(self,'sh_error'   , wx.stc.STC_SH_ERROR)

        # === More stylings ===
        self.SetEdgeMode(wx.stc.STC_EDGE_LINE)
        self.SetEdgeColumn(80)
        self.SetMarginWidth(1, 0)
        #self.SetWrapMode(wx.stc.STC_WRAP_WORD)
        
        # === Dummy controls as lexers ===
        self.bash_parser = wx.stc.StyledTextCtrl(self, -1)
        self.bash_parser.Show(False)
        self.bash_parser.SetLexer(wx.stc.STC_LEX_BASH)
        #self.bash_parser.SetStyleBits(7)
        #self.bash_parser.SetKeyWords(0, HTML_KEYWORDS)
        self.pbs_parser = PbsLexer(self, -1)
    
    def _parse(self, parser, fragment):
        parser.SetText(fragment)
        fl = len(fragment)
        parser.Colourise(0, fl)
        multiplexed = parser.GetStyledText(0, fl)
        return multiplexed
    
    def on_EVT_STC_STYLENEEDED(self, evt):
        text = self.GetText().encode("utf8")
        lines = text.splitlines(True)
        n_lines = len(lines)
        parser = n_lines*[self.bash_parser]
        for l in range(0,n_lines):
            if lines[l].startswith("#PBS"):
                parser[l] = self.pbs_parser
        parsed =  ""
        lex_text = lines[0]
        for l in range(1,n_lines):
            if parser[l]==parser[l-1]:
                lex_text += lines[l]
            else:
                parsed += self._parse(parser[l-1],lex_text)
                lex_text = lines[l]
        if lex_text:
            parsed += self._parse(parser[l],lex_text)
            del lex_text
           
        self.StartStyling(0, 31)
        parsed = "".join([parsed[i] for i in range(1, len(parsed), 2)])
        self.SetStyleBytes(len(parsed), parsed)

def map_theme_to_style(wx_styled_text_ctrl,style_str,wx_style_id,theme='White',face="Courier New",size=12):
    if style_str.startswith('STC_STYLE_'):
        key = style_str
    else:
        key = 'STC_STYLE_'+style_str.upper()
    style_spec = color_theme[theme][key]+",face:"+face+",size:"+str(size)
    wx_styled_text_ctrl.StyleSetSpec(wx_style_id,style_spec)

color_theme = {
    'Blue': {
        'STC_STYLE_TEXT' : "fore:#8DB0D3",
        'STC_STYLE_NUMBER' : "fore:#FF00FF",
        'STC_STYLE_STRING' : "fore:#00FF80",
        'STC_STYLE_COMMENT' : "italic,fore:#00CFCB",
        'STC_STYLE_COMMENTBLOCK' : "italic,fore:#00CFCB",
        'STC_STYLE_KEYWORD1' : "fore:#FFFF00",
        'STC_STYLE_KEYWORD2' : "fore:#AFFFEE",
        'STC_STYLE_IDENTIFIER' : "fore:#8DB0D3",
        'STC_STYLE_OPERATOR' : "fore:#F0804F",
        'STC_STYLE_CHARACTER' : "fore:#E19618",
        'STC_STYLE_REGEX' : "fore:#FFFF80",
        'STC_STYLE_DEFAULT' : "fore:#FFFFFF,back:#112435,face:%(mono)s,size:%(size)d",
        'STC_STYLE_LINENUMBER' : "fore:#FFFFFF,back:#1F4661,face:%(mono)s,size:%(lnsize)d",
        'STC_STYLE_CONTROLCHAR' : "fore:#FFFFFF",
        'STC_STYLE_BRACELIGHT' : "bold,fore:#FF0000",
        'STC_STYLE_BRACEBAD' : "bold,fore:#FFFFFF",
        'STC_STYLE_CARET_FORE' : "fore:#FF0000",
        'STC_STYLE_CARETLINE_BACK' : "back:#413FFF",
        'STC_STYLE_SELECTION_BACK' : "back:#2E9F27",
        'STC_STYLE_FOLDER' : "back:#3476A3",
        'STC_STYLE_MARKER' : "fore:#F0804F,back:#FFFFFF",
        'STC_STYLE_TAG' : "fore:#F56FFF",
        'STC_STYLE_TAG1' : "fore:#2CEF50",
        'STC_STYLE_TAGKEY' : "fore:#FFFF00",
        'STC_STYLE_ATTRNAME' : "fore:#F0804F",
        'STC_STYLE_ATTRVALUE' : "fore:#156EB2",
        'STC_STYLE_CLASSNAME' : "fore:#BBFF4F",
        'STC_STYLE_DEFNAME' : "fore:#8DAF57",
        'STC_STYLE_LINK' : "fore:#FEFF8F,underline",
        'STC_STYLE_STRINGEOL' : "fore:#FF6F82,back:#E0C0E0,eol",
        'STC_STYLE_TRIPLE' : "fore:#00FF80",
    },
    'Black': {
        'STC_STYLE_TEXT' : "fore:#CCCCCC",
        'STC_STYLE_NUMBER' : "fore:#6B238E",
        'STC_STYLE_STRING' : "fore:#E19618",
        'STC_STYLE_COMMENT' : "italic,fore:#626262",
        'STC_STYLE_COMMENTBLOCK' : "italic,fore:#626262",
        'STC_STYLE_KEYWORD1' : "fore:#6AB825",
        'STC_STYLE_KEYWORD2' : "fore:#4179C5",
        'STC_STYLE_IDENTIFIER' : "fore:#CCCCCC",
        'STC_STYLE_OPERATOR' : "fore:#BBBBBB",
        'STC_STYLE_CHARACTER' : "fore:#8F5511",
        'STC_STYLE_REGEX' : "fore:#FFFF80",
        'STC_STYLE_DEFAULT' : "fore:#8000FF,back:#000000,face:%(mono)s,size:%(size)d",
        'STC_STYLE_LINENUMBER' : "fore:#CCCCCC,back:#333333,face:%(mono)s,size:%(lnsize)d",
        'STC_STYLE_CONTROLCHAR' : "fore:#FFFFFF",
        'STC_STYLE_BRACELIGHT' : "bold,fore:#FF0000",
        'STC_STYLE_BRACEBAD' : "bold,fore:#0000FF",
        'STC_STYLE_CARET_FORE' : "fore:#FF0000",
        'STC_STYLE_CARETLINE_BACK' : "back:#050349",
        'STC_STYLE_SELECTION_BACK' : "back:#8080FF",
        'STC_STYLE_FOLDER' : "back:#333333",
        'STC_STYLE_MARKER' : "fore:#FFFFFF,back:#000000",
        'STC_STYLE_TAG' : "fore:#15852B",
        'STC_STYLE_TAG1' : "fore:#2CEF50",
        'STC_STYLE_TAGKEY' : "fore:#FFFF00",
        'STC_STYLE_ATTRNAME' : "fore:#F0804F",
        'STC_STYLE_ATTRVALUE' : "fore:#156EB2",
        'STC_STYLE_CLASSNAME' : "fore:#FF0000",
        'STC_STYLE_DEFNAME' : "fore:#4179C5",
        'STC_STYLE_LINK' : "fore:#FEFF8F,underline",
        'STC_STYLE_STRINGEOL' : "fore:#408080,back:#E0C0E0,eol",
        'STC_STYLE_TRIPLE' : "fore:#EABF71",
    },
    'White': {
        'STC_STYLE_TEXT' : "fore:#000000",
        'STC_STYLE_NUMBER' : "fore:#6B238E",
        'STC_STYLE_STRING' : "fore:#2A2AA5",
        'STC_STYLE_COMMENT' : "italic,fore:#238E23,back:#E8FFE8",
        'STC_STYLE_COMMENTBLOCK' : "italic,fore:#238E23,back:#E8FFE8",
        'STC_STYLE_KEYWORD1' : "fore:#FF7700",
        'STC_STYLE_KEYWORD2' : "bold,fore:#2F032A",
        'STC_STYLE_IDENTIFIER' : "fore:#000000",
        'STC_STYLE_OPERATOR' : "fore:#000000",
        'STC_STYLE_CHARACTER' : "fore:#9F9F9F",
        'STC_STYLE_REGEX' : "fore:#DB70DB",
        'STC_STYLE_DEFAULT' : "face:%(mono)s,size:%(size)d",
        'STC_STYLE_LINENUMBER' : "back:#AAFFAA,size:%(lnsize)d",
        'STC_STYLE_CONTROLCHAR' : "fore:#000000",
        'STC_STYLE_BRACELIGHT' : "fore:#FF0000,bold",
        'STC_STYLE_BRACEBAD' : "fore:#0000FF,bold",
        'STC_STYLE_CARET_FORE' : "fore:#FF0000",
        'STC_STYLE_CARETLINE_BACK' : "back:#EEEEEE",
        'STC_STYLE_SELECTION_BACK' : "back:#000080",
        'STC_STYLE_FOLDER' : "back:#FFFFFF",
        'STC_STYLE_MARKER' : "fore:#FFFFFF,back:#000000",
        'STC_STYLE_TAG' : "fore:#15852B",
        'STC_STYLE_TAG1' : "fore:#2CEF50",
        'STC_STYLE_TAGKEY' : "bold,fore:#0000FF",
        'STC_STYLE_ATTRNAME' : "bold,fore:#F0804F",
        'STC_STYLE_ATTRVALUE' : "fore:#156EB2",
        'STC_STYLE_CLASSNAME' : "bold,fore:#FF0000",
        'STC_STYLE_DEFNAME' : "bold,fore:#007F7F",
        'STC_STYLE_LINK' : "fore:#0000FF,underline",
        'STC_STYLE_STRINGEOL' : "fore:#000000,back:#E0C0E0,eol",
        'STC_STYLE_TRIPLE' : "fore:#2A2AA5",
    },
    'Idle': {
        'STC_STYLE_TEXT' : "fore:#000000",
        'STC_STYLE_NUMBER' : "fore:#000000",
        'STC_STYLE_STRING' : "fore:#00AA00",
        'STC_STYLE_COMMENT' : "fore:#DD0000",
        'STC_STYLE_COMMENTBLOCK' : "fore:#DD0000",
        'STC_STYLE_KEYWORD1' : "fore:#ff7700",
        'STC_STYLE_KEYWORD2' : "fore:#ff7700",
        'STC_STYLE_IDENTIFIER' : "fore:#000000",
        'STC_STYLE_OPERATOR' : "fore:#000000",
        'STC_STYLE_CHARACTER' : "fore:#00AA00",
        'STC_STYLE_REGEX' : "fore:#DB70DB",
        'STC_STYLE_DEFAULT' : "face:%(mono)s,size:%(size)d",
        'STC_STYLE_LINENUMBER' : "back:#CCCCCC,size:%(lnsize)d",
        'STC_STYLE_CONTROLCHAR' : "fore:#FF00FF",
        'STC_STYLE_BRACELIGHT' : "bold,fore:#FF0000",
        'STC_STYLE_BRACEBAD' : "bold,fore:#0000FF",
        'STC_STYLE_CARET_FORE' : "fore:#FF0000",
        'STC_STYLE_CARETLINE_BACK' : "back:#EEEEEE",
        'STC_STYLE_SELECTION_BACK' : "back:#000080",
        'STC_STYLE_FOLDER' : "back:#FFFFFF",
        'STC_STYLE_MARKER' : "fore:#FFFFFF,back:#000000",
        'STC_STYLE_TAG' : "fore:#15852B",
        'STC_STYLE_TAG1' : "fore:#2CEF50",
        'STC_STYLE_TAGKEY' : "bold,fore:#0000FF",
        'STC_STYLE_ATTRNAME' : "bold,fore:#F0804F",
        'STC_STYLE_ATTRVALUE' : "fore:#156EB2",
        'STC_STYLE_CLASSNAME' : "fore:#0000FF",
        'STC_STYLE_DEFNAME' : "fore:#0000FF",
        'STC_STYLE_LINK' : "fore:#0000FF,underline",
        'STC_STYLE_STRINGEOL' : "fore:#000000,back:#E0C0E0,eol",
        'STC_STYLE_TRIPLE' : "fore:#00AA00",
    },
}

# ============================================================================
#
# The remainder of this module is for testing purposes
#
# ============================================================================

_test_string = u"""#!/bin/bash
#PBS -N s
#PBS -M engelbert.tijskens@uantwerpen.be # this is my e-mail address
#PBS -l walltime=0:05:00
#PBS -l nodes=1:ppn=1

# a comment line
cd $PBS_O_WORKDIR        # inline comment
 if [ -d "output" ]; then
    rm -rf output
fi
mkdir output
cd output
echo "this is the output file" > o.txt
cd .."""

class _TestApp(wx.App):

    def OnInit(self):
        frame = wx.Frame(None,-1, "Test Ctrl")
        PbsEditor(frame, -1).SetText(_test_string)
        frame.Show()
        return True

if __name__ == "__main__":
    _TestApp(0).MainLoop()