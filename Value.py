import copy

class Transaction(object):
    started = False
    def start_transaction():

class Value(object):
    def __init__(self,v=None):
        self._val = copy.copy(v)
        self._old = copy.copy(v)

    def set(self, v):
        """
        set current value to v
        """
        self._old = self._val
        self._val = v
        
    def get(self):
        """
        get the current value
        """
        return self._val

    def old(self):
        """
        get the previous value
        """
        return self._old

    def has_changed(self):
        """
        test if the value has changed.
        """
        return self._val!=self._old

###  test code
import unittest
class TestValue(unittest.TestCase):
    def test1(self):
        v = Value()
        self.assertEqual(v.get(), None)
        self.assertEqual(v.old(), None)
        self.assertFalse(v.has_changed())    
    def test2(self):
        Old = [1,'one',[1,2],[1,2]]
        New = [2,'two',[1,3],[1,2,3]]
        for i in range(len(Old)):
            o=Old[i]
            n=New[i]
            v = Value(o)
            self.assertEqual(v.get(), o)
            self.assertEqual(v.old(), o)
            self.assertFalse(v.has_changed())
            v.set(n)
            self.assertEqual(v.get(), n)
            self.assertEqual(v.old(), o)
            self.assertTrue(v.has_changed())
    
if __name__=='__main__':
    unittest.main()
    v=Value()
    