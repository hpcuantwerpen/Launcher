


################################################################################
class Shebang(ScriptLine):
    default = '"#!/bin/bash"'
    def __init__(self,text=None):
        self.text = Shebang.default if not text else text
    
################################################################################
class Comment(object):
    pass

################################################################################
class ScriptLine(object):
    Derived = [Comment]
    @classmethod
    def parse(cls,text):
        for D in cls.Derived:
            parsed = D.parse(text)
            if parsed:
                return parsed
        return cls(text)
    
    def __init__(self,text):
        self.text = text
        
################################################################################
class Script(object):
    def __init__(self,lines=None):
        slines=[]
        if lines:
            self.parse(lines)

################################################################################
### test code
################################################################################
import unittest

if __name__=='__main__':
    unittest.main()