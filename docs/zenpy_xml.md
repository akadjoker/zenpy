# xml

`import xml` — XML parsing and serialisation, on the `ct::Xml` parser from
[akadjoker/containers](https://github.com/akadjoker/containers) (vendored in
`libzen/third_party/ct`). Like `json`, a document is plain script data: no
node class, just dicts and lists.

## Node layout

```python
{"tag": "obj", "attrs": {"id": "1"}, "text": "hello", "children": [...]}
```

Attribute values and text are always strings when parsed. When building a
node in script, attribute values and `text` may be ints, floats or bools;
they are written as text.

## Functions

| Function | Returns | Notes |
|---|---|---|
| `xml.parse(text)` | node | Runtime error with message and offset on malformed input. Entities `&amp; &lt; &gt; &quot; &apos;` and numeric `&#65;`/`&#x41;` are decoded; comments, `<?xml ...?>` and DOCTYPE are skipped. |
| `xml.stringify(node)` | string | Compact. |
| `xml.stringify(node, indent)` | string | `indent` spaces per level (`True` = 2). |

## Example

```python
import xml

doc = xml.parse('<scene name="lvl1"><obj id="1" x="10">hi</obj><obj id="2"/></scene>')
print(doc["attrs"]["name"])                 # lvl1
for obj in doc["children"]:
    print(obj["attrs"]["id"], obj["text"])  # 1 hi / 2

level = {"tag": "level", "attrs": {"id": 7}, "text": "", "children": []}
print(xml.stringify(level, 2))
```
