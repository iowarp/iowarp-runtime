"""
This file is not strictly required. You can use it to build
your monitoring functions for that task. Otherwise, if you
delete, please remove the corresponding CMake line that
installs it.

This skeleton does not actually call this file right now.
More detailed examples on how to use Python for monitoring
in Chimaera are in tasks/small_message.
"""
class compressor:
    @staticmethod
    def monitor_io(params, x, y):
        io_size = x[:, 0]
        y_pred = params[0] * io_size
        return y_pred - y
