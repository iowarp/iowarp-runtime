import numpy as np
from scipy.optimize import least_squares


class ChimaeraMonitor:
    @staticmethod
    def least_squares_fit(params, globals):
        data = params[0]
        data = np.array(data).T
        x = data[:, :-1]
        y = data[:, -1]
        initial_guess = np.array(params[1])
        model_name = str(params[2])
        model = eval(model_name, globals)
        result = least_squares(model,
                               initial_guess,
                               args=(x, y))
        return result.x
