# Test super()

class Animal:
    def __init__(self, name):
        self.name = name
        self.legs = 4

    def describe(self):
        return self.name + " has " + str(self.legs) + " legs"

class Dog(Animal):
    def __init__(self, name, breed):
        super().__init__(name)
        self.breed = breed

    def describe(self):
        base = super().describe()
        return base + " (" + self.breed + ")"

d = Dog("Rex", "Labrador")
print(d.name)
# expected: Rex

print(d.breed)
# expected: Labrador

print(d.legs)
# expected: 4

print(d.describe())
# expected: Rex has 4 legs (Labrador)

# Multi-level super
class Puppy(Dog):
    def __init__(self, name, breed, toy):
        super().__init__(name, breed)
        self.toy = toy

    def describe(self):
        base = super().describe()
        return base + " plays with " + self.toy

p = Puppy("Buddy", "Golden", "ball")
print(p.describe())
# expected: Buddy has 4 legs (Golden) plays with ball
