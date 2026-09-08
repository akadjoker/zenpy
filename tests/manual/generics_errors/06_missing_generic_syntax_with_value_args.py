# Regression for the critical bug found in ultrareview: a generic function
# WITH value parameters, called without <...> syntax, used to silently bind
# the first positional value argument into the type-parameter register
# instead of being rejected (only worked "by accident" for a generic
# function with zero value params, where the arity check alone caught it).
def make<T>(x):
    return (T, x)

make(5)
