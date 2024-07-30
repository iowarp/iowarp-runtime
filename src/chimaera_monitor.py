import numpy as np
from scipy.optimize import least_squares

class Example:
    @staticmethod
    def linear_model(params, x, y):
        y_pred = params[0] * x[:, 0] + params[1]
        return y_pred - y


class ChimaeraMonitor:
    @staticmethod
    def least_squares_fit(params):
        data = params[0]
        data = np.array(data).T
        x = data[:, :-1]
        y = data[:, -1]
        model_name = str(params[2])
        initial_guess = np.array([0.0] * (x.shape[1] + 1))
        model = eval(model_name)
        result = least_squares(model,
                               initial_guess,
                               args=(x, y))
        return result.x


def least_squares_fit(params):
    data = params[0]
    data = np.array(data).T
    x = data[:, :-1]
    y = data[:, -1]
    model_name = str(params[2])
    initial_guess = np.array([0.0] * (x.shape[1] + 1))
    model = eval(model_name)
    result = least_squares(model,
                           initial_guess,
                           args=(x, y))
    return result.x
