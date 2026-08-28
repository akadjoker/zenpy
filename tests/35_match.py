# Test match statement

# Basic match with integer cases
def describe(n):
    match n:
        case 1:
            return "one"
        case 2:
            return "two"
        case 3:
            return "three"
        default:
            return "other"

assert describe(1) == "one"
assert describe(2) == "two"
assert describe(3) == "three"
assert describe(99) == "other"

# Match with string cases
def greet(lang):
    match lang:
        case "en":
            return "hello"
        case "pt":
            return "ola"
        case "fr":
            return "bonjour"
        default:
            return "hi"

assert greet("en") == "hello"
assert greet("pt") == "ola"
assert greet("fr") == "bonjour"
assert greet("de") == "hi"

# Match with expressions as subject
x = 10
match x + 5:
    case 14:
        result = "wrong"
    case 15:
        result = "correct"
    default:
        result = "miss"

assert result == "correct"

# Match without default (falls through)
match 42:
    case 1:
        found = "one"
    case 42:
        found = "forty-two"

assert found == "forty-two"

# Match with side effects in body
counter = 0
match "go":
    case "stop":
        counter = counter + 10
    case "go":
        counter = counter + 1
    default:
        counter = counter + 100

assert counter == 1

# Match inside a loop
results = []
for i in range(5):
    match i:
        case 0:
            results.push("zero")
        case 1:
            results.push("one")
        case 2:
            results.push("two")
        default:
            results.push("big")

assert results[0] == "zero"
assert results[1] == "one"
assert results[2] == "two"
assert results[3] == "big"
assert results[4] == "big"

# Match with boolean
def check_bool(v):
    match v:
        case True:
            return "yes"
        case False:
            return "no"
        default:
            return "unknown"

assert check_bool(True) == "yes"
assert check_bool(False) == "no"

# Match with negative numbers
def sign(n):
    match n:
        case 0:
            return "zero"
        default:
            if n > 0:
                return "positive"
            return "negative"

assert sign(0) == "zero"
assert sign(5) == "positive"
assert sign(-3) == "negative"

print("All match tests passed.")
