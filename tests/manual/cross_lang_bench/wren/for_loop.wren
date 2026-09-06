var start = System.clock
var sum = 0
var i = 0
while (i < 5000000) {
  sum = sum + i
  i = i + 1
}
System.print(sum)
System.print("elapsed: %(System.clock - start)")
