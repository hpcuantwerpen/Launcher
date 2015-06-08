


# ################################################################################
# class Shebang(ScriptLine):
# ################################################################################
#     default = '"#!/bin/bash"'
#     def __init__(self,text=None):
#         self.text = Shebang.default if not text else text
    
################################################################################
class Comment(object):
################################################################################

    @classmethod
    def parse(cls,text):
        
        
    def __init__(self,text):
        self.text = text
################################################################################
class ScriptLine(object):
################################################################################
    derivatives = [Comment]
    @classmethod
    def parse(cls,text):
        for Derived in cls.derivatives:
            parsed Derived.parse(text)
        
        return ScriptLine(text)
    def __init__(self,text):
        self.text = text


################################################################################
def parse_comment(text):
################################################################################
    children = [parse_comment]
    if text.startswith('#'):
        for parse in children:
            parsed = parse(text)
            if parsed:
                return parsed
        
    return False

################################################################################
def parse_line(text):
################################################################################
    children = [parse_comment]
    def __init__(self,text):
        self.parse(text)        
    def parse(self,text):
        self.text = text
        
################################################################################
class NotParsed(Exception):
    pass

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