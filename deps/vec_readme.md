## Dangers of using `vec` from past experience

1. ONE reference to the vector or its contents should be kept at all times.
	- NEVER return a pointer into a `vec`. Always return an index! This is because the pointer can change at any time the vector resizes.
	- NEVER pass a vector into a function that pushes into it!
		- Do this instead: `func(type* vec) { vpush(vec); }` (DANGER OF HOURS OF DEBUGGING) -> `func(type** vec) { vpush(*vec); }\n  func(&vec)`
		- Same reason as the first bullet, except that `vpush` can only change the pointer that the argument contains, not the place the original pointer is kept!
2. HONOR LIFETIMES!!!!!!
3. Be sure a vector is initialized before pushing into it!
