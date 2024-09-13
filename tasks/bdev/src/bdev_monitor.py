
class Bdev:
    @staticmethod
    def monitor_io(params, x, y):
        io_size = x[:, 0]
        y_pred = io_size / params[0] + params[1]
        return y_pred - y
