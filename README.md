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
v for void, only allowed as a return type, and does not accept prefixes
c for char
h for short
i for int
l for long
C for unsigned char
H for unsigned short
I for unsigned int
L for unsigned long
f for float
d for double
s for cstring (ie null terminated char*)

### Pointers
Pointers are specified with a number of `p` flags equal to the level of pointer.
`-pi` = *int
`-ppi` = **int

### Arrays
Array values are specified as unspaced comma delimited values, like `1,2,3,4` or `this,"is an","array of",strings` or `4.3,3.1,9,0`
Type and size will be inferred here as well if unspecified, but specifying types of arrays is probably a good idea.

#### Specifying types and sizes
In the following order, unspaced
`-a`: explicitly flags an array
`<typeflag>`: flag for the type, eg `i` for int or `s` for string
Now you have three choices for size:
* Leave it blank: size is inferred from the number of items you provide
* Size, eg `10`: size is equal to the number you provide
* `t<param_number_with_size>` eg `t3`: size is equal to the number _stored in_ the param denoted by the number, specifically the 1-indexed param position (with 0 denoting the return). This is primarily useful for outpointers to arrays, where you won't know the size until the function returns, and the size will be returned to you in a size_t outparameter. By sizing the array dynamically like this, cliffi will read that parameter for the size before reading and outputting the array.


`1,2,3`         (inferred type)     int[3]       = {1,2,3}
`-ai 1,2,3`     (explicit type)     int[3]       = {1,2,3}
`-ai5 1,2,3`    (statically sized)  int[5]       = {1,2,3,NULL,NULL}
`-pi 0` `-ait1` (dynamically sized) int[ ]* = {NULL}

### Structs
Structs are specified by enclosing the (optional types and) values inside of -S: :S delimiters. Structs can be nested, you can have pointers to them, and they can contain raw arrays.
`-S: 1 2 :S`     struct { int; int; } = {1,2}
`-pS: 1 2 S:`   *struct { int; int; } = {1,2}
`-S: 1 2 -S: test 3.5 :S 6 7 :S` 
            struct { int; int; struct { char*; double; } int; int;}

### Other notes
* Note that arrays inside of structs are raw memory arrays of that size, whereas "arrays" given to functions as arguments directly are really pointer types in disguise, as are cstrings. This is a quirk of C itself, not of cliffi.
* Remember to always include the return type before the function name, and remember that the return type flag there does not take a dash, nor do any inner flags inside of structs used as return types. A struct as return type is all types no values, no dashes, like `S: i ac3 ppi :S`.
* Remember you can always add a type flag in your arguments to solve typing/escaping problems. Eg if you happen to have a parameter that takes a string that is literally `"-ai4"`, or `"..."`, or even just `"3.3"`, you can just prefix it with a `-s` and it will be interpreted as a string. If you need to pass a string with a space in it, just put it in quotes.
* If you mark something as a char array, it will be formatted back to you as a string. If you mark it as a uchar* array, it will be formatted as a hexdump.
* Arrays of structs are not currently supported (as it's just a syntactical nightmare), but you can fake one with a struct of structs if you really want to.
* Just use a 0/1 int for a bool

# License
This is released under the MIT License.

# Contributing
Contributions are welcome! Add a feature! Refactor my messy code! No reasonable offer turned away probably. Feel free to reach out to me first if you want though.

Bug reports are also welcome. Much better if you can include a simple test case. Even better if you add your test case to the test library and open a pull request with your failing test.
