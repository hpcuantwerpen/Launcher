from __future__ import print_function

import pickle,types,os
import log
from indent import Indent

class CfgMissingPath(Exception):
    pass

class Config(object):
    verbose = False
    
    def __init__(self,path=None,clear=False):
        """ 
        Config Ctor. If path is None an empty Config object is created, otherwise, 
        path is expected to point to an existing config file, that will be loaded.
        An exception is raised if the file does not exist.            
        """
        self.values = {}
        if not clear and path:
            try:
                self.load(path)
            except:
                pass
        self.path = path

    def _path(self,path):
        if path is None:
            if self.path is None:
                raise CfgMissingPath
            p = self.path
        else:
            p = path
        return p
            
    def save(self,path=None):
        with log.LogItem('Saving config file:'):
            p = self._path(path)
            self.path = p
            pickle.dump(self.values,open(p,'w+'))
            print("    path: '{}'".format(p))
            if Config.verbose:
                print(Indent(str(self),6))
        
    def load(self,path=None):    
        with log.LogItem('Loading config file:'):
            p = self._path(path)
            if os.path.exists(p):
                self.values = pickle.load(open(p))
            else:
                if Config.verbose:
                    print("Config file '{}' does not exist (it will be created when saving)")
            self.path = p
            print("   path: '{}'".format(p))
            if Config.verbose:
                print(Indent(str(self),6))
        
    def __getitem__(self,name):
        return self.values[name]
    
    def __setitem__(self,name,value):
        assert False, 'Action not qllowd'

    def reset(self):
        for v in self.values.itervalues():
            v.reset()
            
    def inject_all(self,obj):
        for v in self.values.itervalues():
            v.inject(obj)    

    def __str__(self):
        s=""
        for k,v in self.values.iteritems():
            if ConfigValue.verbose:
                s+= k+" :\n"
                s+= str(Indent(str(v),2))
            else:
                s+= "{} : {}\n".format(k,v) 
        return s
    
    def create(self, name, value=None, default=None, inject_in=None):
        if name in self.values:
            return False
        ConfigValue(config=self,name=name,value=value,default=default,inject_in=inject_in)
        return True
    
class ConfigValue(object):
    verbose = False
    def __init__(self, config, name, value=None, default=None, inject_in=None):
        """
        config    : Config objec that stores this ConfigValue
        value     : current value assigned, can be a callable that returns the value
        default   : default value assigned, used by self.reset()
        inject_in : inject getter and setter methods in object 
        """
        assert isinstance(config,Config)
        self.name = name
        self.config = config
        self.default = default
        
        if callable(value):
            try:
                self.value = value()
            except:
                self.read_value(name)
        else:
            if value is None:
                self.read_value(name)
            else:
                self.value = value
        if self.value is None and not default is None:
            self.value = default 
                   
        config.values[name] = self
        if not inject_in is None:
            self.inject(inject_in)
        
    def read_value(self,name):
        assert hasattr(self.config,'values'), "Config object '{}' has not loaded a config file.".format(str(self.config.path))
        if name in self.config.values:
            self.value = self.config.values[name]
        else:
            self.value = self.default

    def get(self):
        return self.value
    
    def set(self,value):
        """assignment"""
        self.value = value

    def inc(self,value):
        self.set( self.value + value )
            
    def reset(self):
        self.value = self.default
    
    def make_getter(self):
        return lambda : self.config[self.name].get()
    
    def inject(self,obj):
        assert not isinstance(obj, Config), 'Injecting getters and setters in a Config object makes is unpickleable.'
        setattr(obj,'get_'+self.name,self.make_getter())
        obj.__dict__['set_'+self.name] = types.MethodType(make_setter(self.config, self.name),obj,type(object))
        if isinstance(self.default,(list,dict)):
            obj.__dict__['add_to_'+self.name] = types.MethodType(make_adder(self.config, self.name),obj,type(object))
            
    def __str__(self, *args, **kwargs):
        s=""
        if ConfigValue.verbose:
            s+= "name   : "+str(self.name)+"\n"
        s+= "value  : "+str(self.value)
        if ConfigValue.verbose:
            s+= "\ndefault: "+str(self.default)
            s+= "\nconfig : '{}'".format(str(self.config.path))
        return s
            
################################################################################
def make_setter(config, name):
    def setter(self, v):
        cfg[name].set(v)
#         on_change_call = getattr(self,'on_change_'+name,None)
#         if on_change_call:
#             self.on_change_call()
#         Let's not do setters with side effects.
#         (Principle of least asthonishment)
#         If you need a side effect, wrap the setter.
    cfg = config
    return setter

################################################################################
def make_adder(config, name):
    def adder(self, v):
        if isinstance(cfg[name].value,list):
            cfg[name].value.append(v)
        elif isinstance(cfg[name].value,dict):
            assert isinstance(v,tuple), 'Expecting a tuple: (key,value)'
            assert len(v)==2, 'Expecting a tuple: (key,value)'
            cfg[name].value[v[0]] = v[1]
        else:
            assert False, 'ConfigValue must hold a list or a dictionary.'
    cfg = config
    return adder

################################################################################
### test code
################################################################################
import unittest


class TestConfig(unittest.TestCase):
    class FOO:
        pass
    def setUp(self):
        unittest.TestCase.setUp(self)
        Config.verbose = True
        ConfigValue.verbose = True        
    def testDefaultConfig(self):
        config = Config()
        self.assertEqual(config.values, {})
        config.save(path='./testDefaultConfig.cfg')
        del config
        config = Config(path='./testDefaultConfig.cfg')
        self.assertEqual(config.values, {})        
    
    def testConfigValue(self):
        config = Config(path='./testDefaultConfig.cfg',clear=True)
        self.assertEqual(config.values, {})
        v = ConfigValue(config,'v',default=0)        
        self.assertEqual(v.get(),0)
        self.assertEqual(config['v'].get(),0)
        v.set(1)
        self.assertEqual(v.get(),1)
        self.assertEqual(config['v'].get(),1)

    def testSaveLoad(self):
        config = Config(path='./testDefaultConfig.cfg',clear=True)
        self.assertEqual(config.values, {})
        ConfigValue(config,'v',default=0)
        config.save()
        config = Config(path='./testDefaultConfig.cfg')
        self.assertEqual(config['v'].get(),0)

    def testInject(self):
        config = Config(path='./testDefaultConfig.cfg',clear=True)
        v = ConfigValue(config,'v',default=0)
        foo = TestConfig.FOO()
        v.inject(foo)
        got_v = foo.get_v()
        self.assertEqual(got_v,0)
        foo.set_v(2)
        self.assertEqual(v.get(),2)
        
    def testAdderList(self):
        config = Config(path='./testDefaultConfig.cfg',clear=True)
        v = ConfigValue(config,'v',default=[])
        foo = TestConfig.FOO()
        v.inject(foo)
        got_v = foo.get_v()
        self.assertEqual(got_v,[])
        foo.add_to_v(1)
        foo.add_to_v(2)
        self.assertEqual(v.get(),[1,2])
        
    def testAdderDict(self):
        config = Config(path='./testDefaultConfig.cfg',clear=True)
        v = ConfigValue(config,'v',default={})
        foo = TestConfig.FOO()
        v.inject(foo)
        got_v = foo.get_v()
        self.assertEqual(got_v,{})
        foo.add_to_v((1,'one'))
        foo.add_to_v((2,'two'))
        self.assertEqual(v.get(),{1:'one',2:'two'})
        
if __name__=='__main__':
    unittest.main()