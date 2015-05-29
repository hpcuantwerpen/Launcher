import copy

class Value(object):
    def __init__(self,v=None):
        self.value    = copy.copy(v)
        self.old_value= copy.copy(v)
#     todo: unsafe use is permitted: old_value is not changed when 
#         Value_object.value = new_value
#     is used instead of
#         Value_object.set(new_value)
#     this is a nice idea but it does not work because the assigment in the ctor is overloaded to, hence self.value does not exist 
#     def __setattr__(self, name, value):
#         """ overwrite member assignment of Value objects
#         http://stackoverflow.com/questions/11024646/is-it-possible-to-overload-python-assignment
#         """
#         if name == "value":
#             self.set(value)
#         else:
#             self.__dict__[name] = value
    

    def set(self, v):
        self.old_value = self.value
        self.value = v
        
    def get(self):
        return self.value

    def has_changed(self):
        return self.value!=self.old_value

###  test code
import unittest
class TestValue(unittest.TestCase):
    def test1(self):
        v = Value()
        self.assertEqual(v.get(), None)
        self.assertEqual(v.old_value, None)
        self.assertFalse(v.has_changed())    
    def test2(self):
        Old = [1,'one',[1,2],[1,2]]
        New = [2,'two',[1,3],[1,2,3]]
        for i in range(len(Old)):
            o=Old[i]
            n=New[i]
            v = Value(o)
            self.assertEqual(v.get()    , o)
            self.assertEqual(v.old_value, o)
            self.assertFalse(v.has_changed())
            v.set(n)
            self.assertEqual(v.get(),     n)
            self.assertEqual(v.old_value, o)
            self.assertTrue(v.has_changed())
    
if __name__=='__main__':
    unittest.main()
    v=Value()
    