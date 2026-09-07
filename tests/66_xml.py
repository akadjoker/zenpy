# xml module (ct::Xml): parse to dicts/lists, stringify back.
import xml

doc = xml.parse('<scene name="lvl1" gravity="9.8"><obj id="1" x="10">hello &amp; bye</obj><obj id="2"/><!-- c --><group><obj id="3"/></group></scene>')
assert doc["tag"] == "scene"
assert doc["attrs"]["name"] == "lvl1" and doc["attrs"]["gravity"] == "9.8"
assert len(doc["children"]) == 3
first = doc["children"][0]
assert first["tag"] == "obj" and first["attrs"]["id"] == "1" and first["attrs"]["x"] == "10"
assert first["text"] == "hello & bye"
assert doc["children"][1]["attrs"]["id"] == "2" and doc["children"][1]["children"] == []
assert doc["children"][2]["tag"] == "group" and doc["children"][2]["children"][0]["attrs"]["id"] == "3"

# round trip
text = xml.stringify(doc)
again = xml.parse(text)
assert again["attrs"]["name"] == "lvl1" and len(again["children"]) == 3
assert again["children"][0]["text"] == "hello & bye"
pretty = xml.stringify(doc, 2)
assert "\n" in pretty and xml.parse(pretty)["children"][2]["children"][0]["attrs"]["id"] == "3"

# building a document from script data; numbers and bools become text
node = {"tag": "level", "attrs": {"id": 7, "hard": True, "scale": 1.5}, "text": "", "children": [
    {"tag": "spawn", "attrs": {"x": 1, "y": 2}, "text": "", "children": []}]}
s = xml.stringify(node)
back = xml.parse(s)
assert back["tag"] == "level" and back["attrs"]["id"] == "7" and back["attrs"]["hard"] == "true" and back["attrs"]["scale"] == "1.5"
assert back["children"][0]["attrs"]["y"] == "2"

# entities and a prolog
d2 = xml.parse('<?xml version="1.0"?><a b="&lt;x&gt;">&#65;&#x42;</a>')
assert d2["attrs"]["b"] == "<x>" and d2["text"] == "AB"
print("ok")
