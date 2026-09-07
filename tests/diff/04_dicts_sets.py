d = {"a": 1, "b": 2}
d["c"] = 3
print(d, len(d), d["a"], "a" in d, "z" in d, d.get("z"), d.get("z", 0), d.get("a"))
print(list(d.keys()), list(d.values()), list(d.items()), sorted(d), d.pop("b"), d, d.pop("zz", "dflt"))
d.update({"e": 5, "a": 10}); print(d)
del d["c"]; print(d, d.setdefault("f", 6), d.setdefault("a", 0), d)
print({}, {1: "one", 2.5: "f", True: "t", None: "n", (1, 2): "tuple"}, {1: 1}[True], {1.0: "x"}[1])
e = {k: v for k, v in [("x", 1), ("y", 2)]}; print(e, {n: n * n for n in range(4)})
for k in {"p": 1, "q": 2}: print(k, end=" ")
print()
for k, v in sorted({"p": 1, "q": 2}.items()): print(k, v, end="; ")
print()
print(dict(a=1, b=2), dict([("k", "v")]), dict(zip("ab", [1, 2])), dict({"x": 1}), len({}))
print({"a": 1} == {"a": 1}, {"a": 1} == {"a": 2}, {1: 2} != {}, {"a": [1]}["a"][0])
d2 = {"n": 0}; d2["n"] += 1; d2["n"] *= 5; print(d2, d2.copy(), list(d2.items())[0])
print(str({"k": "v"}), str({1: [1, 2]}), {"s": {"nested": True}}["s"]["nested"])
s = {3, 1, 2, 3}
print(sorted(s), len(s), 1 in s, 5 in s, sorted(s | {5}), sorted(s & {1, 5}), sorted(s - {1}), sorted(s ^ {1, 9}))
s.add(9); s.discard(1); s.discard(42); print(sorted(s), s == {2, 3, 9}, {1} < {1, 2}, {1, 2} <= {1, 2}, set() == set())
print(sorted(set([1, 1, 2])), sorted(set("hello")), sorted({x % 3 for x in range(10)}), len(set()), sorted(set(range(3)) | set(range(2, 5))))
print(set([1, 2]).issubset({1, 2, 3}), {1, 2}.issuperset({1}), {1}.isdisjoint({2}), sorted({1, 2}.union([3], [4])), sorted({1, 2, 3}.intersection([2, 3, 4])))
fs = frozenset([1, 2]); print(sorted(fs), len(fs), 1 in fs)
print(sorted({"b": 1, "a": 2}), sorted({"b": 1, "a": 2}.items()), sorted({"b": 1, "a": 2}.values()))
counts = {}
for ch in "abracadabra":
    counts[ch] = counts.get(ch, 0) + 1
print(sorted(counts.items()), max(counts, key=counts.get), sum(counts.values()))
print(list({"a": 1, "b": 2}.keys())[0], dict.fromkeys("ab", 0), {**{"a": 1}, **{"b": 2}}, {**{"a": 1}, "a": 2})
print(sorted({1, 2, 3}), sorted({1, 2, 3}, reverse=True), 2 in {1: "a", 2: "b"}, {1: "a"}.get(1), {} or "empty", {1: 1} and "full")
