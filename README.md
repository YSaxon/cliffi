# cliFFI
cliFFI is a tool for calling shared library functions ad-hoc, right from your shell, without having to write and compile a wrapper script. It is especially meant for reverse engineering, but you might find it helpful for scripting or development purposes too.


## Installation
Binaries for many platforms are provided in Releases. On macOS, you can `brew install ysaxon/cliffi/cliffi`.

Alternatively, you can build it yourself pretty easily. Get libffi installed, clone the repo, and run `./build.sh` or `mkdir build; cd build; cmake ..; make`.

## Usage

Let's start with two simple examples.
```
cliffi libc.so L strlen abcdefg
cliffi libc.so i printf "hello %s, this is example number %d" ... world 2
```

In these two examples we've used automatic type inferrence to infer the types of unflagged arguments (except for return type which must be specified). The `...` marks where the varargs started for printf.

We can also specify types explicitly by putting a flag before them:

```
cliffi libm.so d sqrt -d 2
```

Here we denoted that 2 should be a double by preceeding it with a -d flag. Though we could have also written it as 2.0 or 2d to implicitly mark it as a double

This is only rarely necessary with basic primitive types, but becomes more helpful for pointers, arrays, and structs.


### Primitive Types
- v for void, only allowed as a return type, and does not accept prefixes
- c for char
- h for short
- i for int
- l for long
- C for unsigned char
- H for unsigned short
- I for unsigned int
- L for unsigned long
- f for float
- d for double
- s for cstring (ie null terminated char*)
- P for arbitrary pointer (void*) specified by address

### Pointers
Pointers are specified with a number of `p` flags equal to the level of pointer.
- `-pi` = *int
- `-ppi` = **int

This is not to be confused with the `P` _type_ above.
`p` pointers are specified by their ultimate value, so `-pi 4` represents an `int*` to the value 4.
`P` pointers are specified by their address, so `-P 0xdeadbeef` is a pointer to whatever is at 0xdeadbeef
You can even mix them, so `-pP 0xdeadbeef` would be a pointer to a pointer to whatever is at 0xdeadbeef

`P` pointers are especially useful in REPL mode for containing pointers to types you don't care to specify but will need to reuse

### Arrays
Array values are specified as unspaced comma delimited values, like `1,2,3,4` or `this,"is an","array of",strings` or `4.3,3.1,9,0`

Type and size will be inferred here as well if unspecified, but can be specified explicitly with flags.

#### Specifying types and sizes
In the following order, unspaced
`-a`: explicitly flags an array
`<typeflag>`: flag for the type, eg `i` for int or `s` for string
Now you have three choices for size:
* Leave it blank: size is inferred from the number of items you provide
* Size, eg `10`: size is equal to the number you provide
* `t<param_number_with_size>` eg `t3`: size is equal to the number _stored in_ the param denoted by the number, specifically the 1-indexed param position (with 0 denoting the return). This is primarily useful for outpointers to arrays, where you won't know the size until the function returns, and the size will be returned to you in a size_t outparameter. By sizing the array dynamically like this, cliffi will read that parameter for the size before reading and outputting the array.
* Order matters in the pointer syntax for arrays: a pointer to an array of ints -pai, is different than an array of pointers to ints -api


```
1,2,3         (inferred type)     int[3]       = {1,2,3}
-ai 1,2,3     (explicit type)     int[3]       = {1,2,3}
-ai5 1,2,3    (statically sized)  int[5]       = {1,2,3,NULL,NULL}
-pait2 null -pi 0  (dynamically sized) int**        void return_some_ints(int** numbers, size_t* count)
```

### Structs
Structs are specified by enclosing the (optional types and) values inside of -S[K]: :S delimiters. Structs can be nested, you can have pointers to them, and they can contain raw arrays. The optional K denotes that the struct is pacKed.

Some examples:
```
-S: 1 2 :S     struct { int; int; } = {1,2}
-pS: 1 2 :S   *struct { int; int; } = {1,2}
-S: -pf 1 -ac6 test :S
               struct { float*; char[6]; } = { 1.0, "test\x00\x00"}
-S: 1 2 -S: test 3.5 :S 6 7 :S`
               struct { int; int; struct { char*; double; } int; int;}
-SK: a 1 b :S  struct __attribute__((packed))  {char; int; char;}
```

### Other notes
* Note that arrays inside of structs are raw memory arrays of that size, whereas "arrays" given to functions as arguments directly are really pointer types in disguise, as are cstrings. This is a quirk of C itself, not of cliffi.
* Remember to always include the return type before the function name, and remember that the return type flag there does not take a dash, nor do any inner flags inside of structs used as return types. A struct as return type is all types no values, no dashes, like `S: i ac3 ppi :S`.
* Remember you can always add a type flag in your arguments to solve typing/escaping problems. Eg if you happen to have a parameter that takes a string that is literally `"-ai4"`, or `"..."`, or even just `"3.3"`, you can just prefix it with a `-s` and it will be interpreted as a string. If you need to pass a string with a space in it, just put it in quotes.
* If you mark something as a char array, it will be formatted back to you as a string. If you mark it as a uchar array, it will be formatted as a hexdump.
* Arrays of structs are not currently supported (as it's just a syntactical nightmare), but you can fake one with a struct of structs if you really want to.
* Just use a 0/1 int for a bool
* Don't get the order mixed up for struct start and end tags, it's S: :S. I mess this up sometimes myself, but it's still the best shell-inert delimitter syntax I could come up with.

## REPL mode

You can enter REPL mode by running
```
cliffi --repl
```

REPL mode has a few advantages over running a single command:
* Persistence of state
* Support for variables
* Memory manipulation

### Persistence

REPL mode does not automatically dlclose the library after each command, so global state will be retained.

### Variables

In REPL mode you can set variables and then use them in place of arguments. You can also use them in place of the return type in which case the variable will determine the return type and be filled with the return value when the function returns.
```
cliffi --repl
> set foo 3                    // int foo = 3;
> set bar 7                    // int bar = 7;
> set baz 0                    // int baz = 0;
> testlib.so baz add foo bar   // baz = add(foo, bar);
> print baz
```
At the end of this, baz will equal 10.

You can also use the more familiar `<var> = <value>` syntax to set a variable, or simply type `<var>` to print one.

#### Casting

You can always cast a variable to another type by preceeding it with an explicit type specified different than the one it was originally defined with.

```
cbuffer = -ac h,e,l,l,o," ",w,o,r,l,d,0x00
as_string = -s cbuffer
```
```
intpointer = -pi 5
addr_pointer = -P intpointer
```

### Memory manipulation

In REPL mode, there are `load`, `dump`, and `store` to respectively load a value from a memory address, print a value from a memory address, or store a value to a memory address.

There are also commands to do a `hexdump` from a memory address, and `calculate_offset` to calculate a memory offset (and store the offset to a variable) for a particular library loaded into memory, relative to some reference layout, given a known symbol (ie function) name and reference address. This is helpful for calculating the actual locations of various stripped global and static variables from their addresses in the binary.

For example if in your decompiler, there is a static struct `{ int; short; char*; }` of interest at `0x10beef`, you can find the address of an exported function doFoo() at `0x10dead`, calculate the offset and store it in a variable, and then reference it.
```
calculate_offset myoffset mysharedlib.so doFoo 0x10dead  // calculate the offset
hexdump myoffset+0x10beef 16             // to just see 16 bytes at that address
dump S: i h s :S myoffset+0x10beef       // to dump the struct presently at that address
store myoffset+0x10beef -S: 20 -h 0x6008 hello :S // to store that value into that address
```

Note that basic pointer arithmetic (`+` and `*` with no spaces between operands) is allowed in all commands that accept an address, and allowed for -P types as well, but is not parsed for other types.

### Shell related

You can drop into a shell temporarily (without breaking your cliffi session) with `shell` or by prefixing a command with `!` like `!cat file`

# License
This is released under the MIT License.

# Contributing
Contributions are welcome! Add a feature! Refactor my messy code! No reasonable offer turned away probably. Feel free to reach out to me first if you want though.

Bug reports are also welcome. Much better if you can include a simple test case. Even better if you add your test case to the test library and open a pull request with your failing test.
