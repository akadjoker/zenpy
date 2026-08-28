class Animal:
    def __init__(self, name):
        self.name = name
    def speak(self):
        return self.name

class Dog(Animal):
    def speak(self):
        return self.name + " barks"

d = Dog("Rex")
print(d.speak())
