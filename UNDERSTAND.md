codebase-reading / thinking framework
- Wisdom first, horsepower later.

Here’s the question set I’d use to “interrogate” any codebase from macro to micro. Apply it to BabyBitcoin, but it generalizes.

**Pass 1: What Is This Thing?**
Ask before reading implementation:

```text
What real-world problem does this project solve?
Who uses it?
What does success look like?
What must never happen?
What is the smallest end-to-end story?
```

For BabyBitcoin:

```text
Can one wallet create value, send it, mine it, and have another wallet verify ownership?
```

**Pass 2: What Are The Main Nouns?**
Look at filenames, classes, structs, docs.

```text
What are the 5-7 nouns that appear everywhere?
Which nouns are real domain concepts?
Which nouns are just technical plumbing?
Which noun seems to own the truth?
Which noun changes over time?
```

In most systems, “the architecture” is the relationship between the nouns.

**Pass 3: Where Is The Truth Stored?**
This is the highest-value question.

```text
What is the canonical state?
What is derived state?
What is cached state?
What is user input?
What is external input?
What can be recomputed from source-of-truth data?
What data must survive restart?
```

For any serious system, find the thing that answers:

```text
If everything crashes, what do we reload to know reality?
```

**Pass 4: What Is The Main Verb?**
Find the action the system is built around.

```text
What is the one function that changes the important state?
What validates before changing state?
What calls it?
What happens before it?
What happens after it?
```

For BabyBitcoin, the main verb is probably:

```text
accept_block()
```

Other projects might have:

```text
create_order()
process_payment()
sync_document()
compile_project()
handle_request()
```

**Pass 5: What Are The Invariants?**
This is where code becomes understandable.

```text
What must always be true?
What must never be accepted?
What assumptions does every module rely on?
What would corrupt the system if violated?
Where are those rules enforced?
Are they enforced once centrally or scattered everywhere?
```

For BabyBitcoin:

```text
A spent coin cannot be spent again.
A transaction cannot create extra money.
A block must prove work.
A signature must match the locked output.
```

**Pass 6: What Is The End-To-End Flow?**
Trace one happy path.

```text
Where does input enter?
How is it parsed?
How is it validated?
How does it change state?
How is it saved?
How is the result exposed to the user/network?
```

Do not trace five flows. Trace one.

**Pass 7: What Are The Boundaries?**
Boundaries reveal architecture.

```text
Where does the outside world touch the system?
Where does disk touch memory?
Where does network data become trusted data?
Where does user intent become a command?
Where does private data cross into public data?
```

In BabyBitcoin:

```text
CLI boundary
P2P boundary
wallet file boundary
chain storage boundary
crypto/signature boundary
```

**Pass 8: What Depends On What?**
Draw arrows.

```text
Which modules are low-level utilities?
Which modules encode domain rules?
Which modules orchestrate flows?
Which modules are adapters to the outside world?
Could this module be tested without the others?
```

Healthy shape often looks like:

```text
interface/CLI/network -> orchestration -> domain rules -> primitives
```

**Pass 9: What Is Accidental Complexity?**
Separate “the idea” from “the machinery.”

```text
Which code exists because of the domain?
Which code exists because computers are annoying?
Which code exists because files/sockets/parsing/logging?
Which code would disappear in pseudocode?
```

This keeps you from mistaking plumbing for architecture.

**Pass 10: Where Would I Change It?**
Architecture understanding becomes real when you can predict edit locations.

Ask:

```text
If I wanted to add feature X, which files change?
If I wanted to fix bug Y, where would I look first?
If I wanted to test invariant Z, where would the test attach?
If I wanted to replace storage/network/crypto, what stays stable?
```

**The Universal Reading Order**
Use this on almost any repo:

```text
1. README / docs
2. Build file
3. Entry point
4. Main domain types
5. Main state owner
6. Main state-changing function
7. One happy path
8. Tests
9. Error paths
10. Historical commits / TODOs
```

**BabyBitcoin Self-Study Prompts**
Use these without me answering them:

```text
What are the 7 nouns of BabyBitcoin?
Which file owns the canonical ledger?
Which data is source of truth: wallet balance, UTXO set, or blocks?
Which function first decides whether a block is acceptable?
Which function actually mutates the ledger?
Where is money created?
Where is money destroyed or prevented from being duplicated?
Where does a private key prove ownership?
Where does network data enter?
At what point does network data become accepted truth?
What survives restart?
What gets recomputed after restart?
What is currently missing for true consensus?
```

**One Rule**
When overwhelmed, ask only this:

> What state is this code protecting, and what bad thing is it preventing?

That question turns arbitrary code into a map.