import numpy as np
from scipy.optimize import least_squares

def linear_model(params, x, y):
    y_pred = params[0] * x[:, 0] + params[1]
    return y_pred - y


def least_squares_fit(params):
    data = params[0]
    # data = np.array([np.array(col) for col in data])
    data = np.array(data).T
    print(data.shape)
    x = data[:, :-1]
    y = data[:, -1]
    print(x.shape)
    print(y.shape)
    model_name = str(params[2])
    initial_guess = np.array([0.0] * (x.shape[1] + 1))
    print(initial_guess)
    model = globals().get(model_name, None)
    print(model)
    result = least_squares(model,
                           initial_guess,
                           args=(x, y))
    print(result.x)
    return result.x

