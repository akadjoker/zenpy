# Test inheritance

class Animal:
    def __init__(self, name):
        self.name = name

    def speak(self):
        return "..."

    def describe(self):
        return self.name + " says " + self.speak()

class Dog(Animal):
    def speak(self):
        return "Woof"

class Cat(Animal):
    def speak(self):
        return "Meow"

d = Dog("Rex")
c = Cat("Mimi")

print(d.speak())
# expected: Woof

print(c.speak())
# expected: Meow

print(d.describe())
# expected: Rex says Woof

print(c.describe())
# expected: Mimi says Meow

# Inherited __init__ sets fields
print(d.name)
# expected: Rex

# Child with own __init__ calling parent fields
class Puppy(Dog):
    def __init__(self, name, toy):
        self.name = name
        self.toy = toy

    def play(self):
        return self.name + " plays with " + self.toy

p = Puppy("Buddy", "ball")
print(p.speak())
# expected: Woof (inherited from Dog)

print(p.play())
# expected: Buddy plays with ball

print(p.describe())
# expected: Buddy says Woof
