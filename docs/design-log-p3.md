# Project 3 Design Log

**P2 code used:** My own Project 2 `Conversation` and `SentinelScanner`, copied into the four required P3 locations. I did not use the instructor's P2 reference for the build.

**Tools used:** I used ChatGPT to help read the specification, draft parts of the C++ implementation, and suggest test cases. I used CMake, g++, and the provided test programs to compile and check the work. I kept the supplied harness, tokenizer, model clients, and file tools unchanged.

## 1. Hash table

I chose **separate chaining** for `ToolRegistry`. The main table is a vector of buckets, and each bucket has a small vector of `unique_ptr<Tool>` entries. This seemed easier to follow than linear probing because I can look at just one bucket and loop through the names there. If two tool names get the same bucket, they can both stay in that bucket. A lookup is expected to take O(1) time with a good hash and a low load factor, although a bad collision could make it slower.

My hash function is **64-bit FNV-1a**. It starts with an offset value, then XORs and multiplies for every character. I picked this rather than adding up ASCII values because character order should matter. For example, `ab` and `ba` should not give the same hash just because they have the same letters.

The table starts with **16 buckets**. I use a **0.75 load-factor limit**. Before adding a new name, I check whether the next item would go over that limit. If it would, I double the bucket count and put the existing tool pointers into their new buckets. I use the full hash first and only take modulo the current bucket count to get the index.

For the required 10,000 tools, the measurements from my tests were:

| Measurement | FNV-1a with my table | Weak hash (`s[0] + s.size()`) |
|---|---:|---:|
| Bucket count | 16,384 | 16,384 |
| Load factor | 0.6104 | 0.6104 |
| Longest chain | 6 | 9,000 |

This was a big difference. Most of the names start with `tool_`, so the weak hash puts nearly all of them in just a few buckets. FNV-1a spreads them out much better. 
`add()` is **amortized O(1)**. Normally it just inserts into a chain. Resizing is more expensive because it visits every existing tool. However, doubling capacity means there must be many more insertions before the next resize. The total number of entries moved across resizes grows like `1 + 2 + 4 + 8 + ...`, which is less than about twice the final table size. Thus the total work for N additions is O(N) on average, or O(1) amortized per addition.

## 2. Pointer stability

The registry owns the tools using `unique_ptr`, so there is one clear owner. When I resize, I move the `unique_ptr` objects into the new bucket array. I do not create a new `Tool` or copy a tool. Moving a `unique_ptr` only changes which pointer variable owns the same object, so an old `Tool*` from `find()` still points to that object. My resize tests save a pointer, add more tools, and check that it is unchanged. Duplicate names are rejected without replacing the old tool.

## 3. Calculator with two stacks

I used one stack for **numbers** and another for **operators**. In `2 * (3 + 4) - 5`, I push `2` on the value stack and `*` on the operator stack. I then push `(`, `3`, `+`, and `4` into the appropriate stacks. When I reach `)`, I apply `3 + 4`, so the value stack holds `2, 7`, and I remove `(`. When `-` arrives, `*` has higher priority, so I calculate `2 * 7 = 14` first. I push `-`, then push `5`. At the end I subtract and get **9**. For subtraction or division, the first number popped is the right operand. I also use `>=` when checking priority so operators of equal priority run left to right. It is all done with loops, not recursion.

## 4. Errors as values

A missing file or division by zero is something the model might reasonably ask for. It should receive a useful error message and decide what to do next, not crash the harness. That is why `resolve()` and `calc_evaluate()` return `false` with an explanation, and the provided tools turn this into a `ToolResult` failure. Exceptions are more appropriate for unexpected programmer errors than normal tool failures.

## 5. What I would change

If I had more time, I would make a small reusable helper for checking registry bucket statistics and add more randomized collision tests. I would still keep chaining because it was straightforward to debug and passed the required scale check.
