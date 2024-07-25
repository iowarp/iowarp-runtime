import numpy as np
from scipy.optimize import least_squares
from chimaera_monitor import LeastSquares

def linear_model(params, x, y):
    y_pred = params[0] * x + params[1]
    return y_pred - y


def least_squares_fit(x, y, model, initial_guess):
    result = least_squares(model,
                           initial_guess,
                           args=(x, y))

