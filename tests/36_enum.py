# Test enum declaration and usage

enum Direction { UP, DOWN, LEFT, RIGHT }

# Auto-increment from 0
assert Direction.UP == 0
assert Direction.DOWN == 1
assert Direction.LEFT == 2
assert Direction.RIGHT == 3

# Enum with explicit values
enum HttpStatus {
    OK = 200,
    NOT_FOUND = 404,
    SERVER_ERROR = 500
}

assert HttpStatus.OK == 200
assert HttpStatus.NOT_FOUND == 404
assert HttpStatus.SERVER_ERROR == 500

# Enum with mixed auto/explicit
enum Priority {
    LOW,
    MEDIUM = 5,
    HIGH,
    CRITICAL = 10
}

assert Priority.LOW == 0
assert Priority.MEDIUM == 5
assert Priority.HIGH == 6
assert Priority.CRITICAL == 10

# Enum in comparisons
dir = Direction.LEFT
assert dir == 2
assert dir == Direction.LEFT

# Enum in match
def dir_name(d):
    match d:
        case 0:
            return "up"
        case 1:
            return "down"
        case 2:
            return "left"
        case 3:
            return "right"
        default:
            return "unknown"

assert dir_name(Direction.UP) == "up"
assert dir_name(Direction.RIGHT) == "right"

# Enum in function arguments
def is_success(status):
    return status == HttpStatus.OK

assert is_success(HttpStatus.OK) == True
assert is_success(HttpStatus.NOT_FOUND) == False

# Enum as integer in arithmetic
val = Direction.RIGHT + 1
assert val == 4

# Enum with single member
enum Singleton { ONLY = 42 }
assert Singleton.ONLY == 42

print("All enum tests passed.")
