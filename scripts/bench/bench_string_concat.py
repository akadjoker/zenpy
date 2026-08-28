# bench_string_concat

#gc_pause()
s = ""
for i in range(100000):
    s += "x"
print("string concat done, len:", len(s))
 

#gc_resume()
