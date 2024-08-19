
class Bdev:
    @staticmethod
    def monitor_io(params, x, y):
        io_size = x[:, 0]
        y_pred = params[0] * io_size + params[1]
        return y_pred - y
