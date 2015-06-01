import copy

class NoTransactionStarted(Exception):
    pass

class TransactionManager(object):
    def start_transaction(self):
        self._state0 = copy.deepcopy(self.__dict__)
    def stop_transaction(self):
        del self._state0
    def rollback(self):
        if not self._state0:
            raise NoTransactionStarted
        self.__dict__ = copy.deepcopy(self._state0)
        
class TransactionContext(object):
    def __init__(self, transaction_manager):
        self.transaction_manager = transaction_manager

    def __enter__(self):
        self.transaction_manager.start_transaction()

    def __exit__(self, exc_type, exc_value, traceback):
        if type is not None:
            self.transaction_manager.rollback()
            raise
        self.transaction_manager.stop_transaction()

################################################################################
### test code
################################################################################
import unittest

class MyClass(TransactionManager):
    def __init__(self,v):
        self.value = v
    
class TestTransactionManager(unittest.TestCase):
    def testStartStop(self):
        var = MyClass(0)
        self.assertFalse(hasattr(var, '_state0'))
        var.start_transaction()
        self.assertTrue(hasattr(var, '_state0'))
        var.value += 10
        var.stop_transaction()
        self.assertFalse(hasattr(var, '_state0'))
        self.assertEqual(var.value, 10)
        
    def testRollback(self):
        var = MyClass(0)
        var.start_transaction()
        var.value += 10
        var.rollback()
        self.assertFalse(hasattr(var, '_state0'))
        self.assertEqual(var.value, 0)

class TestTransactionContext(unittest.TestCase):
    def test(self):
        var = MyClass(0)
        try:
            with TransactionContext(var):
                var.value += 10
                var.value += "da ga nie lukke"
            self.assertFalse(hasattr(var, '_state0'))
            self.assertEqual(var.value, 0)
        except:
            pass
                
if __name__=='__main__':
    unittest.main()
    