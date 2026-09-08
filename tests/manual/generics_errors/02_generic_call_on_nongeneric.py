def plain(x):
    return x

# Bug #2: <...> on a function that isn't generic must be an error.
plain<int>(5)
