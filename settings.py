import copy

################################################################################
class Settings(object):
################################################################################
    defaults = {}

    @classmethod
    def reset(cls, defaults=None):
        if defaults:
            cls.defaults = defaults
        for k,v in cls.defaults.iteritems():
            setattr(cls,k,copy.copy(v))
            
################################################################################
### test code                                                                ###
################################################################################
import unittest
        
class ClassWithSettings(Settings):
    def __init__(self,p0=None,p1=None):
        #change the settings if provided
        if p0:
            ClassWithSettings.p0=p0
        if p1:
            ClassWithSettings.p1=p1     
               
#set the default settings
ClassWithSettings.reset(defaults={'p0':0,'p1':1})

class TestClassWithSettings(unittest.TestCase):
    def test(self):
        c = ClassWithSettings()
        self.assertEqual(ClassWithSettings.p0, ClassWithSettings.defaults['p0'])
        self.assertEqual(ClassWithSettings.p0, 0)
        self.assertEqual(ClassWithSettings.p1, ClassWithSettings.defaults['p1'])
        self.assertEqual(ClassWithSettings.p1, 1)
        ClassWithSettings.p1 = 0
        self.assertEqual(ClassWithSettings.p1, 0)
        self.assertEqual(ClassWithSettings.defaults['p1'], 1)
        ClassWithSettings.reset()
        self.assertEqual(ClassWithSettings.p0, ClassWithSettings.defaults['p0'])
        self.assertEqual(ClassWithSettings.p0, 0)
        self.assertEqual(ClassWithSettings.p1, ClassWithSettings.defaults['p1'])
        self.assertEqual(ClassWithSettings.p1, 1)
        c = ClassWithSettings(1,2)
        self.assertEqual(ClassWithSettings.p0, 1)
        self.assertEqual(ClassWithSettings.p1, 2)
        
if __name__=='__main__':
    unittest.main()