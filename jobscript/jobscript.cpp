#include "jobscript.h"
#include <QRegularExpression>
#include <cstddef>
#include <stdexcept>

namespace pbs
{//-----------------------------------------------------------------------------
    template <class T>
    T* create(QString const& line )
    {
        T* ptrT = new T(line);
        try {
            ptrT->init();
        } catch( std::logic_error& e ) {
            delete ptrT;
            ptrT = nullptr;
            throw e;
        }
        return ptrT;
    }
 //-----------------------------------------------------------------------------
    ShellCommand * parse(QString const & line) {
        return ShellCommand::parse(line);
    }
 //-----------------------------------------------------------------------------
    ShellCommand*
    ShellCommand::
    parse( QString const &line )
    {
        UserComment* parsed = UserComment::parse(line);
        if( parsed )
            return parsed;
        else
            return create<ShellCommand>(line);
    }
 //-----------------------------------------------------------------------------
    UserComment*
    UserComment::
    parse( QString const &line)
    {
        if( !line.startsWith('#') )
            return nullptr;
        UserComment* parsed = Shebang::parse(line);
        if( parsed )
            return parsed;
        parsed = LauncherComment::parse(line);
        if( parsed )
            return parsed;
        parsed = PbsDirective::parse(line);
        if( parsed )
            return parsed;
        else
            return create<UserComment>(line);
    }
 //-----------------------------------------------------------------------------
    Shebang*
    Shebang::
    parse( QString const &line)
    {
        if( line[1]!='!' )
            return nullptr;
        else
            return create<Shebang>(line);
    }
 //-----------------------------------------------------------------------------
    LauncherComment*
    LauncherComment::
    parse( QString const &line)
    {
        if( !line.startsWith("#La#") )
            return nullptr;
        else
            return create<LauncherComment>(line);
    }
 //-----------------------------------------------------------------------------
    PbsDirective*
    PbsDirective::
    parse( QString const &line)
    {
        if( !line.startsWith("#PBS") )
            return nullptr;
        else
            return create<PbsDirective>(line);
    }
 //-----------------------------------------------------------------------------
    ShellCommand::
    ShellCommand(QString const& line, int ordinate, types::Type type)
      : text_(line)
      , ordinate_(ordinate)
      , type_(type)
    {}
 //-----------------------------------------------------------------------------
    UserComment::
    UserComment(QString const& line, int ordinate, types::Type type)
      : ShellCommand(line,ordinate,type)
    {}
 //-----------------------------------------------------------------------------
    Shebang::
    Shebang( QString const &line, int ordinate, types::Type type)
      : UserComment(line,ordinate,type)
    {}
 //-----------------------------------------------------------------------------
    LauncherComment::
    LauncherComment( QString const &line, int ordinate, types::Type type)
      : UserComment(line,ordinate,type)
    {}
 //-----------------------------------------------------------------------------
    PbsDirective::
    PbsDirective( QString const &line, int ordinate, types::Type type)
      : UserComment(line,ordinate,type)
    {}
 //-----------------------------------------------------------------------------
    void LauncherComment::init()
    {
        QRegularExpressionMatch m;
        QRegularExpression re("#La#(\\s+)(.+)"); // line pattern
        m = re.match( this->text_ );
        if( !m.hasMatch() )
            throw_<std::runtime_error>("Ill formed LauncherComment: %1",this->text_);
        this->value_ = m.captured((2));
        re.setPattern("(\\w+)\\s*=\\s*(.+)"); // parameter pattern
        m = re.match( this->value_ );
        if( !m.hasMatch() )
            throw_<std::runtime_error>("Ill formed LauncherComment: %1",this->text_);
        this->parms_[m.captured(1)] = m.captured(2);
    }
 //-----------------------------------------------------------------------------
    void LauncherComment::compose()
    {
//        this->text_ = QString("#La# ")
//                .append(this->parms_[0].first)
//                .append(" = ")
//                .append(this->parms_[0].second)
//                ;
    }
 //-----------------------------------------------------------------------------
//    QString const&
//    ShellCommand::
//    parm_value( QString const & key ) const {
//        return this->parms_[key];
//    }
 //-----------------------------------------------------------------------------
    void PbsDirective::init()
    {
        QRegularExpressionMatch m;
        QRegularExpression re("#PBS\\s+(-\\w)\\s+([^\\s]*)(\\s*#.+)?");
        m = re.match( this->text_ );
        if( !m.hasMatch() )
            throw_<std::runtime_error>("Ill formed PbsDirective: %1",this->text_);
        this->flag_    = m.captured((1));
        this->value_   = m.captured((2));
        this->comment_ = m.captured((3));
        re.setPattern(":((\\w+)=((\\d{1,2}:\\d\\d:\\d\\d)|([\\w.-]+)))(.*)");
        QString remainder = QString(':')+this->value_;
     // look for parameters
        m = re.match(remainder);
        if( m.hasMatch() )
        {
            while( m.hasMatch() ) {
                this->parms_[m.captured(1)] = m.captured(2);
                remainder = m.captured(m.lastCapturedIndex());
                m = re.match(remainder);
            }
         // look for features
            re.setPattern(":(\\w+)(.*)");
            m = re.match(remainder);
            while( m.hasMatch() ) {
                this->feats_.append(m.captured(0));
                remainder = m.captured(m.lastCapturedIndex());
                m = re.match(remainder);
            }
            if( !remainder.isEmpty() )
                throw_<std::runtime_error>("PBS line not fully matched, remainder='%1'", remainder );
        }
    }
 //-----------------------------------------------------------------------------
    Script::
    Script( QString       const* filepath
          , Lines_t       const* lines
          , cfg::Config_t const* config
          )
    {
        this->filepath_ = ( filepath ? *filepath : QString() );
     // we ar here:>>
        #create default lines. which store dummy parameters
        this->add(Shebang())

        this->add("#La# generated_on = {}".format(datetime.datetime.now()))
        this->add("#La#      cluster = {}".format(cfg_get(config,'cluster'              ,)))
        this->add("#La#      nodeset = {}".format(cfg_get(config,'nodeset')))

        this->add('#PBS -l nodes={}:ppn={}'.format(cfg_get(config,'nodes',1)
                                                 ,cfg_get(config,'ppn'  ,1)))

        this->add('#PBS -l walltime={}'.format(cfg_get(config,'walltime','1:00:00')))

        this->add('#PBS -M {}'.format(cfg_get(config,'notify_M','your.email@address.here')))
        this->add('#PBS -m {}'.format(cfg_get(config,'notify_m','e')))

        this->add('#PBS -W x=nmatchpolicy:exactnode')

        this->add('#PBS -N dummy',hidden=True)

        this->add('#')
        this->add("#--- shell commands below ".ljust(80,'-'))
        this->add('cd $PBS_O_WORKDIR')

        set_is_modified(self)
        this->_unsaved_changes = True
        #parse file, then lines
        this->read(filepath)
        this->parse(lines)
    }
 //=============================================================================
}// namespace pbs
/*
################################################################################
class Script(object):
################################################################################
    """
    Representation of script
    """
    auto_unhide = False

    def __init__(self,filepath=None,lines=[],config=None):
        """
        If a valid filename is provided, that is read and parsed first.
        Than additional lines are added
        """
        self.script_lines=[]
        self.filepath = None

        #create default lines. which store dummy parameters
        self.add(Shebang())

        self.add("#La# generated_on = {}".format(datetime.datetime.now()))
        self.add("#La#      cluster = {}".format(cfg_get(config,'cluster'              ,)))
        self.add("#La#      nodeset = {}".format(cfg_get(config,'nodeset')))

        self.add('#PBS -l nodes={}:ppn={}'.format(cfg_get(config,'nodes',1)
                                                 ,cfg_get(config,'ppn'  ,1)))

        self.add('#PBS -l walltime={}'.format(cfg_get(config,'walltime','1:00:00')))

        self.add('#PBS -M {}'.format(cfg_get(config,'notify_M','your.email@address.here')))
        self.add('#PBS -m {}'.format(cfg_get(config,'notify_m','e')))

        self.add('#PBS -W x=nmatchpolicy:exactnode')

        self.add('#PBS -N dummy',hidden=True)

        self.add('#')
        self.add("#--- shell commands below ".ljust(80,'-'))
        self.add('cd $PBS_O_WORKDIR')

        set_is_modified(self)
        self._unsaved_changes = True
        #parse file, then lines
        self.read(filepath)
        self.parse(lines)

    def read(self,filepath,additive=False):
        if not filepath:
            return

        with open(filepath) as f:
            lines = f.readlines()
        self.parse(lines,additive=additive)
        self.filepath = os.path.abspath(filepath)

    def write(self,filepath=None,warn_before_overwrite=True,create_directories=False):
        new_filepath = filepath
        if not new_filepath:
            new_filepath = self.filepath
        if not new_filepath:
            raise IOError('Script.write(): Destination file unknown.')

        new_filepath = os.path.abspath(new_filepath)
        head,tail = os.path.split(new_filepath)
        if create_directories:
            my_makedirs(head)
        else:
            if not os.path.exists(head):
                raise InexistingParentFolder(head)

        if warn_before_overwrite and os.path.exists(new_filepath):
            raise AttemptToOverwrite(new_filepath)

        with open(new_filepath,'w+') as f:
            f.write(self.get_text())

        self.filepath = new_filepath

        self.set_unsaved_changes(False)

    def set_unsaved_changes(self,unsaved=True):
        """ use this method to control unsaved_changes state from outside.
            the state is automatically set to False after a write.
        """

        self._unsaved_changes = unsaved

    def unsaved_changes(self):
        return self._unsaved_changes

    def parse(self,text,additive=False):
        """
        Text can be
          . plain text (str or unicode), possibly containing newline characters, each line will be parsed separately
          . a list of single lines
        """
        if not text:
            return
        if not additive:
            # this case is when the text represents the full script - possibly containing new information
            # such as new lines, old lines with new parameter values or features...
            # We first remove all lines which do not contain parameterised information
            # from the current script:
            #     . UserComment
            #     . ShellCommand
            # This is because we cannot know whether new lines of these type are equal to existing lines
            # in the script as they may occur more than once.
            for i in xrange(len(self.script_lines)-1,-1,-1):
                line_i = self.script_lines[i]
#                 print
#                 print line_i,
#                 print type(line_i)
                if type(line_i) in (ShellCommand,UserComment):
                    self.script_lines.pop(i)
#                     print '  deleted'
        else:
            # All UserComment and ShellCommands are appended at the end, in the order presented,
            # thus allowing several occurrences of the same line.
            pass
        if type(text) is unicode:
            text = str(text)
        if type(text) is str:
            text = text.splitlines()
        #assert type(text) is list
        for line in text:
            self.add(line)

    def add(self,line,hidden=False,_parsing=False):
        """
        Note that directly calling add acts like calling parse() with additive=True
        _parsing must be true when called by self.parse(), it affects the bookkeeping of unsaved_changes
        """
        if issubclass(type(line),ShellCommand):
            new_script_line = line
        else:
            if type(line) is unicode:
                line = str(line)
            #assert isinstance(line,str), "expecting a str/unicode or a subclass of ShellCommand, got '{}'".format(str(line))
            new_script_line = ShellCommand.parse(line)
            new_script_line.hidden = hidden

        if type(new_script_line) in (ShellCommand,UserComment):
            self._insert(new_script_line)
        else:
            for old_script_line in self.script_lines:
                if new_script_line==old_script_line:
                    type_new_script_line = type(new_script_line)
                    if type_new_script_line in (LauncherComment,PBSDirective):
                        old_script_line.hidden = new_script_line.hidden
                        old_parms = old_script_line.parms
                        if not old_parms:
                            old_script_line.value = new_script_line.value
                            set_is_modified(old_script_line)
                            set_is_modified(self)
                        else:
                            new_parms = new_script_line.parms
                            for k,v in new_parms.iteritems():
                                if not k in old_parms or old_parms[k]!=v:
                                    old_parms[k] = v
                                    set_is_modified(old_script_line)
                                    set_is_modified(self)
                            if hasattr(old_script_line,'features'):
                                old_features = old_script_line.features
                                new_features = new_script_line.features
                                for f in new_features:
                                    if not f in old_features:
                                        old_features.append(f)
                                        set_is_modified(old_script_line)
                                        set_is_modified(self)
                        return

                    if type_new_script_line is Shebang:
                        if old_script_line.text != new_script_line.text:
                            old_script_line.text=new_script_line.text
                            set_is_modified(old_script_line)
                            set_is_modified(self)
                        return

                    raise Exception('Unexpected Script line type : '+str(type(new_script_line)))
            else:
                #this line is completly new and must be inserted
                self._insert(new_script_line)

            if not _parsing:
                self._unsaved_changes = True

    def _insert(self,line):
        ordinate = line.ordinate()
        if ordinate==-1:
            self.script_lines.append(line)
        else:
            for i,l in enumerate(self.script_lines):
                l_ordinate = l.ordinate()
                if ordinate < l_ordinate:
                    self.script_lines.insert(i,line)
                    break
            else:
                self.script_lines.append(line)
        set_is_modified(self)

    def remove(self,line):
        i = self.index(line)
        if i>-1:
            self.script_lines.pop(i)
            set_is_modified(self)
            return True
        return False

    def index(self,line):
        new_script_line = ShellCommand.parse(line)
        for i,old_script_line in enumerate(self.script_lines):
            if new_script_line==old_script_line:
                return i
        return -1

    def __getitem__(self,key):
        """ hidden lines are ignored """
        if key[0]=='-':
            for l in self.script_lines:
#                 if not l.hidden:
                try:
                    if l.flag==key:
                        return l.value
                except AttributeError:
                    pass
        else:
            for l in self.script_lines:
#                 if not l.hidden:
                l_parms = getattr(l,'parms',{})
                if l_parms:
                    val = l_parms.get(key,None)
                    if not val is None:
                        return val
        raise KeyError("Key '"+key+"' not found in script.")

    def __setitem__(self,key,val):
        if key[0]=='-':
            for l in self.script_lines:
                try:
                    if l.flag==key:
                        l.value = val
                        set_is_modified(l)
                        set_is_modified(self)
                        if Script.auto_unhide:
                            l.hidden = False
                        return
                except AttributeError:
                    pass
        else:
            for l in self.script_lines:
                l_parms = getattr(l,'parms',{})
                if l_parms:
                    if key in l_parms:
                        l_parms[key]=val
                        set_is_modified(l)
                        set_is_modified(self)
                        if l.hidden and Script.auto_unhide:
                            for v in l.parms.itervalues():
                                if v=='dummy':
                                    break
                            else:
                                l.hidden = False
                        return
            raise KeyError("Key '"+key+"' not found in script.")

    def add_parameters(self,key,parms,remove=False):
        """ add (or remove) parameters 'parms' from the script line containing the parameter 'key'. """
        assert isinstance(parms,dict)
        if key[0]=='-':
            raise ValueError('Key {} does not accept features.'.format(key))
        else:
            for l in self.script_lines:
                l_parms = getattr(l,'parms',{})
                if l_parms:
                    if key in l_parms:
                        if remove:
                            for k in parms.iterkeys():
                                del l_parms[k]
                        else:
                            l_parms.update(parms)
                        set_is_modified(l)
                        set_is_modified(self)
                        return
            raise KeyError("Key '"+key+"' not found in script.")

    def add_features(self,key,features,remove=False):
        """ add (or remove) the features 'features' in the script line containing parameter 'key'. """
        assert isinstance(features,list)
        if key[0]=='-':
            raise ValueError('Key {} does not accept features.'.format(key))
        else:
            for l in self.script_lines:
                l_parms = getattr(l,'parms',{})
                if key in l_parms:
                    l_features = l.features
                    if remove:
                        for f in features:
                            try:
                                l_features.remove(f)
                            except ValueError:
                                pass
                    else:
                        for f in features:
                            if f not in l_features:
                                l_features.append(f)
                    set_is_modified(l)
                    set_is_modified(self)
                    return
            raise KeyError("Key '"+key+"' not found in script.")

    def set_hidden(self,key,hidden):
        """ hide (or show) the script line(s) containing the parameter or flag 'key'. """
        if key[0]=='-':
            for l in self.script_lines:
                try:
                    flag = l.flag
                except AttributeError:
                    continue
                else:
                    if flag==key:
                        if l.hidden!=hidden:
                            l.hidden = hidden
                            set_is_modified(l)
                            set_is_modified(self)
                        return
        else:
            for l in self.script_lines:
                l_parms = getattr(l,'parms',{})
                if key in l_parms:
                    if l.hidden!=hidden:
                        l.hidden = hidden
                        set_is_modified(l)
                        set_is_modified(self)
                    return
        raise KeyError("Key '"+key+"' not found in script.")

    def is_hidden(self,key):
        """ hide (or show) the script line(s) containing the parameter or flag 'key'. """
        if key[0]=='-':
            for l in self.script_lines:
                try:
                    flag = l.flag
                except AttributeError:
                    continue
                else:
                    if flag==key:
                        return l.hidden
        else:
            for l in self.script_lines:
                l_parms = getattr(l,'parms',{})
                if key in l_parms:
                    return l.hidden
        raise KeyError("Key '"+key+"' not found in script.")

    def has_line(self,line):
        i = self.index(line)
        return i>-1

    def get_text(self):
        if is_modified(self):
            self['generated_on'] = str(datetime.datetime.now())
            for sl in self.script_lines:
                if not sl.hidden:
                    sl.compose()
                if not sl.hidden:
                    self.text += sl.text
        return self.text

    def set_jobname(self,jobname):
        self['-N'] = jobname
        self.set_hidden('-N', False)

    def jobname(self):
        return self['-N']

    def count(self,hidden=False):
        if hidden:
            return len(self.script_lines)
        else:
            n=0
            for l in self.script_lines:
                if not l.hidden:
                    n+=1
            return n

################################################################################
class AttemptToOverwrite(Exception):
    pass
################################################################################
class InexistingParentFolder(Exception):
    pass

################################################################################
### only test code below this line
################################################################################
if __name__=='__main__':
    import unittest
    from indent import Indent

    ################################################################################
    class Test0(unittest.TestCase):
    ################################################################################
        def test0(self):
            script = Script()
            print '0 Test0.test0()\n',Indent(script.script_lines,2)

            self.assertTrue(script.count(True )==13)

            self.assertTrue(script['walltime']=='1:00:00')

            script['walltime'] = '0:02:30'
            Script.auto_unhide = True
            script['walltime'] = '0:01:30'
            self.assertTrue(not script.is_hidden('walltime'))
            self.assertTrue(script['walltime']=='0:01:30')

            email='engelbert.tijskens@uantwerpen.be'
            script['-M']=email
            self.assertTrue(script['-M'], email)

            lines = [''
                    ,'cd $PBS_O_WORKDIR'
                    ,'if [ -d "output" ]; then'
                    ,'    rm -rf output'
                    ,'fi'
                    ,'mkdir output'
                    ,'cd output'
                    ,'echo "this is the output file" > o.txt'
                    ,'cd ..'
                    ,'#PBS -W x=nmatchpolicy:y=soep:z=ballekes:exactnode'
                    ,'#PBS -N a_simple_job'
                    ,'#PBS -M engelbert.tijskens@uantwerpen.be #say hello'
                    ,'#PBS -m ae'
                    ,'#PBS -l walltime=1:00:00'
                    ,'#PBS -l nodes=1:ppn=20'
                    ,'#La# generated_on = '+str(datetime.datetime.now())
                    ,'#La#      cluster = Hopper'
                    ,'#La#      nodeset = Hopper-thin-nodes'
                    ]
            script.parse(lines)
            print '1 Test0.test0()\n',Indent(script.script_lines,2)
            print script.count()
            self.assertTrue(script.count(True )==19)
            self.assertTrue(script.count(False)==19)
            script.set_hidden('-m', True)
            script.set_hidden('walltime', True)
            print script.count(True)
            print script.count(False)

            filepath = 'test_pbs.sh'
            if os.path.exists(filepath):
                os.remove(filepath)
            script.write(filepath)
            try:
                script.write(filepath)
            except AttemptToOverwrite as e:
                print 'as expected: ',e
            else:
                assert False, 'Was expecting KeyError'

        def test1(self):
            filepath = 'test_pbs.sh'
            script = Script(filepath=filepath)
            print '0 Test0.test1()\n',Indent(script.script_lines,2)
    ################################################################################

    unittest.main()
*/
